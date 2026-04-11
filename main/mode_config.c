#include "mode_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mode_json_loader.h"

#if defined(ESP_PLATFORM)
#define MODE_CONFIG_HAS_ESP_SPIFFS 1
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#else
#define MODE_CONFIG_HAS_ESP_SPIFFS 0
#endif

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MODE_CONFIG_SPIFFS_MOUNT_POINT "/spiffs"
#define MODE_CONFIG_SPIFFS_PARTITION_LABEL "storage"

#if MODE_CONFIG_HAS_ESP_SPIFFS
#define MODE_CONFIG_EXTERNAL_PATH MODE_CONFIG_SPIFFS_MOUNT_POINT "/mode-config.json"
static const char *TAG = "mode_config";
#else
#define MODE_CONFIG_EXTERNAL_PATH "mode-config.json"
#endif

#define ACTION_KEY_DOWN(key) { .type = MODE_ACTION_HID_KEY_DOWN, .data.hid_usage = { .report_kind = MODE_HID_REPORT_KIND_KEYBOARD, .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD, .modifiers = MODE_HID_MODIFIER_NONE, .usage_id = (key) } }
#define ACTION_KEY_UP(key) { .type = MODE_ACTION_HID_KEY_UP, .data.hid_usage = { .report_kind = MODE_HID_REPORT_KIND_KEYBOARD, .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD, .modifiers = MODE_HID_MODIFIER_NONE, .usage_id = (key) } }
#define ACTION_KEY_TAP(key) { .type = MODE_ACTION_HID_KEY_TAP, .data.hid_usage = { .report_kind = MODE_HID_REPORT_KIND_KEYBOARD, .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD, .modifiers = MODE_HID_MODIFIER_NONE, .usage_id = (key) } }
#define ACTION_SHORTCUT_TAP(modifier_value, key_value) { .type = MODE_ACTION_HID_SHORTCUT_TAP, .data.hid_usage = { .report_kind = MODE_HID_REPORT_KIND_KEYBOARD, .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD, .modifiers = (modifier_value), .usage_id = (key_value) } }
#define ACTION_USAGE_TAP(kind_value, page_value, usage_value) { .type = MODE_ACTION_HID_USAGE_TAP, .data.hid_usage = { .report_kind = (kind_value), .usage_page = (page_value), .modifiers = MODE_HID_MODIFIER_NONE, .usage_id = (usage_value) } }
#define ACTION_SLEEP_MS(duration_value) { .type = MODE_ACTION_SLEEP_MS, .data.duration_ms = (duration_value) }
#define ACTION_MIC_GATE(enabled_value) { .type = MODE_ACTION_MIC_GATE, .data.enabled = (enabled_value) }
#define ACTION_HINT(text_value) { .type = MODE_ACTION_UI_HINT, .data.text = (text_value) }
#define ACTION_SHOW_MODE() { .type = MODE_ACTION_UI_SHOW_MODE }
#define ACTION_ENTER_BOOT() { .type = MODE_ACTION_ENTER_BOOT_MODE }
#define ACTION_EXIT_BOOT() { .type = MODE_ACTION_EXIT_BOOT_MODE }
#define ACTION_SET_MODE(mode_value) { .type = MODE_ACTION_SET_MODE, .data.mode = (mode_value) }
#define ACTION_CYCLE(direction_value) { .type = MODE_ACTION_CYCLE_MODE, .data.direction = (direction_value) }

static const char *s_builtin_mode_config_json =
    "{\n"
    "  \"version\": 1,\n"
    "  \"activeMode\": \"cursor\",\n"
    "  \"defaults\": {\n"
    "    \"touch\": {\n"
    "      \"holdMs\": 400,\n"
    "      \"doubleTapMs\": 350,\n"
    "      \"swipeMinDistance\": 40\n"
    "    }\n"
    "  },\n"
    "  \"wifi\": {\n"
    "    \"sta\": {\n"
    "      \"ssid\": \"YourNetworkName\",\n"
    "      \"password\": \"YourPassword\"\n"
    "    },\n"
    "    \"ap\": {\n"
    "      \"ssid\": \"walkey-talkey\",\n"
    "      \"password\": \"secretKEY\"\n"
    "    },\n"
    "    \"hostname\": \"walkey-talkey\",\n"
    "    \"localUrl\": \"walkey-talkey.local\"\n"
    "  },\n"
    "  \"globalBindings\": [\n"
    "    {\n"
    "      \"input\": \"boot_button\",\n"
    "      \"trigger\": \"press\",\n"
    "      \"actions\": [\n"
    "        { \"type\": \"enter_boot_mode\" }\n"
    "      ]\n"
    "    },\n"
    "    {\n"
    "      \"input\": \"boot_button\",\n"
    "      \"trigger\": \"release\",\n"
    "      \"actions\": [\n"
    "        { \"type\": \"exit_boot_mode\" }\n"
    "      ]\n"
    "    }\n"
    "  ],\n"
    "  \"bootMode\": {\n"
    "    \"label\": \"Mode Control\",\n"
    "    \"ui\": {\n"
    "      \"title\": \"Swipe to switch mode\",\n"
    "      \"subtitle\": \"Hold BOOT and swipe to change modes\",\n"
    "      \"showModeList\": true,\n"
    "      \"showGestureHints\": true,\n"
    "      \"showCurrentModeCard\": true\n"
    "    },\n"
    "    \"bindings\": [\n"
    "      {\n"
    "        \"input\": \"touch\",\n"
    "        \"trigger\": \"swipe_right\",\n"
    "        \"actions\": [\n"
    "          { \"type\": \"cycle_mode\", \"direction\": \"next\" },\n"
    "          { \"type\": \"ui_show_mode\" }\n"
    "        ]\n"
    "      },\n"
    "      {\n"
    "        \"input\": \"touch\",\n"
    "        \"trigger\": \"swipe_left\",\n"
    "        \"actions\": [\n"
    "          { \"type\": \"cycle_mode\", \"direction\": \"previous\" },\n"
    "          { \"type\": \"ui_show_mode\" }\n"
    "        ]\n"
    "      },\n"
    "      {\n"
    "        \"input\": \"touch\",\n"
    "        \"trigger\": \"tap\",\n"
    "        \"actions\": [\n"
    "          { \"type\": \"ui_show_mode\" }\n"
    "        ]\n"
    "      },\n"
    "      {\n"
    "        \"input\": \"touch\",\n"
    "        \"trigger\": \"long_press\",\n"
    "        \"actions\": [\n"
    "          { \"type\": \"ui_show_mode\" }\n"
    "        ]\n"
    "      }\n"
    "    ]\n"
    "  },\n"
    "  \"modes\": [\n"
    "    {\n"
    "      \"id\": \"cursor\",\n"
    "      \"cycleOrder\": 0,\n"
    "      \"label\": \"Cursor\",\n"
    "      \"bindings\": [\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"hold_start\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_down\", \"key\": \"F13\" },\n"
    "            { \"type\": \"mic_gate\", \"enabled\": true },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Dictation active\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"hold_end\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"mic_gate\", \"enabled\": false },\n"
    "            { \"type\": \"hid_key_up\", \"key\": \"F13\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Cursor mode\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"tap\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"F14\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Cursor mode\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"double_tap\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"ENTER\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Submit\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_up\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_shortcut_tap\", \"modifiers\": [\"CTRL\"], \"key\": \"A\" },\n"
    "            { \"type\": \"sleep_ms\", \"duration_ms\": 20 },\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"BACKSPACE\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Clear field\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_down\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_shortcut_tap\", \"modifiers\": [\"CTRL\"], \"key\": \"PERIOD\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Toggle text mode\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_left\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_shortcut_tap\", \"modifiers\": [\"CTRL\"], \"key\": \"N\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"New chat\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_right\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"ENTER\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Enter\" }\n"
    "          ]\n"
    "        }\n"
    "      ]\n"
    "    },\n"
    "    {\n"
    "      \"id\": \"media\",\n"
    "      \"cycleOrder\": 1,\n"
    "      \"label\": \"Media\",\n"
    "      \"bindings\": [\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"tap\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_usage_tap\", \"usage\": \"PLAY_PAUSE\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Media mode\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_left\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_usage_tap\", \"usage\": \"MEDIA_PREV_TRACK\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Media previous\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_right\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_usage_tap\", \"usage\": \"MEDIA_NEXT_TRACK\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Media next\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_up\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_usage_tap\", \"usage\": \"VOLUME_UP\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Media up\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_down\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_usage_tap\", \"usage\": \"VOLUME_DOWN\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Media down\" }\n"
    "          ]\n"
    "        }\n"
    "      ]\n"
    "    },\n"
    "    {\n"
    "      \"id\": \"arrows\",\n"
    "      \"cycleOrder\": 2,\n"
    "      \"label\": \"Arrows\",\n"
    "      \"bindings\": [\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_up\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"LEFT_ARROW\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Left\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_down\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"RIGHT_ARROW\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Right\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_left\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"DOWN_ARROW\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Down\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"swipe_right\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"UP_ARROW\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Up\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"double_tap\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"ENTER\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Enter\" }\n"
    "          ]\n"
    "        },\n"
    "        {\n"
    "          \"input\": \"touch\",\n"
    "          \"trigger\": \"long_press\",\n"
    "          \"actions\": [\n"
    "            { \"type\": \"hid_key_tap\", \"key\": \"SPACE\" },\n"
    "            { \"type\": \"ui_hint\", \"text\": \"Space\" }\n"
    "          ]\n"
    "        }\n"
    "      ]\n"
    "    }\n"
    "  ]\n"
    "}\n";

static const mode_action_t s_global_boot_press_actions[] = {
    ACTION_ENTER_BOOT(),
};

static const mode_action_t s_global_boot_release_actions[] = {
    ACTION_EXIT_BOOT(),
};

static const mode_binding_t s_global_bindings[] = {
    {
        .input = MODE_INPUT_BOOT_BUTTON,
        .trigger = MODE_TRIGGER_PRESS,
        .actions = s_global_boot_press_actions,
        .action_count = ARRAY_SIZE(s_global_boot_press_actions),
    },
    {
        .input = MODE_INPUT_BOOT_BUTTON,
        .trigger = MODE_TRIGGER_RELEASE,
        .actions = s_global_boot_release_actions,
        .action_count = ARRAY_SIZE(s_global_boot_release_actions),
    },
};

static const mode_action_t s_boot_swipe_right_actions[] = {
    ACTION_CYCLE(MODE_CYCLE_DIRECTION_NEXT),
    ACTION_SHOW_MODE(),
};

static const mode_action_t s_boot_swipe_left_actions[] = {
    ACTION_CYCLE(MODE_CYCLE_DIRECTION_PREVIOUS),
    ACTION_SHOW_MODE(),
};

static const mode_action_t s_boot_tap_actions[] = {
    ACTION_SHOW_MODE(),
};

static const mode_action_t s_boot_long_press_actions[] = {
    ACTION_SHOW_MODE(),
};

static const mode_binding_t s_boot_bindings[] = {
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_RIGHT,
        .actions = s_boot_swipe_right_actions,
        .action_count = ARRAY_SIZE(s_boot_swipe_right_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_LEFT,
        .actions = s_boot_swipe_left_actions,
        .action_count = ARRAY_SIZE(s_boot_swipe_left_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_TAP,
        .actions = s_boot_tap_actions,
        .action_count = ARRAY_SIZE(s_boot_tap_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_LONG_PRESS,
        .actions = s_boot_long_press_actions,
        .action_count = ARRAY_SIZE(s_boot_long_press_actions),
    },
};

static const mode_action_t s_cursor_hold_start_actions[] = {
    ACTION_KEY_DOWN(MODE_KEY_F13),
    ACTION_MIC_GATE(true),
    ACTION_HINT("Dictation active"),
};

static const mode_action_t s_cursor_hold_end_actions[] = {
    ACTION_MIC_GATE(false),
    ACTION_KEY_UP(MODE_KEY_F13),
    ACTION_HINT("Cursor mode"),
};

static const mode_action_t s_cursor_tap_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_F14),
    ACTION_HINT("Cursor mode"),
};

static const mode_action_t s_cursor_double_tap_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_ENTER),
    ACTION_HINT("Submit"),
};

static const mode_action_t s_cursor_swipe_up_actions[] = {
    ACTION_SHORTCUT_TAP(MODE_HID_MODIFIER_LEFT_CTRL, MODE_KEY_A),
    ACTION_SLEEP_MS(20),
    ACTION_KEY_TAP(MODE_KEY_BACKSPACE),
    ACTION_HINT("Clear field"),
};

static const mode_action_t s_cursor_swipe_down_actions[] = {
    ACTION_SHORTCUT_TAP(MODE_HID_MODIFIER_LEFT_CTRL, MODE_KEY_PERIOD),
    ACTION_HINT("Toggle text mode"),
};

static const mode_action_t s_cursor_swipe_left_actions[] = {
    ACTION_SHORTCUT_TAP(MODE_HID_MODIFIER_LEFT_CTRL, MODE_KEY_N),
    ACTION_HINT("New chat"),
};

static const mode_action_t s_cursor_swipe_right_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_ENTER),
    ACTION_HINT("Enter"),
};

static const mode_binding_t s_cursor_bindings[] = {
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_HOLD_START,
        .actions = s_cursor_hold_start_actions,
        .action_count = ARRAY_SIZE(s_cursor_hold_start_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_HOLD_END,
        .actions = s_cursor_hold_end_actions,
        .action_count = ARRAY_SIZE(s_cursor_hold_end_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_TAP,
        .actions = s_cursor_tap_actions,
        .action_count = ARRAY_SIZE(s_cursor_tap_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_DOUBLE_TAP,
        .actions = s_cursor_double_tap_actions,
        .action_count = ARRAY_SIZE(s_cursor_double_tap_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_UP,
        .actions = s_cursor_swipe_up_actions,
        .action_count = ARRAY_SIZE(s_cursor_swipe_up_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_DOWN,
        .actions = s_cursor_swipe_down_actions,
        .action_count = ARRAY_SIZE(s_cursor_swipe_down_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_LEFT,
        .actions = s_cursor_swipe_left_actions,
        .action_count = ARRAY_SIZE(s_cursor_swipe_left_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_RIGHT,
        .actions = s_cursor_swipe_right_actions,
        .action_count = ARRAY_SIZE(s_cursor_swipe_right_actions),
    },
};

static const mode_action_t s_presentation_swipe_left_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_PAGE_DOWN),
    ACTION_HINT("Next slide"),
};

static const mode_action_t s_presentation_swipe_right_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_PAGE_UP),
    ACTION_HINT("Previous slide"),
};

static const mode_action_t s_presentation_tap_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_SPACE),
    ACTION_HINT("Presentation mode"),
};

static const mode_action_t s_presentation_double_tap_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_B),
    ACTION_HINT("Black screen"),
};

static const mode_binding_t s_presentation_bindings[] __attribute__((unused)) = {
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_LEFT,
        .actions = s_presentation_swipe_left_actions,
        .action_count = ARRAY_SIZE(s_presentation_swipe_left_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_RIGHT,
        .actions = s_presentation_swipe_right_actions,
        .action_count = ARRAY_SIZE(s_presentation_swipe_right_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_TAP,
        .actions = s_presentation_tap_actions,
        .action_count = ARRAY_SIZE(s_presentation_tap_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_DOUBLE_TAP,
        .actions = s_presentation_double_tap_actions,
        .action_count = ARRAY_SIZE(s_presentation_double_tap_actions),
    },
};

static const mode_action_t s_media_tap_actions[] = {
    ACTION_USAGE_TAP(MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, MODE_CONSUMER_USAGE_PLAY_PAUSE),
    ACTION_HINT("Media mode"),
};

static const mode_action_t s_media_swipe_left_actions[] = {
    ACTION_USAGE_TAP(MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, MODE_CONSUMER_USAGE_SCAN_PREVIOUS_TRACK),
    ACTION_HINT("Media previous"),
};

static const mode_action_t s_media_swipe_right_actions[] = {
    ACTION_USAGE_TAP(MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, MODE_CONSUMER_USAGE_SCAN_NEXT_TRACK),
    ACTION_HINT("Media next"),
};

static const mode_action_t s_media_swipe_up_actions[] = {
    ACTION_USAGE_TAP(MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, MODE_CONSUMER_USAGE_VOLUME_INCREMENT),
    ACTION_HINT("Media up"),
};

static const mode_action_t s_media_swipe_down_actions[] = {
    ACTION_USAGE_TAP(MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, MODE_CONSUMER_USAGE_VOLUME_DECREMENT),
    ACTION_HINT("Media down"),
};

static const mode_binding_t s_media_bindings[] = {
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_TAP,
        .actions = s_media_tap_actions,
        .action_count = ARRAY_SIZE(s_media_tap_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_LEFT,
        .actions = s_media_swipe_left_actions,
        .action_count = ARRAY_SIZE(s_media_swipe_left_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_RIGHT,
        .actions = s_media_swipe_right_actions,
        .action_count = ARRAY_SIZE(s_media_swipe_right_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_UP,
        .actions = s_media_swipe_up_actions,
        .action_count = ARRAY_SIZE(s_media_swipe_up_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_DOWN,
        .actions = s_media_swipe_down_actions,
        .action_count = ARRAY_SIZE(s_media_swipe_down_actions),
    },
};

static const mode_action_t s_arrows_swipe_up_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_LEFT_ARROW),
    ACTION_HINT("Left"),
};

static const mode_action_t s_arrows_swipe_down_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_RIGHT_ARROW),
    ACTION_HINT("Right"),
};

static const mode_action_t s_arrows_swipe_left_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_DOWN_ARROW),
    ACTION_HINT("Down"),
};

static const mode_action_t s_arrows_swipe_right_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_UP_ARROW),
    ACTION_HINT("Up"),
};

static const mode_action_t s_arrows_double_tap_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_ENTER),
    ACTION_HINT("Enter"),
};

static const mode_action_t s_arrows_long_press_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_SPACE),
    ACTION_HINT("Space"),
};

static const mode_binding_t s_arrows_bindings[] = {
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_UP,
        .actions = s_arrows_swipe_up_actions,
        .action_count = ARRAY_SIZE(s_arrows_swipe_up_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_DOWN,
        .actions = s_arrows_swipe_down_actions,
        .action_count = ARRAY_SIZE(s_arrows_swipe_down_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_LEFT,
        .actions = s_arrows_swipe_left_actions,
        .action_count = ARRAY_SIZE(s_arrows_swipe_left_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_RIGHT,
        .actions = s_arrows_swipe_right_actions,
        .action_count = ARRAY_SIZE(s_arrows_swipe_right_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_DOUBLE_TAP,
        .actions = s_arrows_double_tap_actions,
        .action_count = ARRAY_SIZE(s_arrows_double_tap_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_LONG_PRESS,
        .actions = s_arrows_long_press_actions,
        .action_count = ARRAY_SIZE(s_arrows_long_press_actions),
    },
};

static const mode_action_t s_navigation_swipe_up_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_UP_ARROW),
    ACTION_HINT("Up"),
};

static const mode_action_t s_navigation_swipe_down_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_DOWN_ARROW),
    ACTION_HINT("Down"),
};

static const mode_action_t s_navigation_swipe_left_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_LEFT_ARROW),
    ACTION_HINT("Left"),
};

static const mode_action_t s_navigation_swipe_right_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_RIGHT_ARROW),
    ACTION_HINT("Right"),
};

static const mode_action_t s_navigation_tap_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_ESCAPE),
    ACTION_HINT("Navigation mode"),
};

static const mode_action_t s_navigation_double_tap_actions[] = {
    ACTION_KEY_TAP(MODE_KEY_ESCAPE),
    ACTION_HINT("Cancel"),
};

static const mode_binding_t s_navigation_bindings[] __attribute__((unused)) = {
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_UP,
        .actions = s_navigation_swipe_up_actions,
        .action_count = ARRAY_SIZE(s_navigation_swipe_up_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_DOWN,
        .actions = s_navigation_swipe_down_actions,
        .action_count = ARRAY_SIZE(s_navigation_swipe_down_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_LEFT,
        .actions = s_navigation_swipe_left_actions,
        .action_count = ARRAY_SIZE(s_navigation_swipe_left_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_SWIPE_RIGHT,
        .actions = s_navigation_swipe_right_actions,
        .action_count = ARRAY_SIZE(s_navigation_swipe_right_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_TAP,
        .actions = s_navigation_tap_actions,
        .action_count = ARRAY_SIZE(s_navigation_tap_actions),
    },
    {
        .input = MODE_INPUT_TOUCH,
        .trigger = MODE_TRIGGER_DOUBLE_TAP,
        .actions = s_navigation_double_tap_actions,
        .action_count = ARRAY_SIZE(s_navigation_double_tap_actions),
    },
};

static const mode_definition_t s_failsafe_modes[] = {
    {
        .id = MODE_ID_CURSOR,
        .name = "cursor",
        .label = "Cursor",
        .cycle_order = 0,
        .bindings = s_cursor_bindings,
        .binding_count = ARRAY_SIZE(s_cursor_bindings),
    },
    {
        .id = MODE_ID_MEDIA,
        .name = "media",
        .label = "Media",
        .cycle_order = 1,
        .bindings = s_media_bindings,
        .binding_count = ARRAY_SIZE(s_media_bindings),
    },
    {
        .id = MODE_ID_ARROWS,
        .name = "arrows",
        .label = "Arrows",
        .cycle_order = 2,
        .bindings = s_arrows_bindings,
        .binding_count = ARRAY_SIZE(s_arrows_bindings),
    },
};

static const mode_config_t s_failsafe_mode_config = {
    .version = 1,
    .active_mode = MODE_ID_CURSOR,
    .defaults = {
        .touch = {
            .hold_ms = 400,
            .double_tap_ms = 350,
            .swipe_min_distance = 40,
        },
    },
    .wifi = {
        .sta = {
            .ssid = "YourNetworkName",
            .password = "YourPassword",
        },
        .ap = {
            .ssid = "walkey-talkey",
            .password = "secretKEY",
        },
        .hostname = "walkey-talkey",
        .local_url = "walkey-talkey.local",
    },
    .global_bindings = s_global_bindings,
    .global_binding_count = ARRAY_SIZE(s_global_bindings),
    .boot_mode = {
        .label = "Mode Control",
        .ui = {
            .title = "Swipe to switch mode",
            .subtitle = "Hold BOOT and swipe to change modes",
            .show_mode_list = true,
            .show_gesture_hints = true,
            .show_current_mode_card = true,
        },
        .bindings = s_boot_bindings,
        .binding_count = ARRAY_SIZE(s_boot_bindings),
    },
    .modes = s_failsafe_modes,
    .mode_count = ARRAY_SIZE(s_failsafe_modes),
};

static const mode_config_t *s_mode_config = &s_failsafe_mode_config;
static mode_config_t *s_loaded_mode_config = NULL;
static bool s_mode_config_initialized = false;
static mode_config_source_t s_mode_config_source = MODE_CONFIG_SOURCE_FAILSAFE;
static mode_config_storage_error_t s_last_storage_error = {0};
static bool s_last_mount_allowed_format = false;

static void mode_config_clear_storage_error(void)
{
    s_last_storage_error = (mode_config_storage_error_t){0};
    s_last_mount_allowed_format = false;
}

static void mode_config_set_storage_error(mode_config_storage_stage_t stage,
                                          bool format_attempted,
                                          int errno_value,
                                          const char *esp_error,
                                          const char *message)
{
    s_last_storage_error = (mode_config_storage_error_t){
        .stage = stage,
        .format_attempted = format_attempted,
        .errno_value = errno_value,
    };

    (void)snprintf(s_last_storage_error.path,
                   sizeof(s_last_storage_error.path),
                   "%s",
                   MODE_CONFIG_EXTERNAL_PATH);
    (void)snprintf(s_last_storage_error.partition_label,
                   sizeof(s_last_storage_error.partition_label),
                   "%s",
                   MODE_CONFIG_SPIFFS_PARTITION_LABEL);

    if (esp_error != NULL) {
        (void)snprintf(s_last_storage_error.esp_error,
                       sizeof(s_last_storage_error.esp_error),
                       "%s",
                       esp_error);
    }

    if (errno_value != 0) {
        const char *errno_text = strerror(errno_value);
        (void)snprintf(s_last_storage_error.errno_message,
                       sizeof(s_last_storage_error.errno_message),
                       "%s",
                       (errno_text != NULL) ? errno_text : "Unknown errno");
    }

    if (message != NULL) {
        (void)snprintf(s_last_storage_error.message,
                       sizeof(s_last_storage_error.message),
                       "%s",
                       message);
    }
}

static void mode_config_log_parse_error(const char *source, const mode_json_error_t *error)
{
#if MODE_CONFIG_HAS_ESP_SPIFFS
    ESP_LOGW(TAG,
             "Mode JSON from %s failed at offset %u: %s",
             source,
             (unsigned)((error != NULL) ? error->offset : 0),
             ((error != NULL) && (error->message[0] != '\0')) ? error->message : "unknown error");
#else
    (void)source;
    (void)error;
#endif
}

static bool mode_config_read_file(const char *path, char **out_text)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }

    long length = ftell(file);
    if (length < 0) {
        fclose(file);
        return false;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }

    char *buffer = (char *)calloc((size_t)length + 1, 1);
    if (buffer == NULL) {
        fclose(file);
        return false;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)length, file);
    fclose(file);
    if (bytes_read != (size_t)length) {
        free(buffer);
        return false;
    }

    buffer[length] = '\0';
    *out_text = buffer;
    return true;
}

static bool mode_config_write_file(const char *path, const char *text)
{
    if ((path == NULL) || (text == NULL)) {
        mode_config_set_storage_error(MODE_CONFIG_STORAGE_STAGE_NONE,
                                      s_last_mount_allowed_format,
                                      EINVAL,
                                      NULL,
                                      "The firmware was asked to write the external config with a missing file path or JSON payload.");
        return false;
    }

    errno = 0;
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        int open_errno = errno;
        char message[256];
        (void)snprintf(message,
                       sizeof(message),
                       "The firmware could not open `%s` for writing. This usually means the SPIFFS storage partition is missing, not mounted, still unformatted, or the filesystem is corrupted.",
                       path);
        mode_config_set_storage_error(MODE_CONFIG_STORAGE_STAGE_OPEN,
                                      s_last_mount_allowed_format,
                                      open_errno,
                                      NULL,
                                      message);
        return false;
    }

    size_t length = strlen(text);
    size_t bytes_written = fwrite(text, 1, length, file);
    if (bytes_written != length) {
        int write_errno = errno;
        char message[256];
        (void)snprintf(message,
                       sizeof(message),
                       "The firmware started writing `%s` but only wrote %u of %u bytes. The SPIFFS storage partition may be full, damaged, or otherwise not accepting writes.",
                       path,
                       (unsigned)bytes_written,
                       (unsigned)length);
        mode_config_set_storage_error(MODE_CONFIG_STORAGE_STAGE_WRITE,
                                      s_last_mount_allowed_format,
                                      write_errno,
                                      NULL,
                                      message);
        fclose(file);
        return false;
    }

    if (fflush(file) != 0) {
        int flush_errno = errno;
        char message[256];
        (void)snprintf(message,
                       sizeof(message),
                       "The firmware wrote `%s` but failed while flushing the file to SPI flash. The SPIFFS partition may need to be reformatted or reflashed.",
                       path);
        mode_config_set_storage_error(MODE_CONFIG_STORAGE_STAGE_FLUSH,
                                      s_last_mount_allowed_format,
                                      flush_errno,
                                      NULL,
                                      message);
        fclose(file);
        return false;
    }

    fclose(file);
    return true;
}

#if MODE_CONFIG_HAS_ESP_SPIFFS
static bool mode_config_mount_spiffs(bool allow_format)
{
    static bool mounted = false;
    s_last_mount_allowed_format = allow_format;
    if (mounted) {
        return true;
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = MODE_CONFIG_SPIFFS_MOUNT_POINT,
        .partition_label = MODE_CONFIG_SPIFFS_PARTITION_LABEL,
        .max_files = 4,
        .format_if_mount_failed = allow_format,
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err == ESP_ERR_INVALID_STATE) {
        mounted = true;
        ESP_LOGI(TAG, "SPIFFS already mounted");
        return true;
    }

    if (err != ESP_OK) {
        char message[256];
        (void)snprintf(message,
                       sizeof(message),
                       "The firmware could not mount the `%s` SPIFFS partition at `%s` before accessing `%s`. The partition may be blank, corrupt, missing from the flashed partition table, or require formatting.",
                       MODE_CONFIG_SPIFFS_PARTITION_LABEL,
                       MODE_CONFIG_SPIFFS_MOUNT_POINT,
                       MODE_CONFIG_EXTERNAL_PATH);
        mode_config_set_storage_error(MODE_CONFIG_STORAGE_STAGE_MOUNT,
                                      allow_format,
                                      0,
                                      esp_err_to_name(err),
                                      message);
        ESP_LOGW(TAG,
                 "SPIFFS mount failed%s: %s",
                 allow_format ? " (with format enabled)" : "",
                 esp_err_to_name(err));
        return false;
    }

    mounted = true;
    return true;
}
#endif

static bool mode_config_try_load_external(mode_config_t **out_config)
{
#if MODE_CONFIG_HAS_ESP_SPIFFS
    if (!mode_config_mount_spiffs(false)) {
        return false;
    }
#endif

    char *json_text = NULL;
    if (!mode_config_read_file(MODE_CONFIG_EXTERNAL_PATH, &json_text)) {
        return false;
    }

    mode_json_error_t error = {0};
    bool ok = mode_json_load_from_string(json_text, out_config, &error);
    free(json_text);
    if (!ok) {
        mode_config_log_parse_error(MODE_CONFIG_EXTERNAL_PATH, &error);
    }

    return ok;
}

static bool mode_config_load_builtin(mode_config_t **out_config)
{
    mode_json_error_t error = {0};
    bool ok = mode_json_load_from_string(s_builtin_mode_config_json, out_config, &error);
    if (!ok) {
        mode_config_log_parse_error("built-in fallback", &error);
    }
    return ok;
}

bool mode_config_init(void)
{
    if (s_mode_config_initialized) {
        return true;
    }

    mode_config_clear_storage_error();
    s_mode_config = &s_failsafe_mode_config;
    s_loaded_mode_config = NULL;
    s_mode_config_source = MODE_CONFIG_SOURCE_FAILSAFE;

    if (!mode_config_try_load_external(&s_loaded_mode_config)) {
        if (mode_config_load_builtin(&s_loaded_mode_config)) {
            s_mode_config_source = MODE_CONFIG_SOURCE_BUILTIN;
        }
    } else {
        s_mode_config_source = MODE_CONFIG_SOURCE_EXTERNAL;
    }

    if (s_loaded_mode_config != NULL) {
        s_mode_config = s_loaded_mode_config;
    }

    s_mode_config_initialized = true;
    return true;
}

void mode_config_reset(void)
{
    if (s_loaded_mode_config != NULL) {
        mode_json_free_config(s_loaded_mode_config);
        s_loaded_mode_config = NULL;
    }

    s_mode_config = &s_failsafe_mode_config;
    s_mode_config_source = MODE_CONFIG_SOURCE_FAILSAFE;
    s_mode_config_initialized = false;
}

const mode_config_t *mode_config_get(void)
{
    (void)mode_config_init();
    return s_mode_config;
}

bool mode_config_reload(void)
{
    mode_config_reset();
    return mode_config_init();
}

mode_config_source_t mode_config_get_source(void)
{
    return s_mode_config_source;
}

const char *mode_config_source_name(mode_config_source_t source)
{
    switch (source) {
    case MODE_CONFIG_SOURCE_EXTERNAL:
        return "external";
    case MODE_CONFIG_SOURCE_BUILTIN:
        return "builtin";
    case MODE_CONFIG_SOURCE_FAILSAFE:
    default:
        return "failsafe";
    }
}

const mode_definition_t *mode_config_find_mode(mode_id_t mode)
{
    const mode_config_t *config = mode_config_get();
    for (size_t i = 0; i < config->mode_count; ++i) {
        if (config->modes[i].id == mode) {
            return &config->modes[i];
        }
    }

    return NULL;
}

const char *mode_config_mode_label(mode_id_t mode)
{
    const mode_definition_t *mode_definition = mode_config_find_mode(mode);
    if (mode_definition != NULL) {
        return mode_definition->label;
    }

    return "Unknown";
}

const char *mode_config_builtin_json(void)
{
    return s_builtin_mode_config_json;
}

bool mode_config_read_active_json(char **out_text, mode_config_source_t *out_source)
{
    if (out_text == NULL) {
        return false;
    }

    *out_text = NULL;
    mode_config_source_t source = mode_config_get_source();
    if (out_source != NULL) {
        *out_source = source;
    }

    switch (source) {
    case MODE_CONFIG_SOURCE_EXTERNAL:
#if MODE_CONFIG_HAS_ESP_SPIFFS
        if (!mode_config_mount_spiffs(false)) {
            return false;
        }
#endif
        return mode_config_read_file(MODE_CONFIG_EXTERNAL_PATH, out_text);

    case MODE_CONFIG_SOURCE_BUILTIN: {
        size_t length = strlen(s_builtin_mode_config_json) + 1;
        char *buffer = (char *)malloc(length);
        if (buffer == NULL) {
            return false;
        }
        memcpy(buffer, s_builtin_mode_config_json, length);
        *out_text = buffer;
        return true;
    }

    case MODE_CONFIG_SOURCE_FAILSAFE:
    default:
        *out_text = mode_json_export_canonical_string(mode_config_get());
        return *out_text != NULL;
    }
}

bool mode_config_write_external_json(const char *json_text)
{
    mode_config_clear_storage_error();

#if MODE_CONFIG_HAS_ESP_SPIFFS
    if (!mode_config_mount_spiffs(true)) {
        return false;
    }
#endif

    return mode_config_write_file(MODE_CONFIG_EXTERNAL_PATH, json_text);
}

const mode_config_storage_error_t *mode_config_last_storage_error(void)
{
    return &s_last_storage_error;
}
