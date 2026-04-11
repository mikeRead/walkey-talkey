#include "mouse_mode.h"

#include <stdlib.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

#include "air_mouse.h"
#include "multitouch_poc.h"
#include "ui_status.h"
#include "usb_composite.h"

#define MOUSE_MODE_CLICK_RELEASE_MS 20
#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

static const char *TAG = "mouse_mode";

static touch_mouse_config_t s_touch_cfg = {
    .sensitivity = 1.0f,
    .move_threshold_px = 10,
    .tap_drag_window_ms = 200,
};

static mouse_mode_type_t s_type = MOUSE_MODE_TYPE_TOUCH;
static bool s_active = false;
static lv_point_t s_last_point = {0};
static int32_t s_move_accum_x = 0;
static int32_t s_move_accum_y = 0;
static bool s_moved_significantly = false;
static bool s_drag_active = false;
static bool s_right_click_sent = false;
static bool s_multitouch_seen = false;
static bool s_long_press_cursor = false;
static bool s_air_pressed = false;
static uint32_t s_last_tap_release_ms = 0;
static bool s_overlay_tracking_only = false;

static lv_timer_t *s_click_timer = NULL;

static void mouse_mode_reset_state(void)
{
    s_last_point = (lv_point_t){0};
    s_move_accum_x = 0;
    s_move_accum_y = 0;
    s_moved_significantly = false;
    s_drag_active = false;
    s_right_click_sent = false;
    s_multitouch_seen = false;
    s_long_press_cursor = false;
    s_air_pressed = false;
    s_last_tap_release_ms = 0;
    s_overlay_tracking_only = false;
}

static void mouse_mode_click_release_cb(lv_timer_t *timer)
{
    (void)timer;
    usb_composite_send_mouse_report(0, 0, 0, 0);
    if (s_type == MOUSE_MODE_TYPE_AIR) {
        air_mouse_set_buttons(0);
    }
    lv_timer_pause(timer);
}

void mouse_mode_init(void)
{
    if (s_click_timer != NULL) {
        return;
    }

    mouse_mode_reset_state();

    bsp_display_lock(-1);
    s_click_timer = lv_timer_create(mouse_mode_click_release_cb, MOUSE_MODE_CLICK_RELEASE_MS, NULL);
    if (s_click_timer != NULL) {
        lv_timer_pause(s_click_timer);
    }
    bsp_display_unlock();

    ESP_LOGI(TAG, "mouse mode initialised");
}

void mouse_mode_set_touch_config(const touch_mouse_config_t *cfg)
{
    if (cfg != NULL) {
        s_touch_cfg = *cfg;
    }
}

void mouse_mode_set_type(mouse_mode_type_t type)
{
    if (s_type == type) {
        return;
    }

    bool was_active = s_active;
    if (was_active) {
        mouse_mode_set_active(false);
    }

    s_type = type;
    ESP_LOGI(TAG, "mouse backend set to %s", (type == MOUSE_MODE_TYPE_AIR) ? "air" : "touch");

    if (was_active) {
        mouse_mode_set_active(true);
    }
}

mouse_mode_type_t mouse_mode_get_type(void)
{
    return s_type;
}

void mouse_mode_set_active(bool active)
{
    if (s_active == active) {
        return;
    }

    s_active = active;

    if (!active) {
        usb_composite_send_mouse_report(0, 0, 0, 0);
        if (s_type == MOUSE_MODE_TYPE_AIR) {
            air_mouse_set_active(false);
        }
    } else {
        if (s_type == MOUSE_MODE_TYPE_AIR) {
            air_mouse_set_active(true);
        }
    }
    mouse_mode_reset_state();

    ESP_LOGI(TAG, "mouse mode %s (%s)",
             active ? "active" : "inactive",
             (s_type == MOUSE_MODE_TYPE_AIR) ? "air" : "touch");
}

bool mouse_mode_is_active(void)
{
    return s_active;
}

void mouse_mode_start_tracking(void)
{
    if (!s_active) {
        return;
    }

    s_overlay_tracking_only = true;

    if (s_type == MOUSE_MODE_TYPE_AIR) {
        s_air_pressed = true;
        s_long_press_cursor = true;
        air_mouse_set_tracking(true);
        ESP_LOGI(TAG, "overlay: immediate air tracking started");
    } else {
        s_moved_significantly = true;
        ESP_LOGI(TAG, "overlay: immediate touch tracking started");
    }
}

void mouse_mode_force_release(void)
{
    if (!s_active) {
        return;
    }

    if (s_type == MOUSE_MODE_TYPE_AIR) {
        if (air_mouse_is_tracking()) {
            air_mouse_set_tracking(false);
            air_mouse_set_buttons(0);
        }
    }
    usb_composite_send_mouse_report(0, 0, 0, 0);
    mouse_mode_reset_state();
    ESP_LOGI(TAG, "overlay: force release");
}

/* ========================================================================== */
/* Touch-trackpad backend (original behavior)                                 */
/* ========================================================================== */

static void touch_handle_press(int32_t x, int32_t y)
{
    bool tap_drag = (s_last_tap_release_ms > 0) &&
                    ((lv_tick_get() - s_last_tap_release_ms) < s_touch_cfg.tap_drag_window_ms);

    s_last_point.x = x;
    s_last_point.y = y;
    s_move_accum_x = 0;
    s_move_accum_y = 0;
    s_moved_significantly = false;
    s_drag_active = false;
    s_last_tap_release_ms = 0;

    if (tap_drag) {
        s_drag_active = true;
        esp_err_t err = usb_composite_send_mouse_report(MOUSE_BUTTON_LEFT, 0, 0, 0);
        ESP_LOGI(TAG, "tap-drag start at (%d,%d) err=%s", (int)x, (int)y, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "press at (%d,%d)", (int)x, (int)y);
    }

    if (multitouch_poc_get_touch_count() >= 2) {
        s_multitouch_seen = true;
    }
    ui_status_set_multitouch_indicator(s_multitouch_seen);
}

static void touch_handle_move(int32_t x, int32_t y)
{
    int32_t touch_dx = x - s_last_point.x;
    int32_t touch_dy = y - s_last_point.y;
    s_last_point.x = x;
    s_last_point.y = y;

    /* Compensate for 90-degree CW physical screen rotation. */
    int32_t raw_dx = -touch_dy;
    int32_t raw_dy = touch_dx;

    s_move_accum_x += touch_dx;
    s_move_accum_y += touch_dy;
    if ((abs(s_move_accum_x) + abs(s_move_accum_y)) >= (int)s_touch_cfg.move_threshold_px) {
        s_moved_significantly = true;
    }

    if (multitouch_poc_get_touch_count() >= 2) {
        s_multitouch_seen = true;
    }
    ui_status_set_multitouch_indicator(s_multitouch_seen);

    float scaled_dx = (float)raw_dx * s_touch_cfg.sensitivity;
    float scaled_dy = (float)raw_dy * s_touch_cfg.sensitivity;
    raw_dx = (int32_t)scaled_dx;
    raw_dy = (int32_t)scaled_dy;

    if ((raw_dx == 0) && (raw_dy == 0)) {
        return;
    }

    int8_t dx = (raw_dx > 127) ? 127 : (raw_dx < -128) ? -128 : (int8_t)raw_dx;
    int8_t dy = (raw_dy > 127) ? 127 : (raw_dy < -128) ? -128 : (int8_t)raw_dy;
    uint8_t buttons = s_drag_active ? MOUSE_BUTTON_LEFT : 0;
    esp_err_t err = usb_composite_send_mouse_report(buttons, dx, dy, 0);
    ESP_LOGI(TAG, "move dx=%d dy=%d drag=%d err=%s", (int)dx, (int)dy, s_drag_active, esp_err_to_name(err));
}

static void touch_handle_long_press(void)
{
    if (s_moved_significantly || s_drag_active) {
        return;
    }

    s_right_click_sent = true;
    uint8_t button = s_multitouch_seen ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_RIGHT;
    esp_err_t err = usb_composite_send_mouse_report(button, 0, 0, 0);
    if (s_click_timer != NULL) {
        lv_timer_reset(s_click_timer);
        lv_timer_resume(s_click_timer);
    }
    ESP_LOGI(TAG, "%s click err=%s",
             s_multitouch_seen ? "middle" : "right", esp_err_to_name(err));
}

static void touch_handle_release(void)
{
    if (s_overlay_tracking_only) {
        ESP_LOGI(TAG, "touch: overlay tracking release (no click)");
    } else if (s_drag_active) {
        esp_err_t err = usb_composite_send_mouse_report(0, 0, 0, 0);
        ESP_LOGI(TAG, "drag end (left up) err=%s", esp_err_to_name(err));
    } else if (!s_moved_significantly && !s_right_click_sent) {
        esp_err_t err = usb_composite_send_mouse_report(MOUSE_BUTTON_LEFT, 0, 0, 0);
        if (s_click_timer != NULL) {
            lv_timer_reset(s_click_timer);
            lv_timer_resume(s_click_timer);
        }
        s_last_tap_release_ms = lv_tick_get();
        ESP_LOGI(TAG, "left click err=%s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "drag end");
    }

    s_move_accum_x = 0;
    s_move_accum_y = 0;
    s_moved_significantly = false;
    s_drag_active = false;
    s_right_click_sent = false;
    s_multitouch_seen = false;

    ui_status_set_multitouch_indicator(false);
}

/* ========================================================================== */
/* Air-mouse backend                                                          */
/*                                                                            */
/*   Long press        -> move cursor via gyro (no click)                     */
/*   Tap               -> left click (no cursor movement)                     */
/*   Double tap        -> double left click (two taps, OS sees double-click)  */
/*   Tap and hold      -> mouse1 down + gyro cursor (drag-and-drop)           */
/*   Multi-touch       -> right click (mouse 2)                               */
/* ========================================================================== */

static void air_handle_press(int32_t x, int32_t y)
{
    (void)x;
    (void)y;

    if (air_mouse_is_tracking()) {
        air_mouse_set_tracking(false);
        air_mouse_set_buttons(0);
    }

    bool tap_drag = (s_last_tap_release_ms > 0) &&
                    ((lv_tick_get() - s_last_tap_release_ms) < s_touch_cfg.tap_drag_window_ms);

    s_air_pressed = true;
    s_drag_active = false;
    s_right_click_sent = false;
    s_long_press_cursor = false;
    s_last_tap_release_ms = 0;

    if (multitouch_poc_get_touch_count() >= 2) {
        s_multitouch_seen = true;
    }
    ui_status_set_multitouch_indicator(s_multitouch_seen);

    if (tap_drag) {
        s_drag_active = true;
        air_mouse_set_buttons(MOUSE_BUTTON_LEFT);
        air_mouse_set_tracking(true);
        ESP_LOGI(TAG, "air: tap-drag start (mouse1 down + gyro on)");
    } else {
        ESP_LOGI(TAG, "air: finger down (waiting for long-press or release)");
    }
}

static void air_handle_move(int32_t x, int32_t y)
{
    (void)x;
    (void)y;

    if (multitouch_poc_get_touch_count() >= 2) {
        s_multitouch_seen = true;
    }
    ui_status_set_multitouch_indicator(s_multitouch_seen);
}

static void air_handle_long_press(void)
{
    if (!s_air_pressed || s_drag_active) {
        return;
    }

    if (s_multitouch_seen) {
        s_right_click_sent = true;
        esp_err_t err = usb_composite_send_mouse_report(MOUSE_BUTTON_RIGHT, 0, 0, 0);
        if (s_click_timer != NULL) {
            lv_timer_reset(s_click_timer);
            lv_timer_resume(s_click_timer);
        }
        ESP_LOGI(TAG, "air: right click (multitouch) err=%s", esp_err_to_name(err));
        return;
    }

    s_long_press_cursor = true;
    air_mouse_set_tracking(true);
    ESP_LOGI(TAG, "air: long press -> gyro cursor on");
}

static void air_handle_release(void)
{
    if (s_overlay_tracking_only) {
        air_mouse_set_tracking(false);
        air_mouse_set_buttons(0);
        ESP_LOGI(TAG, "air: overlay tracking release (no click)");
    } else if (s_drag_active) {
        air_mouse_set_tracking(false);
        air_mouse_set_buttons(0);
        esp_err_t err = usb_composite_send_mouse_report(0, 0, 0, 0);
        ESP_LOGI(TAG, "air: drag end (mouse1 up) err=%s", esp_err_to_name(err));
    } else if (s_long_press_cursor) {
        air_mouse_set_tracking(false);
        ESP_LOGI(TAG, "air: long press release -> gyro cursor off");
    } else if (!s_right_click_sent) {
        esp_err_t err = usb_composite_send_mouse_report(MOUSE_BUTTON_LEFT, 0, 0, 0);
        if (s_click_timer != NULL) {
            lv_timer_reset(s_click_timer);
            lv_timer_resume(s_click_timer);
        }
        s_last_tap_release_ms = lv_tick_get();
        ESP_LOGI(TAG, "air: left click err=%s", esp_err_to_name(err));
    }

    if (air_mouse_is_tracking()) {
        air_mouse_set_tracking(false);
        air_mouse_set_buttons(0);
        ESP_LOGW(TAG, "air: safety-net tracking stop on release");
    }

    s_air_pressed = false;
    s_drag_active = false;
    s_right_click_sent = false;
    s_long_press_cursor = false;
    s_multitouch_seen = false;

    ui_status_set_multitouch_indicator(false);
}

/* ========================================================================== */
/* Public dispatch                                                            */
/* ========================================================================== */

void mouse_mode_handle_press(int32_t x, int32_t y)
{
    if (!s_active) {
        return;
    }
    if (s_type == MOUSE_MODE_TYPE_AIR) {
        air_handle_press(x, y);
    } else {
        touch_handle_press(x, y);
    }
}

void mouse_mode_handle_move(int32_t x, int32_t y)
{
    if (!s_active) {
        return;
    }
    if (s_type == MOUSE_MODE_TYPE_AIR) {
        air_handle_move(x, y);
    } else {
        touch_handle_move(x, y);
    }
}

void mouse_mode_handle_long_press(void)
{
    if (!s_active) {
        return;
    }
    if (s_type == MOUSE_MODE_TYPE_AIR) {
        air_handle_long_press();
    } else {
        touch_handle_long_press();
    }
}

void mouse_mode_handle_release(void)
{
    if (!s_active) {
        return;
    }
    if (s_type == MOUSE_MODE_TYPE_AIR) {
        air_handle_release();
    } else {
        touch_handle_release();
    }
}
