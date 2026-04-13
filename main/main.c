#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"

#include "action_engine.h"
#include "audio_input.h"
#include "audio_recorder.h"
#include "boot_button.h"
#include "config_http_server.h"
#include "device_log.h"
#include "input_router.h"
#include "mode_config.h"
#include "mode_controller.h"
#include "air_mouse.h"
#include "mouse_mode.h"
#include "multitouch_poc.h"
#include "sd_card.h"
#include "sd_card_config.h"
#include "ui_status.h"
#include "usb_cdc_log.h"
#include "usb_composite.h"

static const char *TAG = "mode_controller_app";

#define APP_EVENT_QUEUE_LEN 12
#define APP_TASK_STACK_WORDS 4096
#define APP_TASK_PRIORITY 5
#define APP_MAX_NORMALIZED_EVENTS 4
#define APP_SWIPE_TRIGGER_COOLDOWN_MS 300
#define APP_HID_SEND_RETRY_MS 30
#define APP_HID_SEND_RETRY_STEP_MS 2
#define APP_ENABLE_CONFIG_PORTAL 1
/* Delay portal startup until the UI stack is stable on this board. */
#define APP_CONFIG_PORTAL_START_DELAY_MS 1500

typedef enum {
    APP_EVENT_BOOT_BUTTON = 0,
    APP_EVENT_TOUCH_RAW,
    APP_EVENT_RELOAD_CONFIG,
    APP_EVENT_REFRESH_UI,
} app_event_type_t;

typedef struct {
    app_event_type_t type;
    bool pressed;
    ui_status_touch_raw_event_t touch_event;
    uint32_t tick_ms;
    TaskHandle_t reply_task;
    esp_err_t *result_out;
} app_event_t;

typedef struct {
    const char *main_status_text;
    const char *boot_title;
    const char *boot_subtitle;
} app_boot_ui_text_t;

static QueueHandle_t s_app_event_queue = NULL;
static TaskHandle_t s_app_task_handle = NULL;
static const mode_config_t *s_mode_config = NULL;
static mode_controller_t s_mode_controller;
static input_router_t s_input_router;
static action_engine_context_t s_action_context;
static const char *s_hint_text = "";
static bool s_mic_gate_enabled = false;
static bool s_mouse_overlay_active = false;

static void app_deactivate_mouse_overlay(void);
static const char *s_touch_debug_text = NULL;
EXT_RAM_BSS_ATTR static char s_top_status_text[128] = {0};
EXT_RAM_BSS_ATTR static char s_sd_diag_text[48] = {0};
static mode_trigger_t s_last_touch_swipe_trigger = MODE_TRIGGER_PRESS;
static uint32_t s_last_touch_swipe_ms = 0;
static bool s_config_portal_start_pending = false;
static uint32_t s_config_portal_start_deadline_ms = 0;
static bool s_config_portal_running = false;

static void app_reset_active_outputs(void *user_data);
static void app_sync_mouse_mode(void);
static void app_refresh_ui(void *user_data);
static void touch_event_cb(ui_status_touch_raw_event_t event, void *user_data);
static esp_err_t app_request_config_reload(void *user_data);
static esp_err_t app_request_ui_refresh_callback(void *user_data);
static esp_err_t app_request_ui_refresh(void);
static esp_err_t app_post_event(app_event_t event);

static bool app_tick_deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static bool app_is_swipe_trigger(mode_trigger_t trigger)
{
    return (trigger == MODE_TRIGGER_SWIPE_UP) ||
           (trigger == MODE_TRIGGER_SWIPE_DOWN) ||
           (trigger == MODE_TRIGGER_SWIPE_LEFT) ||
           (trigger == MODE_TRIGGER_SWIPE_RIGHT);
}

static bool app_should_suppress_touch_swipe(const mode_binding_event_t *event)
{
    if ((event == NULL) ||
        (event->input != MODE_INPUT_TOUCH) ||
        !app_is_swipe_trigger(event->trigger)) {
        return false;
    }

    uint32_t now_ms = lv_tick_get();
    if ((event->trigger == s_last_touch_swipe_trigger) &&
        !app_tick_deadline_reached(now_ms, s_last_touch_swipe_ms + APP_SWIPE_TRIGGER_COOLDOWN_MS)) {
        ESP_LOGW(TAG, "Suppressing repeated swipe trigger %d", (int)event->trigger);
        return true;
    }

    s_last_touch_swipe_trigger = event->trigger;
    s_last_touch_swipe_ms = now_ms;
    return false;
}

static void app_clear_touch_debug_text(void)
{
    s_touch_debug_text = NULL;
}

static bool app_update_next_deadline(uint32_t candidate_ms, bool *has_deadline, uint32_t *deadline_ms)
{
    if ((has_deadline == NULL) || (deadline_ms == NULL)) {
        return false;
    }

    if (!*has_deadline || ((int32_t)(candidate_ms - *deadline_ms) < 0)) {
        *deadline_ms = candidate_ms;
        *has_deadline = true;
    }

    return true;
}

static void app_start_config_portal_if_due(uint32_t now_ms)
{
    if (!s_config_portal_start_pending ||
        !app_tick_deadline_reached(now_ms, s_config_portal_start_deadline_ms)) {
        return;
    }

    s_config_portal_start_pending = false;
    ESP_LOGI(TAG, "Starting config portal after startup delay");

    esp_err_t config_server_err = config_http_server_start(app_request_config_reload,
                                                           NULL,
                                                           app_request_ui_refresh_callback,
                                                           NULL);
    s_config_portal_running = (config_server_err == ESP_OK);
    if (config_server_err != ESP_OK) {
        ESP_LOGW(TAG, "Config portal start failed: %s", esp_err_to_name(config_server_err));
    }
    (void)app_request_ui_refresh();
}

static TickType_t app_wait_ticks(uint32_t now_ms)
{
    bool has_deadline = false;
    uint32_t deadline_ms = 0;
    uint32_t input_deadline_ms = 0;

    if (input_router_next_deadline_ms(&s_input_router, &input_deadline_ms)) {
        app_update_next_deadline(input_deadline_ms, &has_deadline, &deadline_ms);
    }

    if (s_config_portal_start_pending) {
        app_update_next_deadline(s_config_portal_start_deadline_ms, &has_deadline, &deadline_ms);
    }

    if (!has_deadline) {
        return portMAX_DELAY;
    }

    if (app_tick_deadline_reached(now_ms, deadline_ms)) {
        return 0;
    }

    uint32_t remaining_ms = deadline_ms - now_ms;
    TickType_t wait_ticks = pdMS_TO_TICKS(remaining_ms);
    return (wait_ticks > 0) ? wait_ticks : 1;
}

static void app_set_touch_debug_text(const char *text)
{
    if ((text == NULL) || (text[0] == '\0')) {
        app_clear_touch_debug_text();
        return;
    }

    s_touch_debug_text = text;
}

static const char *app_touch_trigger_debug_text(mode_trigger_t trigger)
{
    switch (trigger) {
    case MODE_TRIGGER_PRESS:
        return "PRESS";
    case MODE_TRIGGER_RELEASE:
        return "RELEASE";
    case MODE_TRIGGER_TAP:
        return "TAP";
    case MODE_TRIGGER_DOUBLE_TAP:
        return "DOUBLE TAP";
    case MODE_TRIGGER_LONG_PRESS:
    case MODE_TRIGGER_HOLD_START:
        return "LONG PRESS";
    case MODE_TRIGGER_HOLD_END:
        return "HOLD END";
    case MODE_TRIGGER_SWIPE_UP:
        return "SWIPE UP";
    case MODE_TRIGGER_SWIPE_DOWN:
        return "SWIPE DOWN";
    case MODE_TRIGGER_SWIPE_LEFT:
        return "SWIPE LEFT";
    case MODE_TRIGGER_SWIPE_RIGHT:
        return "SWIPE RIGHT";
    default:
        return NULL;
    }
}

static const char *app_touch_raw_debug_text(ui_status_touch_raw_event_t event)
{
    switch (event) {
    case UI_STATUS_TOUCH_RAW_PRESSED:
        return "PRESS";
    default:
        return NULL;
    }
}

static const char *app_current_touch_debug_text(void)
{
    if (mode_controller_is_boot_mode_active(&s_mode_controller)) {
        return "";
    }

    return (s_touch_debug_text != NULL) ? s_touch_debug_text : "";
}

static const char *app_normal_status_text(void)
{
    if (s_mic_gate_enabled) {
        return "Dictation Active";
    }

    return ((s_hint_text != NULL) && (s_hint_text[0] != '\0')) ? s_hint_text : "";
}

static const char *app_top_status_text(void)
{
    const char *display_address = config_http_server_display_address();
    snprintf(s_top_status_text,
             sizeof(s_top_status_text),
             "%s\n%s",
             s_sd_diag_text,
             ((display_address != NULL) && (display_address[0] != '\0')) ? display_address : "Unavailable");
    return s_top_status_text;
}

static app_boot_ui_text_t app_resolve_boot_ui_text(bool boot_mode_active)
{
    return (app_boot_ui_text_t){
        .main_status_text = app_normal_status_text(),
        .boot_title = boot_mode_active ? mode_controller_get_active_mode_label(&s_mode_controller) : "",
        .boot_subtitle = "",
    };
}

static esp_err_t app_apply_mode_config(const mode_config_t *config)
{
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (mouse_mode_is_active()) {
        mouse_mode_set_active(false);
        ui_status_set_mouse_mode(false);
    }
    app_reset_active_outputs(NULL);
    s_mode_config = config;
    mode_controller_init(&s_mode_controller, s_mode_config);
    ESP_LOGI(TAG,
             "Applied mode config source=%s modes=%u active=%s",
             mode_config_source_name(mode_config_get_source()),
             (unsigned)s_mode_config->mode_count,
             mode_controller_get_active_mode_label(&s_mode_controller));
    input_router_init(&s_input_router, &s_mode_config->defaults.touch);
    s_hint_text = "";
    s_last_touch_swipe_trigger = MODE_TRIGGER_PRESS;
    s_last_touch_swipe_ms = 0;
    app_clear_touch_debug_text();

    ui_status_init(touch_event_cb, NULL, s_mode_config->defaults.touch.hold_ms);
    ui_status_set_swipe_min_distance(s_mode_config->defaults.touch.swipe_min_distance);
    app_refresh_ui(NULL);
    mouse_mode_set_touch_config(&s_mode_config->defaults.touch_mouse);
    mouse_mode_set_type(s_mode_config->defaults.default_mouse);
    app_sync_mouse_mode();
    return ESP_OK;
}

static esp_err_t app_reload_mode_config(void)
{
    if (!mode_config_reload()) {
        return ESP_FAIL;
    }

    return app_apply_mode_config(mode_config_get());
}

static void app_refresh_ui(void *user_data)
{
    (void)user_data;
    bool boot_mode_active = mode_controller_is_boot_mode_active(&s_mode_controller);
    app_boot_ui_text_t boot_ui = app_resolve_boot_ui_text(boot_mode_active);
    ui_status_view_model_t view_model = {
        .mode_label = mode_controller_get_active_mode_label(&s_mode_controller),
        .top_status_text = app_top_status_text(),
        .status_text = boot_ui.main_status_text,
        .primary_active = s_mic_gate_enabled,
        .boot_overlay_visible = boot_mode_active,
        .boot_title = boot_ui.boot_title,
        .boot_subtitle = boot_ui.boot_subtitle,
        .touch_debug_text = app_current_touch_debug_text(),
    };

    ui_status_render(&view_model);
}

static bool app_send_hid_usage(bool pressed,
                               const mode_hid_usage_t *usage,
                               void *user_data)
{
    (void)user_data;
    if (usage == NULL) {
        return false;
    }

    uint32_t deadline_ms = lv_tick_get() + APP_HID_SEND_RETRY_MS;
    while (1) {
        if (usb_composite_hid_ready()) {
            esp_err_t err = usb_composite_send_usage(pressed, usage);
            if (err == ESP_OK) {
                return true;
            }

            ESP_LOGW(TAG, "HID usage page 0x%02X id 0x%02X modifier 0x%02X %s failed: %s",
                     usage->usage_page,
                     usage->usage_id,
                     usage->modifiers,
                     pressed ? "down" : "up",
                     esp_err_to_name(err));
        } else {
            ESP_LOGW(TAG,
                     "HID usage page 0x%02X id 0x%02X modifier 0x%02X waiting: HID not ready",
                     usage->usage_page,
                     usage->usage_id,
                     usage->modifiers);
        }

        if (app_tick_deadline_reached(lv_tick_get(), deadline_ms)) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(APP_HID_SEND_RETRY_STEP_MS));
    }

    ESP_LOGW(TAG,
             "HID usage page 0x%02X id 0x%02X modifier 0x%02X %s timed out",
             usage->usage_page,
             usage->usage_id,
             usage->modifiers,
             pressed ? "down" : "up");

    if (!pressed) {
        deadline_ms = lv_tick_get() + APP_HID_SEND_RETRY_MS;
        while (1) {
            if (usb_composite_hid_ready()) {
                if (usb_composite_release_all_keys() == ESP_OK) {
                    ESP_LOGW(TAG, "Forced HID release after key-up timeout");
                    return true;
                }
            }

            if (app_tick_deadline_reached(lv_tick_get(), deadline_ms)) {
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(APP_HID_SEND_RETRY_STEP_MS));
        }
    }

    return false;
}

static void app_sleep_ms(uint32_t duration_ms, void *user_data)
{
    (void)user_data;

    if (duration_ms == 0) {
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(duration_ms));
}

static void app_set_mic_gate(bool enabled, int8_t recording_override, void *user_data)
{
    (void)user_data;
    s_mic_gate_enabled = enabled;

    if (!enabled) {
        usb_composite_set_ptt_audio_active(false);
        audio_recorder_stop();
        return;
    }

    bool should_record;
    if (recording_override >= 0) {
        should_record = (recording_override == 1);
    } else {
        const mode_config_t *cfg = mode_config_get();
        should_record = (cfg != NULL) && cfg->recording.enabled;
    }

    if (should_record && sd_card_is_present()) {
        const char *mode_name = mode_controller_get_active_mode_name(&s_mode_controller);
        esp_err_t rec_err = audio_recorder_start(mode_name);
        if (rec_err != ESP_OK) {
            ESP_LOGE(TAG, "audio_recorder_start failed: %s", esp_err_to_name(rec_err));
        }
    }

    usb_composite_set_ptt_audio_active(true);
}

static bool app_get_mic_gate(void *user_data)
{
    (void)user_data;
    return s_mic_gate_enabled;
}

static void app_set_hint_text(const char *text, void *user_data)
{
    (void)user_data;
    s_hint_text = text;
}

static void app_reset_active_outputs(void *user_data)
{
    (void)user_data;

    s_mic_gate_enabled = false;
    audio_recorder_stop();
    usb_composite_set_ptt_audio_active(false);
    input_router_cancel_touch(&s_input_router);
    (void)usb_composite_release_all_keys();
    if (s_mouse_overlay_active) {
        app_deactivate_mouse_overlay();
    } else if (mouse_mode_is_active()) {
        mouse_mode_set_active(false);
        ui_status_set_mouse_mode(false);
    }
}

static void app_sync_mouse_mode(void)
{
    bool should_be_active = (mode_controller_get_active_mode(&s_mode_controller) == MODE_ID_MOUSE);
    if (should_be_active != mouse_mode_is_active()) {
        mouse_mode_set_active(should_be_active);
        ui_status_set_mouse_mode(should_be_active);
    }
}

static void app_deactivate_mouse_overlay(void)
{
    if (!s_mouse_overlay_active) {
        return;
    }
    mouse_mode_force_release();
    mouse_mode_set_active(false);
    ui_status_set_mouse_mode(false);
    s_mouse_overlay_active = false;
    ESP_LOGI(TAG, "mouse overlay deactivated");
}

static bool app_get_mouse_overlay(void *user_data)
{
    (void)user_data;
    return s_mouse_overlay_active;
}

static void app_set_mouse_overlay(bool enabled, mouse_mode_type_t type,
                                  bool immediate_tracking, void *user_data)
{
    (void)user_data;

    if (!enabled) {
        app_deactivate_mouse_overlay();
        return;
    }

    if (s_mouse_overlay_active) {
        return;
    }

    if (type != MOUSE_MODE_TYPE_DEFAULT) {
        mouse_mode_set_type(type);
    }
    mouse_mode_set_active(true);
    ui_status_set_mouse_mode(true);
    s_mouse_overlay_active = true;

    if (immediate_tracking) {
        mouse_mode_start_tracking();
    }
    ESP_LOGI(TAG, "mouse overlay activated (tracking=%d)", immediate_tracking);
}

static void app_dispatch_binding_event(const mode_binding_event_t *event)
{
    if (event == NULL) {
        return;
    }

    if (app_should_suppress_touch_swipe(event)) {
        return;
    }

    if (event->input == MODE_INPUT_BOOT_BUTTON) {
        if (event->trigger == MODE_TRIGGER_RELEASE) {
            input_router_cancel_touch(&s_input_router);
        }
        app_clear_touch_debug_text();
    } else if (event->input == MODE_INPUT_TOUCH) {
        if (!mode_controller_is_boot_mode_active(&s_mode_controller)) {
            const char *touch_debug_text = app_touch_trigger_debug_text(event->trigger);
            if (touch_debug_text != NULL) {
                app_set_touch_debug_text(touch_debug_text);
            }
        }
    }

    const mode_binding_t *bindings[MODE_CONFIG_MAX_MATCHED_BINDINGS] = {0};
    bool suppress_boot_long_press_binding =
        mode_controller_is_boot_mode_active(&s_mode_controller) &&
        (event->input == MODE_INPUT_TOUCH) &&
        (event->trigger == MODE_TRIGGER_LONG_PRESS);
    size_t binding_count = 0;

    if (!suppress_boot_long_press_binding) {
        binding_count = mode_controller_collect_bindings(&s_mode_controller,
                                                         event->input,
                                                         event->trigger,
                                                         bindings,
                                                         MODE_CONFIG_MAX_MATCHED_BINDINGS);
    }

    if (binding_count > MODE_CONFIG_MAX_MATCHED_BINDINGS) {
        ESP_LOGW(TAG, "Binding match truncated from %u to %u",
                 (unsigned)binding_count,
                 (unsigned)MODE_CONFIG_MAX_MATCHED_BINDINGS);
        binding_count = MODE_CONFIG_MAX_MATCHED_BINDINGS;
    }

    /* Log actionable interactions to the device ring buffer for the web dashboard. */
    {
        const char *input_name = (event->input == MODE_INPUT_BOOT_BUTTON) ? "Button"
                               : (event->input == MODE_INPUT_TOUCH)       ? "Touch"
                               : NULL;
        const char *trigger_name = app_touch_trigger_debug_text(event->trigger);
        if (input_name != NULL && trigger_name != NULL &&
            event->trigger != MODE_TRIGGER_PRESS &&
            event->trigger != MODE_TRIGGER_RELEASE) {
            if (binding_count > 0) {
                device_log("INPUT", "%s %s → %u binding(s)", input_name, trigger_name, (unsigned)binding_count);
            } else {
                device_log("INPUT", "%s %s (no bindings)", input_name, trigger_name);
            }
        }
    }

    mode_id_t mode_before = mode_controller_get_active_mode(&s_mode_controller);

    for (size_t i = 0; i < binding_count; ++i) {
        if (!action_engine_execute_actions(bindings[i]->actions, bindings[i]->action_count, &s_action_context)) {
            ESP_LOGW(TAG, "Action execution failed for input=%d trigger=%d",
                     (int)event->input,
                     (int)event->trigger);
            device_log("ERROR", "Action failed: %s %s",
                       (event->input == MODE_INPUT_BOOT_BUTTON) ? "Button" : "Touch",
                       app_touch_trigger_debug_text(event->trigger));
            break;
        }
    }

    /* Detect mode transitions to/from mouse mode and update trackpad state. */
    mode_id_t mode_after = mode_controller_get_active_mode(&s_mode_controller);
    if (mode_before != mode_after) {
        if (s_mouse_overlay_active) {
            app_deactivate_mouse_overlay();
        }
        bool was_mouse = (mode_before == MODE_ID_MOUSE);
        bool is_mouse = (mode_after == MODE_ID_MOUSE);
        if (is_mouse && !was_mouse) {
            mouse_mode_set_active(true);
            ui_status_set_mouse_mode(true);
        } else if (was_mouse && !is_mouse) {
            mouse_mode_set_active(false);
            ui_status_set_mouse_mode(false);
        }
    }

    app_refresh_ui(NULL);
}

static input_router_touch_event_t app_map_touch_event(ui_status_touch_raw_event_t event)
{
    switch (event) {
    case UI_STATUS_TOUCH_RAW_PRESSED:
        return INPUT_ROUTER_TOUCH_EVENT_PRESSED;
    case UI_STATUS_TOUCH_RAW_LONG_PRESSED:
        return INPUT_ROUTER_TOUCH_EVENT_LONG_PRESSED;
    case UI_STATUS_TOUCH_RAW_GESTURE_UP:
        return INPUT_ROUTER_TOUCH_EVENT_GESTURE_UP;
    case UI_STATUS_TOUCH_RAW_GESTURE_DOWN:
        return INPUT_ROUTER_TOUCH_EVENT_GESTURE_DOWN;
    case UI_STATUS_TOUCH_RAW_GESTURE_LEFT:
        return INPUT_ROUTER_TOUCH_EVENT_GESTURE_LEFT;
    case UI_STATUS_TOUCH_RAW_GESTURE_RIGHT:
        return INPUT_ROUTER_TOUCH_EVENT_GESTURE_RIGHT;
    case UI_STATUS_TOUCH_RAW_RELEASED:
    default:
        return INPUT_ROUTER_TOUCH_EVENT_RELEASED;
    }
}

static void app_task(void *arg)
{
    (void)arg;

    app_event_t event = {0};
    mode_binding_event_t normalized_events[APP_MAX_NORMALIZED_EVENTS] = {0};

    while (1) {
        uint32_t now_ms = lv_tick_get();
        app_start_config_portal_if_due(now_ms);
        size_t timeout_event_count = input_router_flush_timeouts(&s_input_router,
                                                                 now_ms,
                                                                 normalized_events,
                                                                 APP_MAX_NORMALIZED_EVENTS);
        for (size_t i = 0; i < timeout_event_count; ++i) {
            app_dispatch_binding_event(&normalized_events[i]);
        }

        TickType_t wait_ticks = app_wait_ticks(now_ms);
        if (xQueueReceive(s_app_event_queue, &event, wait_ticks) != pdTRUE) {
            continue;
        }

        size_t event_count = 0;
        switch (event.type) {
        case APP_EVENT_BOOT_BUTTON:
            event_count = input_router_handle_button(&s_input_router,
                                                     event.pressed,
                                                     normalized_events,
                                                     APP_MAX_NORMALIZED_EVENTS);
            break;

        case APP_EVENT_TOUCH_RAW:
            if (mouse_mode_is_active()) {
                if (s_mouse_overlay_active &&
                    (event.touch_event == UI_STATUS_TOUCH_RAW_RELEASED)) {
                    event_count = input_router_handle_touch(&s_input_router,
                                                            app_map_touch_event(event.touch_event),
                                                            event.tick_ms,
                                                            normalized_events,
                                                            APP_MAX_NORMALIZED_EVENTS);
                    break;
                }
                break;
            }
            if (!mode_controller_is_boot_mode_active(&s_mode_controller)) {
                const char *touch_debug_text = app_touch_raw_debug_text(event.touch_event);
                if (touch_debug_text != NULL) {
                    app_set_touch_debug_text(touch_debug_text);
                }
            }
            event_count = input_router_handle_touch(&s_input_router,
                                                    app_map_touch_event(event.touch_event),
                                                    event.tick_ms,
                                                    normalized_events,
                                                    APP_MAX_NORMALIZED_EVENTS);
            break;

        case APP_EVENT_RELOAD_CONFIG: {
            esp_err_t reload_result = app_reload_mode_config();
            if (event.result_out != NULL) {
                *event.result_out = reload_result;
            }
            if (event.reply_task != NULL) {
                xTaskNotifyGive(event.reply_task);
            }
            continue;
        }

        case APP_EVENT_REFRESH_UI:
            app_refresh_ui(NULL);
            continue;

        default:
            break;
        }

        if (event_count > APP_MAX_NORMALIZED_EVENTS) {
            event_count = APP_MAX_NORMALIZED_EVENTS;
        }

        for (size_t i = 0; i < event_count; ++i) {
            app_dispatch_binding_event(&normalized_events[i]);
        }
    }
}

static esp_err_t app_post_event(app_event_t event)
{
    if (s_app_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xQueueSend(s_app_event_queue, &event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Dropping app event %d", (int)event.type);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static esp_err_t app_event_loop_init(void)
{
    if (s_app_event_queue != NULL) {
        return ESP_OK;
    }

    s_app_event_queue = xQueueCreate(APP_EVENT_QUEUE_LEN, sizeof(app_event_t));
    if (s_app_event_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_created = xTaskCreate(
        app_task,
        "app_ctrl",
        APP_TASK_STACK_WORDS,
        NULL,
        APP_TASK_PRIORITY,
        &s_app_task_handle
    );
    if (task_created != pdPASS) {
        s_app_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t app_request_config_reload(void *user_data)
{
    (void)user_data;

    if (s_app_event_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t reload_result = ESP_FAIL;
    app_event_t reload_event = {
        .type = APP_EVENT_RELOAD_CONFIG,
        .reply_task = xTaskGetCurrentTaskHandle(),
        .result_out = &reload_result,
    };
    if (xQueueSend(s_app_event_queue, &reload_event, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(5000)) == 0) {
        return ESP_ERR_TIMEOUT;
    }

    return reload_result;
}

static esp_err_t app_request_ui_refresh_callback(void *user_data)
{
    (void)user_data;
    return app_request_ui_refresh();
}

static esp_err_t app_request_ui_refresh(void)
{
    return app_post_event((app_event_t){
        .type = APP_EVENT_REFRESH_UI,
    });
}

static void boot_button_event_cb(bool pressed, void *user_data)
{
    (void)user_data;

    (void)app_post_event((app_event_t){
        .type = APP_EVENT_BOOT_BUTTON,
        .pressed = pressed,
    });
}

static void touch_event_cb(ui_status_touch_raw_event_t event, void *user_data)
{
    (void)user_data;

    (void)app_post_event((app_event_t){
        .type = APP_EVENT_TOUCH_RAW,
        .touch_event = event,
        .tick_ms = lv_tick_get(),
    });
}

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_display_start() ? ESP_OK : ESP_FAIL);

    esp_err_t sd_err = sd_card_init();
    if (sd_err == ESP_OK) {
        sd_card_config_populate();
        sdmmc_card_t *card = sd_card_get_card();
        if (card != NULL) {
            uint64_t size_mb = ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024 * 1024);
            snprintf(s_sd_diag_text, sizeof(s_sd_diag_text), "SD: %.6s %lluMB",
                     card->cid.name, (unsigned long long)size_mb);
        } else {
            snprintf(s_sd_diag_text, sizeof(s_sd_diag_text), "SD: OK");
        }
    } else {
        snprintf(s_sd_diag_text, sizeof(s_sd_diag_text), "SD: %s",
                 esp_err_to_name(sd_err));
    }

    device_log_init();
    device_log("BOOT", "WalKEY-TalKEY starting");
    device_log("BOOT", "SD card: %s", s_sd_diag_text);

    audio_recorder_init();

    ESP_ERROR_CHECK(mode_config_init() ? ESP_OK : ESP_FAIL);
    s_mode_config = mode_config_get();
    device_log("BOOT", "Config loaded (%zu modes)", s_mode_config->mode_count);

    mode_controller_init(&s_mode_controller, s_mode_config);
    input_router_init(&s_input_router, &s_mode_config->defaults.touch);
    s_action_context = (action_engine_context_t){
        .controller = &s_mode_controller,
        .send_hid_usage = app_send_hid_usage,
        .sleep_ms = app_sleep_ms,
        .get_mic_gate = app_get_mic_gate,
        .set_mic_gate = app_set_mic_gate,
        .set_hint_text = app_set_hint_text,
        .reset_active_outputs = app_reset_active_outputs,
        .refresh_ui = app_refresh_ui,
        .get_mouse_overlay = app_get_mouse_overlay,
        .set_mouse_overlay = app_set_mouse_overlay,
        .user_data = NULL,
    };

    ESP_ERROR_CHECK(app_event_loop_init());
    ui_status_init(touch_event_cb, NULL, s_mode_config->defaults.touch.hold_ms);
    ui_status_set_swipe_min_distance(s_mode_config->defaults.touch.swipe_min_distance);
    mouse_mode_init();
    air_mouse_init(&s_mode_config->defaults.air_mouse);
    mouse_mode_set_touch_config(&s_mode_config->defaults.touch_mouse);
    mouse_mode_set_type(s_mode_config->defaults.default_mouse);
    multitouch_poc_init();
    app_refresh_ui(NULL);
    app_sync_mouse_mode();

    ESP_ERROR_CHECK(audio_input_init());
    ESP_ERROR_CHECK(usb_composite_init());
    usb_cdc_log_init();
    usb_composite_set_ptt_audio_active(false);
    ESP_ERROR_CHECK(boot_button_init(boot_button_event_cb, NULL));
    device_log("BOOT", "USB composite + audio input ready");

    if (APP_ENABLE_CONFIG_PORTAL) {
        s_config_portal_start_pending = true;
        s_config_portal_start_deadline_ms = lv_tick_get() + APP_CONFIG_PORTAL_START_DELAY_MS;
        (void)app_request_ui_refresh();
        device_log("BOOT", "Config portal startup scheduled");
    } else {
        ESP_LOGW(TAG, "Config portal startup disabled");
        device_log("WARN", "Config portal startup disabled");
    }

    app_refresh_ui(NULL);
    device_log("BOOT", "Boot complete");
}