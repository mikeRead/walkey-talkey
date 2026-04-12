#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint16_t mode_keycode_t;
typedef uint8_t mode_hid_modifier_t;
typedef uint16_t mode_id_t;
typedef uint16_t mode_hid_usage_page_t;

enum {
    MODE_HID_USAGE_PAGE_GENERIC_DESKTOP = 0x01,
    MODE_HID_USAGE_PAGE_KEYBOARD = 0x07,
    MODE_HID_USAGE_PAGE_CONSUMER = 0x0C,
};

typedef enum {
    MODE_HID_REPORT_KIND_KEYBOARD = 0,
    MODE_HID_REPORT_KIND_CONSUMER,
    MODE_HID_REPORT_KIND_SYSTEM,
} mode_hid_report_kind_t;

enum {
    MODE_KEY_NONE = 0x00,
    MODE_KEY_1 = 0x1E,
    MODE_KEY_A = 0x04,
    MODE_KEY_B = 0x05,
    MODE_KEY_N = 0x11,
    MODE_KEY_Z = 0x1D,
    MODE_KEY_ENTER = 0x28,
    MODE_KEY_ESCAPE = 0x29,
    MODE_KEY_BACKSPACE = 0x2A,
    MODE_KEY_SPACE = 0x2C,
    MODE_KEY_PERIOD = 0x37,
    MODE_KEY_PAGE_UP = 0x4B,
    MODE_KEY_DOWN_ARROW = 0x51,
    MODE_KEY_UP_ARROW = 0x52,
    MODE_KEY_RIGHT_ARROW = 0x4F,
    MODE_KEY_LEFT_ARROW = 0x50,
    MODE_KEY_PAGE_DOWN = 0x4E,
    MODE_KEY_TAB = 0x2B,
    MODE_KEY_DELETE = 0x4C,
    MODE_KEY_APPLICATION = 0x65,
    MODE_KEY_F13 = 0x68,
    MODE_KEY_F14 = 0x69,
};

enum {
    MODE_HID_MODIFIER_NONE = 0x00,
    MODE_HID_MODIFIER_LEFT_CTRL = 0x01,
    MODE_HID_MODIFIER_LEFT_SHIFT = 0x02,
    MODE_HID_MODIFIER_LEFT_ALT = 0x04,
    MODE_HID_MODIFIER_LEFT_GUI = 0x08,
    MODE_HID_MODIFIER_RIGHT_CTRL = 0x10,
    MODE_HID_MODIFIER_RIGHT_SHIFT = 0x20,
    MODE_HID_MODIFIER_RIGHT_ALT = 0x40,
    MODE_HID_MODIFIER_RIGHT_GUI = 0x80,
};

enum {
    MODE_SYSTEM_USAGE_POWER_DOWN = 0x81,
    MODE_SYSTEM_USAGE_SLEEP = 0x82,
    MODE_SYSTEM_USAGE_WAKE_UP = 0x83,
};

enum {
    MODE_CONSUMER_USAGE_PLAY_PAUSE = 0x00CD,
    MODE_CONSUMER_USAGE_SCAN_NEXT_TRACK = 0x00B5,
    MODE_CONSUMER_USAGE_SCAN_PREVIOUS_TRACK = 0x00B6,
    MODE_CONSUMER_USAGE_STOP = 0x00B7,
    MODE_CONSUMER_USAGE_MUTE = 0x00E2,
    MODE_CONSUMER_USAGE_VOLUME_INCREMENT = 0x00E9,
    MODE_CONSUMER_USAGE_VOLUME_DECREMENT = 0x00EA,
};

typedef enum {
    MODE_INPUT_BOOT_BUTTON = 0,
    MODE_INPUT_TOUCH,
    MODE_INPUT_ENCODER,
    MODE_INPUT_USB_HOST_KEY,
    MODE_INPUT_TIMER,
    MODE_INPUT_IMU,
} mode_input_t;

typedef enum {
    MODE_TRIGGER_PRESS = 0,
    MODE_TRIGGER_RELEASE,
    MODE_TRIGGER_TAP,
    MODE_TRIGGER_DOUBLE_TAP,
    MODE_TRIGGER_LONG_PRESS,
    MODE_TRIGGER_HOLD_START,
    MODE_TRIGGER_HOLD_END,
    MODE_TRIGGER_SWIPE_UP,
    MODE_TRIGGER_SWIPE_DOWN,
    MODE_TRIGGER_SWIPE_LEFT,
    MODE_TRIGGER_SWIPE_RIGHT,
} mode_trigger_t;

enum {
    MODE_ID_CURSOR = 0,
    MODE_ID_MEDIA,
    MODE_ID_ARROWS,
    /* Legacy aliases retained for older static references. */
    MODE_ID_PRESENTATION = MODE_ID_MEDIA,
    MODE_ID_NAVIGATION = MODE_ID_ARROWS,
    MODE_ID_COUNT = 3,
    /* Hardcoded mouse/trackpad mode — not part of the JSON config. */
    MODE_ID_MOUSE = 0xFFFE,
    MODE_ID_INVALID = 0xFFFF,
};

typedef enum {
    MODE_CYCLE_DIRECTION_NEXT = 0,
    MODE_CYCLE_DIRECTION_PREVIOUS,
} mode_cycle_direction_t;

typedef enum {
    MODE_ACTION_HID_KEY_DOWN = 0,
    MODE_ACTION_HID_KEY_UP,
    MODE_ACTION_HID_KEY_TAP,
    MODE_ACTION_HID_SHORTCUT_TAP,
    MODE_ACTION_HID_MODIFIER_DOWN,
    MODE_ACTION_HID_MODIFIER_UP,
    MODE_ACTION_HID_USAGE_DOWN,
    MODE_ACTION_HID_USAGE_UP,
    MODE_ACTION_HID_USAGE_TAP,
    MODE_ACTION_SLEEP_MS,
    MODE_ACTION_ENTER_BOOT_MODE,
    MODE_ACTION_EXIT_BOOT_MODE,
    MODE_ACTION_MIC_GATE,
    MODE_ACTION_MIC_GATE_TOGGLE,
    MODE_ACTION_UI_HINT,
    MODE_ACTION_UI_SHOW_MODE,
    MODE_ACTION_SET_MODE,
    MODE_ACTION_CYCLE_MODE,
    MODE_ACTION_MOUSE_ON,
    MODE_ACTION_MOUSE_OFF,
    MODE_ACTION_MOUSE_TOGGLE,
    MODE_ACTION_NOOP,
} mode_action_type_t;

typedef enum {
    MOUSE_MODE_TYPE_TOUCH = 0,
    MOUSE_MODE_TYPE_AIR,
    MOUSE_MODE_TYPE_DEFAULT = 0xFF,
} mouse_mode_type_t;

typedef struct {
    mouse_mode_type_t mouse_type;
    bool tracking;
} mode_mouse_overlay_t;

typedef struct {
    mode_hid_report_kind_t report_kind;
    mode_hid_usage_page_t usage_page;
    mode_hid_modifier_t modifiers;
    uint16_t usage_id;
} mode_hid_usage_t;

typedef struct {
    bool enabled;
    int8_t recording_override; /* -1 = use global default, 0 = force off, 1 = force on */
} mode_mic_gate_data_t;

typedef struct {
    mode_action_type_t type;
    union {
        mode_hid_usage_t hid_usage;
        mode_hid_modifier_t modifier;
        uint32_t duration_ms;
        bool enabled;
        mode_mic_gate_data_t mic_gate;
        const char *text;
        mode_id_t mode;
        mode_cycle_direction_t direction;
        mode_mouse_overlay_t mouse_overlay;
    } data;
} mode_action_t;

typedef struct {
    mode_input_t input;
    mode_trigger_t trigger;
    const mode_action_t *actions;
    size_t action_count;
} mode_binding_t;

typedef struct {
    uint32_t hold_ms;
    uint32_t double_tap_ms;
    uint16_t swipe_min_distance;
} mode_touch_defaults_t;

typedef struct {
    float sensitivity;
    float dead_zone_dps;
    float easing_exponent;
    float max_dps;
    float ema_alpha;
    uint32_t rewind_depth;
    float rewind_decay;
    uint32_t calibration_samples;
} air_mouse_config_t;

typedef struct {
    float sensitivity;
    uint16_t move_threshold_px;
    uint32_t tap_drag_window_ms;
} touch_mouse_config_t;

typedef struct {
    mode_touch_defaults_t touch;
    mouse_mode_type_t default_mouse;
    air_mouse_config_t air_mouse;
    touch_mouse_config_t touch_mouse;
} mode_defaults_t;

typedef struct {
    const char *ssid;
    const char *password;
} mode_wifi_credentials_t;

typedef struct {
    mode_wifi_credentials_t sta;
    mode_wifi_credentials_t ap;
    const char *hostname;
    const char *local_url;
} mode_wifi_config_t;

typedef struct {
    bool enabled;
    const char *format;
} mode_recording_config_t;

typedef struct {
    const char *title;
    const char *subtitle;
    bool show_mode_list;
    bool show_gesture_hints;
    bool show_current_mode_card;
} mode_boot_ui_t;

typedef struct {
    const char *label;
    mode_boot_ui_t ui;
    const mode_binding_t *bindings;
    size_t binding_count;
} mode_boot_mode_t;

typedef struct {
    mode_id_t id;
    const char *name;
    const char *label;
    uint32_t cycle_order;
    const mode_binding_t *bindings;
    size_t binding_count;
} mode_definition_t;

typedef struct {
    uint32_t version;
    mode_id_t active_mode;
    mode_defaults_t defaults;
    mode_wifi_config_t wifi;
    mode_recording_config_t recording;
    const mode_binding_t *global_bindings;
    size_t global_binding_count;
    mode_boot_mode_t boot_mode;
    const mode_definition_t *modes;
    size_t mode_count;
} mode_config_t;

typedef struct {
    mode_input_t input;
    mode_trigger_t trigger;
} mode_binding_event_t;
