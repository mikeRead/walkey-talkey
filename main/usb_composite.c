#include "usb_composite.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "class/hid/hid.h"
#include "class/hid/hid_device.h"
#include "class/cdc/cdc_device.h"
#include "class/msc/msc.h"
#include "class/msc/msc_device.h"
#include "esp_check.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"

#include "audio_input.h"
#include "audio_recorder.h"
#include "sd_card.h"

#define HID_REPORT_ID_KEYBOARD 1
#define HID_REPORT_ID_CONSUMER 2
#define HID_REPORT_ID_SYSTEM 3
#define HID_REPORT_ID_MOUSE 4
#define USB_AUDIO_EP_IN_ADDR 0x81
#define USB_HID_EP_IN_ADDR 0x82
#define HID_EP_SIZE 16
#define HID_POLL_INTERVAL_MS 10
#define USB_CONFIG_POWER_MA 100
#define USB_VENDOR_ID 0x303A
#define USB_PRODUCT_ID 0x4214
#define USB_BCD_DEVICE 0x0205
#define USB_AUDIO_TASK_STACK_WORDS 4096
#define USB_AUDIO_TASK_PRIORITY 6
#define USB_STRING_INDEX_AUDIO 4
#define USB_STRING_INDEX_HID 5
#define USB_STRING_INDEX_MSC 6
#define USB_MSC_EP_OUT_ADDR 0x03
#define USB_MSC_EP_IN_ADDR 0x83
#define USB_MSC_EP_SIZE 64
#define USB_STRING_INDEX_CDC 7
#define USB_CDC_DATA_EP_OUT_ADDR 0x04
#define USB_CDC_DATA_EP_IN_ADDR 0x84
#define USB_CDC_DATA_EP_SIZE 64

// CDC descriptor WITHOUT notification endpoint (saves 1 IN endpoint to stay
// within the ESP32-S3 DWC2 limit of 5 total IN FIFOs including EP0).
#define TUD_CDC_NONOTIF_DESC_LEN (8 + 9 + 5 + 5 + 4 + 5 + 9 + 7 + 7)
#define TUD_CDC_NONOTIF_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize) \
  8, TUSB_DESC_INTERFACE_ASSOCIATION, _itfnum, 2, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, 0, \
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_NONE, _stridx, \
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, U16_TO_U8S_LE(0x0120), \
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_CALL_MANAGEMENT, 0, (uint8_t)((_itfnum) + 1), \
  4, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 2, \
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, _itfnum, (uint8_t)((_itfnum) + 1), \
  9, TUSB_DESC_INTERFACE, (uint8_t)((_itfnum)+1), 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0, \
  7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0, \
  7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_BULK, U16_TO_U8S_LE(_epsize), 0

#define USB_VOLUME_MIN_DB (-50)
#define USB_VOLUME_MAX_DB 0
#define USB_VOLUME_RES_DB 1
#define USB_DEFAULT_VOLUME_DB 0
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_AUDIO * TUD_AUDIO_MIC_ONE_CH_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN + CFG_TUD_MSC * TUD_MSC_DESC_LEN + CFG_TUD_CDC * TUD_CDC_NONOTIF_DESC_LEN)

static const char *TAG = "usb_composite";

enum {
    ITF_NUM_AUDIO_CONTROL = 0,
    ITF_NUM_AUDIO_STREAMING,
    ITF_NUM_HID,
    ITF_NUM_MSC,
    ITF_NUM_CDC_COMM,
    ITF_NUM_CDC_DATA,
    ITF_NUM_TOTAL
};

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_AUDIO,
    STRID_HID,
    STRID_MSC,
    STRID_CDC,
};

enum {
    AUDIO_ENTITY_INPUT_TERMINAL = 1,
    AUDIO_ENTITY_FEATURE_UNIT = 2,
    AUDIO_ENTITY_CLOCK_SOURCE = 4,
};

static bool s_suspended = false;
static bool s_remote_wakeup_allowed = false;
static bool s_initialized = false;
static volatile bool s_ptt_audio_active = false;
static volatile bool s_audio_capture_suspended = false;
static volatile bool s_audio_stream_open = false;
static TaskHandle_t s_audio_task_handle = NULL;
static uint8_t s_active_keycodes[6] = {0};
static uint8_t s_active_modifiers = 0;
static uint16_t s_active_consumer_usage = 0;
static uint8_t s_active_system_usage = 0;

static uint8_t s_mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1] = {0};
static int16_t s_volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1] = {USB_DEFAULT_VOLUME_DB};
static audio_control_range_2_n_t(1) s_volume_range[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1];
static uint32_t s_sample_freq = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
static uint8_t s_clock_valid = 1;
static audio_control_range_4_n_t(1) s_sample_freq_range;

static const uint8_t s_hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(HID_REPORT_ID_CONSUMER)),
    TUD_HID_REPORT_DESC_SYSTEM_CONTROL(HID_REPORT_ID(HID_REPORT_ID_SYSTEM)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_REPORT_ID_MOUSE)),
};

static const tusb_desc_device_t s_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VENDOR_ID,
    .idProduct = USB_PRODUCT_ID,
    .bcdDevice = USB_BCD_DEVICE,
    .iManufacturer = STRID_MANUFACTURER,
    .iProduct = STRID_PRODUCT,
    .iSerialNumber = STRID_SERIAL,
    .bNumConfigurations = 0x01,
};

static const char *s_string_descriptor[] = {
    (char[]){0x09, 0x04},
    "Waveshare",
    "WalKEY-TalKEY USB",
    "WALKEY-TALKEY-S3",
    "WalKEY-TalKEY",
    "WalKEY-TalKEY Keyboard",
    "WalKEY-TalKEY Storage",
    "WalKEY-TalKEY Serial",
};

static const uint8_t s_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, TUSB_DESC_TOTAL_LEN, 0x00, USB_CONFIG_POWER_MA),
    TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, USB_STRING_INDEX_AUDIO,
                                    CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX,
                                    CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX * 8,
                                    USB_AUDIO_EP_IN_ADDR,
                                    CFG_TUD_AUDIO_EP_SZ_IN),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, USB_STRING_INDEX_HID, false, sizeof(s_hid_report_descriptor),
                       USB_HID_EP_IN_ADDR, HID_EP_SIZE, HID_POLL_INTERVAL_MS),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, USB_STRING_INDEX_MSC, USB_MSC_EP_OUT_ADDR, USB_MSC_EP_IN_ADDR, USB_MSC_EP_SIZE),
    TUD_CDC_NONOTIF_DESCRIPTOR(ITF_NUM_CDC_COMM, USB_STRING_INDEX_CDC,
                               USB_CDC_DATA_EP_OUT_ADDR, USB_CDC_DATA_EP_IN_ADDR, USB_CDC_DATA_EP_SIZE),
};

static void usb_composite_audio_task(void *arg)
{
    (void)arg;

    uint8_t frame[AUDIO_INPUT_FRAME_BYTES] = {0};

    while (1) {
        bool usb_ready = s_initialized && tud_mounted() && !s_suspended;
        bool stream_open = usb_ready && s_audio_stream_open;
        bool need_mic = stream_open || s_ptt_audio_active;

        if (!need_mic) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        if (s_audio_capture_suspended) {
            memset(frame, 0, sizeof(frame));
        } else {
            esp_err_t err = audio_input_read_frame(frame, sizeof(frame), s_ptt_audio_active);
            if ((err != ESP_OK) && s_ptt_audio_active) {
                ESP_LOGW(TAG, "Mic frame read failed: %s", esp_err_to_name(err));
            }
            if (s_ptt_audio_active) {
                audio_recorder_feed(frame, sizeof(frame));
            }
        }

        if (stream_open) {
            uint16_t written = tud_audio_write(frame, sizeof(frame));
            if (written != sizeof(frame)) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
}

static void usb_composite_audio_init_controls(void)
{
    s_sample_freq = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    s_clock_valid = 1;

    s_sample_freq_range.wNumSubRanges = 1;
    s_sample_freq_range.subrange[0].bMin = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    s_sample_freq_range.subrange[0].bMax = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    s_sample_freq_range.subrange[0].bRes = 0;

    for (size_t i = 0; i < (CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX + 1); i++) {
        s_mute[i] = 0;
        s_volume[i] = USB_DEFAULT_VOLUME_DB;
        s_volume_range[i].wNumSubRanges = 1;
        s_volume_range[i].subrange[0].bMin = USB_VOLUME_MIN_DB;
        s_volume_range[i].subrange[0].bMax = USB_VOLUME_MAX_DB;
        s_volume_range[i].subrange[0].bRes = USB_VOLUME_RES_DB;
    }
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer,
                               uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

static void usb_composite_event_cb(tinyusb_event_t *event, void *arg)
{
    (void)arg;

    switch (event->id) {
    case TINYUSB_EVENT_ATTACHED:
        ESP_LOGI(TAG, "USB composite device ready");
        break;
    case TINYUSB_EVENT_DETACHED:
        s_suspended = false;
        s_remote_wakeup_allowed = false;
        s_audio_stream_open = false;
        ESP_LOGW(TAG, "USB host disconnected");
        break;
#ifdef CONFIG_TINYUSB_SUSPEND_CALLBACK
    case TINYUSB_EVENT_SUSPENDED:
        s_suspended = true;
        s_remote_wakeup_allowed = event->suspended.remote_wakeup;
        ESP_LOGI(TAG, "USB suspended");
        break;
#endif
#ifdef CONFIG_TINYUSB_RESUME_CALLBACK
    case TINYUSB_EVENT_RESUMED:
        s_suspended = false;
        s_remote_wakeup_allowed = false;
        ESP_LOGI(TAG, "USB resumed");
        break;
#endif
    default:
        break;
    }
}

esp_err_t usb_composite_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "USB initialized");
    usb_composite_audio_init_controls();

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG(usb_composite_event_cb);
    tusb_cfg.descriptor.device = &s_device_descriptor;
    tusb_cfg.descriptor.full_speed_config = s_configuration_descriptor;
    tusb_cfg.descriptor.string = s_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(s_string_descriptor) / sizeof(s_string_descriptor[0]);

    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG, "Failed to install TinyUSB");

    BaseType_t task_created = xTaskCreate(
        usb_composite_audio_task,
        "usb_audio",
        USB_AUDIO_TASK_STACK_WORDS,
        NULL,
        USB_AUDIO_TASK_PRIORITY,
        &s_audio_task_handle
    );
    if (task_created != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    return ESP_OK;
}

bool usb_composite_hid_ready(void)
{
    return s_initialized && tud_mounted() && tud_hid_ready() && !s_suspended;
}

bool usb_composite_audio_ready(void)
{
    return s_initialized && tud_mounted() && !s_suspended && s_audio_stream_open && audio_input_ready();
}

void usb_composite_set_audio_capture_suspended(bool suspended)
{
    s_audio_capture_suspended = suspended;
}

void usb_composite_set_ptt_audio_active(bool active)
{
    s_ptt_audio_active = active;
}

uint32_t usb_composite_audio_stack_high_water_mark(void)
{
    if (s_audio_task_handle == NULL) {
        return 0;
    }

    return (uint32_t)uxTaskGetStackHighWaterMark(s_audio_task_handle);
}

static bool usb_composite_contains_key(uint16_t keycode)
{
    for (size_t i = 0; i < sizeof(s_active_keycodes); i++) {
        if (s_active_keycodes[i] == keycode) {
            return true;
        }
    }

    return false;
}

static bool usb_composite_add_key(uint16_t keycode)
{
    if (usb_composite_contains_key(keycode)) {
        return true;
    }

    for (size_t i = 0; i < sizeof(s_active_keycodes); i++) {
        if (s_active_keycodes[i] == 0) {
            s_active_keycodes[i] = keycode;
            return true;
        }
    }

    return false;
}

static void usb_composite_remove_key(uint16_t keycode)
{
    for (size_t i = 0; i < sizeof(s_active_keycodes); i++) {
        if (s_active_keycodes[i] == keycode) {
            s_active_keycodes[i] = 0;
            return;
        }
    }
}

static esp_err_t usb_composite_send_keyboard_usage(bool pressed, const mode_hid_usage_t *usage)
{
    if (!s_initialized || !tud_mounted()) {
        ESP_LOGW(TAG, "HID unavailable: USB not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_suspended) {
        if (s_remote_wakeup_allowed) {
            ESP_LOGW(TAG, "HID unavailable: waking suspended host");
            tud_remote_wakeup();
        } else {
            ESP_LOGW(TAG, "HID unavailable: USB suspended");
        }
        return ESP_ERR_INVALID_STATE;
    }

    if (!tud_hid_ready()) {
        ESP_LOGW(TAG, "HID unavailable: interface not ready");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t previous_keycodes[6] = {0};
    uint8_t previous_modifiers = s_active_modifiers;
    memcpy(previous_keycodes, s_active_keycodes, sizeof(previous_keycodes));

    if (pressed) {
        s_active_modifiers |= usage->modifiers;
        if ((usage->usage_id != MODE_KEY_NONE) && !usb_composite_add_key(usage->usage_id)) {
            s_active_modifiers = previous_modifiers;
            ESP_LOGW(TAG, "HID unavailable: no free key slots");
            return ESP_ERR_NO_MEM;
        }
    } else {
        if ((usage->usage_id != MODE_KEY_NONE) && usb_composite_contains_key(usage->usage_id)) {
            usb_composite_remove_key(usage->usage_id);
        }
        s_active_modifiers &= (uint8_t)~usage->modifiers;
    }

    bool any_key_active = false;
    for (size_t i = 0; i < sizeof(s_active_keycodes); i++) {
        if (s_active_keycodes[i] != 0) {
            any_key_active = true;
            break;
        }
    }

    bool report_sent = tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD,
                                               s_active_modifiers,
                                               (any_key_active || (s_active_modifiers != 0)) ? s_active_keycodes : NULL);
    if (!report_sent) {
        memcpy(s_active_keycodes, previous_keycodes, sizeof(s_active_keycodes));
        s_active_modifiers = previous_modifiers;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,
             "keyboard usage 0x%02X modifier 0x%02X %s sent",
             (unsigned)usage->usage_id,
             usage->modifiers,
             pressed ? "down" : "up");
    return ESP_OK;
}

static esp_err_t usb_composite_send_consumer_usage(bool pressed, const mode_hid_usage_t *usage)
{
    uint16_t previous_usage = s_active_consumer_usage;
    uint16_t report_usage = pressed ? usage->usage_id : 0;
    s_active_consumer_usage = report_usage;

    if (!tud_hid_report(HID_REPORT_ID_CONSUMER, &report_usage, sizeof(report_usage))) {
        s_active_consumer_usage = previous_usage;
        return ESP_FAIL;
    }

    return ESP_OK;
}

static uint8_t usb_composite_system_report_value(uint16_t usage_id)
{
    switch (usage_id) {
    case MODE_SYSTEM_USAGE_POWER_DOWN:
        return 1;
    case MODE_SYSTEM_USAGE_SLEEP:
        return 2;
    case MODE_SYSTEM_USAGE_WAKE_UP:
        return 3;
    default:
        return 0;
    }
}

static esp_err_t usb_composite_send_system_usage(bool pressed, const mode_hid_usage_t *usage)
{
    uint8_t previous_usage = s_active_system_usage;
    uint8_t report_usage = pressed ? usb_composite_system_report_value(usage->usage_id) : 0;
    if (pressed && (report_usage == 0)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    s_active_system_usage = report_usage;
    if (!tud_hid_report(HID_REPORT_ID_SYSTEM, &report_usage, sizeof(report_usage))) {
        s_active_system_usage = previous_usage;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t usb_composite_send_usage(bool pressed, const mode_hid_usage_t *usage)
{
    if (usage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized || !tud_mounted()) {
        ESP_LOGW(TAG, "HID unavailable: USB not mounted");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_suspended) {
        if (s_remote_wakeup_allowed) {
            ESP_LOGW(TAG, "HID unavailable: waking suspended host");
            tud_remote_wakeup();
        } else {
            ESP_LOGW(TAG, "HID unavailable: USB suspended");
        }
        return ESP_ERR_INVALID_STATE;
    }

    if (!tud_hid_ready()) {
        ESP_LOGW(TAG, "HID unavailable: interface not ready");
        return ESP_ERR_INVALID_STATE;
    }

    switch (usage->report_kind) {
    case MODE_HID_REPORT_KIND_KEYBOARD:
        return usb_composite_send_keyboard_usage(pressed, usage);
    case MODE_HID_REPORT_KIND_CONSUMER:
        return usb_composite_send_consumer_usage(pressed, usage);
    case MODE_HID_REPORT_KIND_SYSTEM:
        return usb_composite_send_system_usage(pressed, usage);
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t usb_composite_release_all_keys(void)
{
    memset(s_active_keycodes, 0, sizeof(s_active_keycodes));
    s_active_modifiers = 0;
    s_active_consumer_usage = 0;
    s_active_system_usage = 0;

    if (!s_initialized || !tud_mounted() || s_suspended || !tud_hid_ready()) {
        return ESP_OK;
    }

    if (!tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, 0, NULL)) {
        return ESP_FAIL;
    }
    if (!tud_hid_report(HID_REPORT_ID_CONSUMER, &s_active_consumer_usage, sizeof(s_active_consumer_usage))) {
        return ESP_FAIL;
    }
    if (!tud_hid_report(HID_REPORT_ID_SYSTEM, &s_active_system_usage, sizeof(s_active_system_usage))) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "all HID keys released");
    return ESP_OK;
}

esp_err_t usb_composite_send_mouse_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel)
{
    if (!s_initialized || !tud_mounted()) {
        ESP_LOGW(TAG, "Mouse report blocked: init=%d mounted=%d", s_initialized, tud_mounted());
        return ESP_ERR_INVALID_STATE;
    }

    if (s_suspended) {
        if (s_remote_wakeup_allowed) {
            ESP_LOGW(TAG, "Mouse report: waking suspended host");
            tud_remote_wakeup();
        } else {
            ESP_LOGW(TAG, "Mouse report blocked: suspended, no remote wakeup");
        }
        return ESP_ERR_INVALID_STATE;
    }

    if (!tud_hid_ready()) {
        ESP_LOGW(TAG, "Mouse report blocked: HID interface not ready");
        return ESP_ERR_INVALID_STATE;
    }

    if (!tud_hid_mouse_report(HID_REPORT_ID_MOUSE, buttons, dx, dy, wheel, 0)) {
        ESP_LOGW(TAG, "Mouse report send failed (tud_hid_mouse_report returned false)");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t usb_composite_send_f13_down(void)
{
    return usb_composite_send_usage(true,
                                    &(mode_hid_usage_t){
                                        .report_kind = MODE_HID_REPORT_KIND_KEYBOARD,
                                        .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD,
                                        .modifiers = MODE_HID_MODIFIER_NONE,
                                        .usage_id = HID_KEY_F13,
                                    });
}

esp_err_t usb_composite_send_f13_up(void)
{
    return usb_composite_send_usage(false,
                                    &(mode_hid_usage_t){
                                        .report_kind = MODE_HID_REPORT_KIND_KEYBOARD,
                                        .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD,
                                        .modifiers = MODE_HID_MODIFIER_NONE,
                                        .usage_id = HID_KEY_F13,
                                    });
}

esp_err_t usb_composite_send_f14_down(void)
{
    return usb_composite_send_usage(true,
                                    &(mode_hid_usage_t){
                                        .report_kind = MODE_HID_REPORT_KIND_KEYBOARD,
                                        .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD,
                                        .modifiers = MODE_HID_MODIFIER_NONE,
                                        .usage_id = HID_KEY_F14,
                                    });
}

esp_err_t usb_composite_send_f14_up(void)
{
    return usb_composite_send_usage(false,
                                    &(mode_hid_usage_t){
                                        .report_kind = MODE_HID_REPORT_KIND_KEYBOARD,
                                        .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD,
                                        .modifiers = MODE_HID_MODIFIER_NONE,
                                        .usage_id = HID_KEY_F14,
                                    });
}

bool tud_audio_set_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
{
    (void)rhport;
    (void)p_request;
    (void)pBuff;
    return false;
}

bool tud_audio_set_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
{
    (void)rhport;
    (void)p_request;
    (void)pBuff;
    return false;
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *pBuff)
{
    (void)rhport;

    uint8_t channel_num = TU_U16_LOW(p_request->wValue);
    uint8_t ctrl_sel = TU_U16_HIGH(p_request->wValue);
    uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);

    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    if (entity_id == AUDIO_ENTITY_FEATURE_UNIT) {
        switch (ctrl_sel) {
        case AUDIO_FU_CTRL_MUTE:
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));
            s_mute[channel_num] = ((audio_control_cur_1_t *)pBuff)->bCur;
            return true;
        case AUDIO_FU_CTRL_VOLUME:
            TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));
            s_volume[channel_num] = (int16_t)((audio_control_cur_2_t *)pBuff)->bCur;
            return true;
        default:
            return false;
        }
    }

    return false;
}

bool tud_audio_get_req_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;
    (void)p_request;
    return false;
}

bool tud_audio_get_req_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;
    (void)p_request;
    return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    uint8_t channel_num = TU_U16_LOW(p_request->wValue);
    uint8_t ctrl_sel = TU_U16_HIGH(p_request->wValue);
    uint8_t entity_id = TU_U16_HIGH(p_request->wIndex);

    if (entity_id == AUDIO_ENTITY_INPUT_TERMINAL) {
        if (ctrl_sel == AUDIO_TE_CTRL_CONNECTOR) {
            audio_desc_channel_cluster_t ret = {
                .bNrChannels = CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX,
                .bmChannelConfig = (audio_channel_config_t)0,
                .iChannelNames = 0,
            };
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void *)&ret, sizeof(ret));
        }
        return false;
    }

    if (entity_id == AUDIO_ENTITY_FEATURE_UNIT) {
        switch (ctrl_sel) {
        case AUDIO_FU_CTRL_MUTE:
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &s_mute[channel_num], sizeof(s_mute[channel_num]));
        case AUDIO_FU_CTRL_VOLUME:
            if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &s_volume[channel_num], sizeof(s_volume[channel_num]));
            }
            if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &s_volume_range[channel_num], sizeof(s_volume_range[channel_num]));
            }
            return false;
        default:
            return false;
        }
    }

    if (entity_id == AUDIO_ENTITY_CLOCK_SOURCE) {
        switch (ctrl_sel) {
        case AUDIO_CS_CTRL_SAM_FREQ:
            if (p_request->bRequest == AUDIO_CS_REQ_CUR) {
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &s_sample_freq, sizeof(s_sample_freq));
            }
            if (p_request->bRequest == AUDIO_CS_REQ_RANGE) {
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &s_sample_freq_range, sizeof(s_sample_freq_range));
            }
            return false;
        case AUDIO_CS_CTRL_CLK_VALID:
            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &s_clock_valid, sizeof(s_clock_valid));
        default:
            return false;
        }
    }

    return false;
}

bool tud_audio_set_itf_close_ep_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;

    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t alt = TU_U16_LOW(p_request->wValue);

    if ((itf == ITF_NUM_AUDIO_STREAMING) && (alt == 0)) {
        s_audio_stream_open = false;
        tud_audio_clear_ep_in_ff();
        ESP_LOGI(TAG, "USB microphone streaming stopped");
    }

    return true;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request)
{
    (void)rhport;

    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t alt = TU_U16_LOW(p_request->wValue);

    if ((itf == ITF_NUM_AUDIO_STREAMING) && (alt != 0)) {
        s_audio_stream_open = true;
        tud_audio_clear_ep_in_ff();
        ESP_LOGI(TAG, "USB microphone streaming started");
    }

    return true;
}

/*--------------------------------------------------------------------------*
 * Stubs required by esp_tinyusb's tinyusb.c when MSC is enabled.
 * We provide our own callbacks below instead of using tinyusb_msc.c.
 *--------------------------------------------------------------------------*/

esp_err_t msc_storage_mount_to_usb(void) { return ESP_OK; }
esp_err_t msc_storage_mount_to_app(void) { return ESP_OK; }

/*--------------------------------------------------------------------------*
 * MSC Device Callbacks
 *--------------------------------------------------------------------------*/

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
    (void)lun;
    memcpy(vendor_id,   "WALKEY  ", 8);
    memcpy(product_id,  "TalKEY Storage  ", 16);
    memcpy(product_rev, "1.0 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun)
{
    (void)lun;
    return sd_card_is_present();
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count, uint16_t *block_size)
{
    (void)lun;
    *block_count = sd_card_block_count();
    *block_size = sd_card_block_size();
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
    (void)lun;
    (void)power_condition;
    (void)start;
    (void)load_eject;
    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    (void)lun;
    (void)offset;
    if (!sd_card_is_present()) {
        return -1;
    }
    uint16_t bs = sd_card_block_size();
    if (bs == 0) {
        return -1;
    }
    uint32_t block_count = bufsize / bs;
    return sd_card_read_blocks(lba, buffer, block_count);
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    (void)lun;
    (void)offset;
    if (!sd_card_is_present()) {
        return -1;
    }
    uint16_t bs = sd_card_block_size();
    if (bs == 0) {
        return -1;
    }
    uint32_t block_count = bufsize / bs;
    return sd_card_write_blocks(lba, buffer, block_count);
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer, uint16_t bufsize)
{
    (void)buffer;
    (void)bufsize;

    int32_t resplen = 0;
    switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        break;
    default:
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
        resplen = -1;
        break;
    }
    return resplen;
}

/*--------------------------------------------------------------------------*
 * CDC ACM Callbacks
 *--------------------------------------------------------------------------*/

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *p_line_coding)
{
    (void)itf;
    (void)p_line_coding;
}

void tud_cdc_rx_cb(uint8_t itf)
{
    uint8_t buf[64];
    while (tud_cdc_available()) {
        tud_cdc_read(buf, sizeof(buf));
    }
}
