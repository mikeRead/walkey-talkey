#include "mode_hid_tokens.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool mode_hid_equals_ignore_case(const char *left, const char *right)
{
    if ((left == NULL) || (right == NULL)) {
        return false;
    }

    while ((*left != '\0') && (*right != '\0')) {
        if (toupper((unsigned char)*left) != toupper((unsigned char)*right)) {
            return false;
        }
        left++;
        right++;
    }

    return (*left == '\0') && (*right == '\0');
}

static const mode_hid_modifier_token_t s_modifier_tokens[] = {
    { "CTRL", "CTRL", MODE_HID_MODIFIER_LEFT_CTRL },
    { "CONTROL", "CTRL", MODE_HID_MODIFIER_LEFT_CTRL },
    { "LCTRL", "CTRL", MODE_HID_MODIFIER_LEFT_CTRL },
    { "LEFT_CTRL", "CTRL", MODE_HID_MODIFIER_LEFT_CTRL },
    { "RCTRL", "RIGHT_CTRL", MODE_HID_MODIFIER_RIGHT_CTRL },
    { "RIGHT_CTRL", "RIGHT_CTRL", MODE_HID_MODIFIER_RIGHT_CTRL },
    { "SHIFT", "SHIFT", MODE_HID_MODIFIER_LEFT_SHIFT },
    { "LSHIFT", "SHIFT", MODE_HID_MODIFIER_LEFT_SHIFT },
    { "LEFT_SHIFT", "SHIFT", MODE_HID_MODIFIER_LEFT_SHIFT },
    { "RSHIFT", "RIGHT_SHIFT", MODE_HID_MODIFIER_RIGHT_SHIFT },
    { "RIGHT_SHIFT", "RIGHT_SHIFT", MODE_HID_MODIFIER_RIGHT_SHIFT },
    { "ALT", "ALT", MODE_HID_MODIFIER_LEFT_ALT },
    { "LALT", "ALT", MODE_HID_MODIFIER_LEFT_ALT },
    { "LEFT_ALT", "ALT", MODE_HID_MODIFIER_LEFT_ALT },
    { "RALT", "RIGHT_ALT", MODE_HID_MODIFIER_RIGHT_ALT },
    { "RIGHT_ALT", "RIGHT_ALT", MODE_HID_MODIFIER_RIGHT_ALT },
    { "OPTION", "ALT", MODE_HID_MODIFIER_LEFT_ALT },
    { "GUI", "GUI", MODE_HID_MODIFIER_LEFT_GUI },
    { "WIN", "GUI", MODE_HID_MODIFIER_LEFT_GUI },
    { "META", "GUI", MODE_HID_MODIFIER_LEFT_GUI },
    { "CMD", "GUI", MODE_HID_MODIFIER_LEFT_GUI },
    { "LGUI", "GUI", MODE_HID_MODIFIER_LEFT_GUI },
    { "LEFT_GUI", "GUI", MODE_HID_MODIFIER_LEFT_GUI },
    { "RGUI", "RIGHT_GUI", MODE_HID_MODIFIER_RIGHT_GUI },
    { "RIGHT_GUI", "RIGHT_GUI", MODE_HID_MODIFIER_RIGHT_GUI },
};

static const mode_hid_token_t s_special_tokens[] = {
    { "NONE", "NONE", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_NONE } },
    { "ENTER", "ENTER", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_ENTER } },
    { "RETURN", "ENTER", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_ENTER } },
    { "ESC", "ESC", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_ESCAPE } },
    { "ESCAPE", "ESC", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_ESCAPE } },
    { "BACKSPACE", "BACKSPACE", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_BACKSPACE } },
    { "SPACE", "SPACE", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_SPACE } },
    { "TAB", "TAB", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_TAB } },
    { "DELETE", "DELETE", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_DELETE } },
    { "DEL", "DELETE", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_DELETE } },
    { "PAGE_UP", "PAGE_UP", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_PAGE_UP } },
    { "PAGE_DOWN", "PAGE_DOWN", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_PAGE_DOWN } },
    { "LEFT_ARROW", "LEFT_ARROW", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_LEFT_ARROW } },
    { "RIGHT_ARROW", "RIGHT_ARROW", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_RIGHT_ARROW } },
    { "UP_ARROW", "UP_ARROW", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_UP_ARROW } },
    { "DOWN_ARROW", "DOWN_ARROW", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_DOWN_ARROW } },
    { "APPLICATION", "APPLICATION", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_APPLICATION } },
    { "MENU", "APPLICATION", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_APPLICATION } },
    { ".", "PERIOD", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_PERIOD } },
    { "PERIOD", "PERIOD", { MODE_HID_REPORT_KIND_KEYBOARD, MODE_HID_USAGE_PAGE_KEYBOARD, 0, MODE_KEY_PERIOD } },
    { "PLAY_PAUSE", "PLAY_PAUSE", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_PLAY_PAUSE } },
    { "MEDIA_PLAY_PAUSE", "PLAY_PAUSE", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_PLAY_PAUSE } },
    { "NEXT_TRACK", "MEDIA_NEXT_TRACK", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_SCAN_NEXT_TRACK } },
    { "MEDIA_NEXT_TRACK", "MEDIA_NEXT_TRACK", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_SCAN_NEXT_TRACK } },
    { "PREV_TRACK", "MEDIA_PREV_TRACK", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_SCAN_PREVIOUS_TRACK } },
    { "MEDIA_PREV_TRACK", "MEDIA_PREV_TRACK", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_SCAN_PREVIOUS_TRACK } },
    { "STOP", "STOP", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_STOP } },
    { "MUTE", "MUTE", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_MUTE } },
    { "VOLUME_UP", "VOLUME_UP", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_VOLUME_INCREMENT } },
    { "VOLUME_DOWN", "VOLUME_DOWN", { MODE_HID_REPORT_KIND_CONSUMER, MODE_HID_USAGE_PAGE_CONSUMER, 0, MODE_CONSUMER_USAGE_VOLUME_DECREMENT } },
    { "SYSTEM_POWER_DOWN", "SYSTEM_POWER_DOWN", { MODE_HID_REPORT_KIND_SYSTEM, MODE_HID_USAGE_PAGE_GENERIC_DESKTOP, 0, MODE_SYSTEM_USAGE_POWER_DOWN } },
    { "SYSTEM_SLEEP", "SYSTEM_SLEEP", { MODE_HID_REPORT_KIND_SYSTEM, MODE_HID_USAGE_PAGE_GENERIC_DESKTOP, 0, MODE_SYSTEM_USAGE_SLEEP } },
    { "SYSTEM_WAKE_UP", "SYSTEM_WAKE_UP", { MODE_HID_REPORT_KIND_SYSTEM, MODE_HID_USAGE_PAGE_GENERIC_DESKTOP, 0, MODE_SYSTEM_USAGE_WAKE_UP } },
};

static bool mode_hid_usage_matches_token(const mode_hid_usage_t *usage, const mode_hid_token_t *token)
{
    if ((usage == NULL) || (token == NULL)) {
        return false;
    }

    return (usage->report_kind == token->usage.report_kind) &&
           (usage->usage_page == token->usage.usage_page) &&
           (usage->usage_id == token->usage.usage_id);
}

bool mode_hid_usage_is_keyboard(const mode_hid_usage_t *usage)
{
    return (usage != NULL) && (usage->report_kind == MODE_HID_REPORT_KIND_KEYBOARD);
}

bool mode_hid_usage_is_consumer(const mode_hid_usage_t *usage)
{
    return (usage != NULL) && (usage->report_kind == MODE_HID_REPORT_KIND_CONSUMER);
}

bool mode_hid_usage_is_system(const mode_hid_usage_t *usage)
{
    return (usage != NULL) && (usage->report_kind == MODE_HID_REPORT_KIND_SYSTEM);
}

size_t mode_hid_modifier_token_count(void)
{
    return sizeof(s_modifier_tokens) / sizeof(s_modifier_tokens[0]);
}

const mode_hid_modifier_token_t *mode_hid_modifier_token_at(size_t index)
{
    if (index >= mode_hid_modifier_token_count()) {
        return NULL;
    }

    return &s_modifier_tokens[index];
}

size_t mode_hid_usage_token_count(void)
{
    return sizeof(s_special_tokens) / sizeof(s_special_tokens[0]);
}

const mode_hid_token_t *mode_hid_usage_token_at(size_t index)
{
    if (index >= mode_hid_usage_token_count()) {
        return NULL;
    }

    return &s_special_tokens[index];
}

const char *mode_hid_canonical_modifier_token(mode_hid_modifier_t modifier_mask)
{
    for (size_t i = 0; i < mode_hid_modifier_token_count(); ++i) {
        if (s_modifier_tokens[i].modifier_mask == modifier_mask) {
            return s_modifier_tokens[i].canonical_token;
        }
    }

    return NULL;
}

static bool mode_hid_usage_to_algorithmic_token(const mode_hid_usage_t *usage,
                                                char *buffer,
                                                size_t buffer_size)
{
    if ((usage == NULL) || (buffer == NULL) || (buffer_size == 0)) {
        return false;
    }

    if (!mode_hid_usage_is_keyboard(usage)) {
        return false;
    }

    if ((usage->usage_id >= MODE_KEY_A) && (usage->usage_id <= MODE_KEY_Z)) {
        if (buffer_size < 2) {
            return false;
        }

        buffer[0] = (char)('A' + (usage->usage_id - MODE_KEY_A));
        buffer[1] = '\0';
        return true;
    }

    if ((usage->usage_id >= MODE_KEY_1) && (usage->usage_id <= 0x26)) {
        if (buffer_size < 2) {
            return false;
        }

        buffer[0] = (usage->usage_id == 0x27)
            ? '0'
            : (char)('1' + (usage->usage_id - MODE_KEY_1));
        buffer[1] = '\0';
        return true;
    }

    if (usage->usage_id == 0x27) {
        if (buffer_size < 2) {
            return false;
        }

        buffer[0] = '0';
        buffer[1] = '\0';
        return true;
    }

    int function_number = 0;
    if ((usage->usage_id >= 0x3A) && (usage->usage_id <= 0x45)) {
        function_number = 1 + (int)(usage->usage_id - 0x3A);
    } else if ((usage->usage_id >= 0x68) && (usage->usage_id <= 0x73)) {
        function_number = 13 + (int)(usage->usage_id - 0x68);
    }

    if (function_number > 0) {
        (void)snprintf(buffer, buffer_size, "F%d", function_number);
        return true;
    }

    return false;
}

bool mode_hid_usage_to_canonical_token(const mode_hid_usage_t *usage, char *buffer, size_t buffer_size)
{
    if ((usage == NULL) || (buffer == NULL) || (buffer_size == 0)) {
        return false;
    }

    for (size_t i = 0; i < mode_hid_usage_token_count(); ++i) {
        if (!mode_hid_usage_matches_token(usage, &s_special_tokens[i])) {
            continue;
        }

        (void)snprintf(buffer, buffer_size, "%s", s_special_tokens[i].canonical_token);
        return true;
    }

    return mode_hid_usage_to_algorithmic_token(usage, buffer, buffer_size);
}

bool mode_hid_parse_modifier_token(const char *text, mode_hid_modifier_t *modifier_mask)
{
    if ((text == NULL) || (modifier_mask == NULL)) {
        return false;
    }

    for (size_t i = 0; i < (sizeof(s_modifier_tokens) / sizeof(s_modifier_tokens[0])); ++i) {
        if (mode_hid_equals_ignore_case(text, s_modifier_tokens[i].token)) {
            *modifier_mask = s_modifier_tokens[i].modifier_mask;
            return true;
        }
    }

    return false;
}

static bool mode_hid_parse_letter_token(const char *text, mode_hid_usage_t *usage)
{
    if ((text == NULL) || (usage == NULL) || (text[0] == '\0') || (text[1] != '\0')) {
        return false;
    }

    if (!isalpha((unsigned char)text[0])) {
        return false;
    }

    char upper = (char)toupper((unsigned char)text[0]);
    usage->report_kind = MODE_HID_REPORT_KIND_KEYBOARD;
    usage->usage_page = MODE_HID_USAGE_PAGE_KEYBOARD;
    usage->modifiers = MODE_HID_MODIFIER_NONE;
    usage->usage_id = (uint16_t)(0x04 + (upper - 'A'));
    return true;
}

static bool mode_hid_parse_digit_token(const char *text, mode_hid_usage_t *usage)
{
    if ((text == NULL) || (usage == NULL) || (text[0] == '\0') || (text[1] != '\0')) {
        return false;
    }

    if (!isdigit((unsigned char)text[0])) {
        return false;
    }

    usage->report_kind = MODE_HID_REPORT_KIND_KEYBOARD;
    usage->usage_page = MODE_HID_USAGE_PAGE_KEYBOARD;
    usage->modifiers = MODE_HID_MODIFIER_NONE;
    usage->usage_id = (text[0] == '0') ? 0x27 : (uint16_t)(0x1E + (text[0] - '1'));
    return true;
}

static bool mode_hid_parse_function_token(const char *text, mode_hid_usage_t *usage)
{
    if ((text == NULL) || (usage == NULL) || (toupper((unsigned char)text[0]) != 'F')) {
        return false;
    }

    int function_number = 0;
    if (sscanf(text + 1, "%d", &function_number) != 1) {
        return false;
    }

    if ((function_number < 1) || (function_number > 24)) {
        return false;
    }

    usage->report_kind = MODE_HID_REPORT_KIND_KEYBOARD;
    usage->usage_page = MODE_HID_USAGE_PAGE_KEYBOARD;
    usage->modifiers = MODE_HID_MODIFIER_NONE;
    usage->usage_id = (function_number <= 12)
        ? (uint16_t)(0x3A + (function_number - 1))
        : (uint16_t)(0x68 + (function_number - 13));
    return true;
}

bool mode_hid_parse_usage_token(const char *text, mode_hid_usage_t *usage)
{
    if ((text == NULL) || (usage == NULL)) {
        return false;
    }

    for (size_t i = 0; i < mode_hid_usage_token_count(); ++i) {
        if (mode_hid_equals_ignore_case(text, s_special_tokens[i].token)) {
            *usage = s_special_tokens[i].usage;
            return true;
        }
    }

    return mode_hid_parse_letter_token(text, usage) ||
           mode_hid_parse_digit_token(text, usage) ||
           mode_hid_parse_function_token(text, usage);
}

bool mode_hid_parse_shortcut_string(const char *text, mode_hid_usage_t *usage)
{
    if ((text == NULL) || (usage == NULL)) {
        return false;
    }

    const char *cursor = text;
    const char *plus = strchr(cursor, '+');
    if (plus == NULL) {
        return false;
    }

    mode_hid_modifier_t modifiers = MODE_HID_MODIFIER_NONE;
    while (plus != NULL) {
        size_t token_length = (size_t)(plus - cursor);
        if ((token_length == 0) || (token_length >= 32)) {
            return false;
        }

        char token[32];
        memcpy(token, cursor, token_length);
        token[token_length] = '\0';

        mode_hid_modifier_t modifier = MODE_HID_MODIFIER_NONE;
        if (!mode_hid_parse_modifier_token(token, &modifier)) {
            return false;
        }

        modifiers |= modifier;
        cursor = plus + 1;
        plus = strchr(cursor, '+');
    }

    if (!mode_hid_parse_usage_token(cursor, usage)) {
        return false;
    }

    if (!mode_hid_usage_is_keyboard(usage)) {
        return false;
    }

    usage->modifiers = modifiers;
    return true;
}

bool mode_hid_parse_report_kind(const char *text, mode_hid_report_kind_t *report_kind)
{
    if ((text == NULL) || (report_kind == NULL)) {
        return false;
    }

    if (mode_hid_equals_ignore_case(text, "keyboard")) {
        *report_kind = MODE_HID_REPORT_KIND_KEYBOARD;
        return true;
    }
    if (mode_hid_equals_ignore_case(text, "consumer")) {
        *report_kind = MODE_HID_REPORT_KIND_CONSUMER;
        return true;
    }
    if (mode_hid_equals_ignore_case(text, "system")) {
        *report_kind = MODE_HID_REPORT_KIND_SYSTEM;
        return true;
    }

    return false;
}
