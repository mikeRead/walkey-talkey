#include "mode_json_loader.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mode_config.h"
#include "mode_hid_tokens.h"

#define MODE_JSON_DEFAULT_HOLD_MS 400
#define MODE_JSON_DEFAULT_DOUBLE_TAP_MS 350
#define MODE_JSON_DEFAULT_SWIPE_MIN_DISTANCE 40
#define MODE_JSON_DEFAULT_WIFI_STA_SSID "YourNetworkName"
#define MODE_JSON_DEFAULT_WIFI_STA_PASSWORD "YourPassword"
#define MODE_JSON_DEFAULT_WIFI_AP_SSID "walkey-talkey"
#define MODE_JSON_DEFAULT_WIFI_AP_PASSWORD "secretKEY"
#define MODE_JSON_DEFAULT_WIFI_HOSTNAME "walkey-talkey"
#define MODE_JSON_DEFAULT_WIFI_LOCAL_URL "walkey-talkey.local"

typedef enum {
    MODE_JSON_VALUE_NULL = 0,
    MODE_JSON_VALUE_BOOL,
    MODE_JSON_VALUE_NUMBER,
    MODE_JSON_VALUE_STRING,
    MODE_JSON_VALUE_ARRAY,
    MODE_JSON_VALUE_OBJECT,
} mode_json_value_type_t;

typedef struct mode_json_value mode_json_value_t;

typedef struct {
    char *key;
    mode_json_value_t *value;
} mode_json_object_entry_t;

struct mode_json_value {
    mode_json_value_type_t type;
    union {
        bool boolean;
        long long number;
        char *string;
        struct {
            mode_json_value_t **items;
            size_t count;
        } array;
        struct {
            mode_json_object_entry_t *entries;
            size_t count;
        } object;
    } data;
};

typedef struct {
    const char *start;
    const char *cursor;
    mode_json_error_t *error;
} mode_json_parser_t;

typedef struct {
    mode_action_t *action;
    char *mode_name;
} mode_json_pending_mode_ref_t;

typedef struct {
    mode_json_pending_mode_ref_t *items;
    size_t count;
} mode_json_pending_mode_refs_t;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} mode_json_string_builder_t;

static void mode_json_free_actions(mode_action_t *actions, size_t action_count);

static void mode_json_set_error(mode_json_error_t *error,
                                size_t offset,
                                const char *path,
                                const char *message,
                                mode_json_error_code_t code)
{
    if (error == NULL) {
        return;
    }

    error->offset = offset;
    error->code = code;
    if (path != NULL) {
        (void)snprintf(error->path, sizeof(error->path), "%s", path);
    } else {
        error->path[0] = '\0';
    }
    (void)snprintf(error->message, sizeof(error->message), "%s", message);
}

static bool mode_json_fail(mode_json_parser_t *parser, const char *message)
{
    size_t offset = 0;
    if ((parser != NULL) && (parser->cursor != NULL) && (parser->start != NULL)) {
        offset = (size_t)(parser->cursor - parser->start);
    }

    mode_json_set_error((parser != NULL) ? parser->error : NULL,
                        offset,
                        "",
                        message,
                        MODE_JSON_ERROR_PARSE);
    return false;
}

static bool mode_json_fail_compile_at_code(mode_json_error_t *error,
                                           const char *path,
                                           const char *message,
                                           mode_json_error_code_t code)
{
    mode_json_set_error(error, 0, path, message, code);
    return false;
}

static bool mode_json_fail_compile_at(mode_json_error_t *error, const char *path, const char *message)
{
    return mode_json_fail_compile_at_code(error, path, message, MODE_JSON_ERROR_INVALID_VALUE);
}

static bool mode_json_fail_compile(mode_json_error_t *error, const char *message)
{
    return mode_json_fail_compile_at(error, "", message);
}

static bool mode_json_is_ws(char ch)
{
    return (ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n');
}

static void mode_json_skip_ws(mode_json_parser_t *parser)
{
    while ((parser != NULL) && (*parser->cursor != '\0') && mode_json_is_ws(*parser->cursor)) {
        parser->cursor++;
    }
}

static bool mode_json_match_literal(mode_json_parser_t *parser, const char *literal)
{
    size_t length = strlen(literal);
    if (strncmp(parser->cursor, literal, length) != 0) {
        return false;
    }

    parser->cursor += length;
    return true;
}

static void *mode_json_calloc(size_t count, size_t size)
{
    if ((count == 0) || (size == 0)) {
        return NULL;
    }

    return calloc(count, size);
}

static char *mode_json_strdup(const char *text)
{
    if (text == NULL) {
        return NULL;
    }

    size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

static void mode_json_free_value(mode_json_value_t *value)
{
    if (value == NULL) {
        return;
    }

    switch (value->type) {
    case MODE_JSON_VALUE_STRING:
        free(value->data.string);
        break;

    case MODE_JSON_VALUE_ARRAY:
        for (size_t i = 0; i < value->data.array.count; ++i) {
            mode_json_free_value(value->data.array.items[i]);
        }
        free(value->data.array.items);
        break;

    case MODE_JSON_VALUE_OBJECT:
        for (size_t i = 0; i < value->data.object.count; ++i) {
            free(value->data.object.entries[i].key);
            mode_json_free_value(value->data.object.entries[i].value);
        }
        free(value->data.object.entries);
        break;

    case MODE_JSON_VALUE_NULL:
    case MODE_JSON_VALUE_BOOL:
    case MODE_JSON_VALUE_NUMBER:
    default:
        break;
    }

    free(value);
}

static mode_json_value_t *mode_json_new_value(mode_json_value_type_t type)
{
    mode_json_value_t *value = (mode_json_value_t *)mode_json_calloc(1, sizeof(*value));
    if (value == NULL) {
        return NULL;
    }

    value->type = type;
    return value;
}

static bool mode_json_append_array_item(mode_json_value_t *array_value, mode_json_value_t *item)
{
    size_t new_count = array_value->data.array.count + 1;
    mode_json_value_t **new_items = (mode_json_value_t **)realloc(array_value->data.array.items,
                                                                  new_count * sizeof(*new_items));
    if (new_items == NULL) {
        return false;
    }

    array_value->data.array.items = new_items;
    array_value->data.array.items[array_value->data.array.count] = item;
    array_value->data.array.count = new_count;
    return true;
}

static bool mode_json_append_object_entry(mode_json_value_t *object_value,
                                          char *key,
                                          mode_json_value_t *item)
{
    size_t new_count = object_value->data.object.count + 1;
    mode_json_object_entry_t *new_entries = (mode_json_object_entry_t *)realloc(
        object_value->data.object.entries,
        new_count * sizeof(*new_entries));
    if (new_entries == NULL) {
        return false;
    }

    object_value->data.object.entries = new_entries;
    object_value->data.object.entries[object_value->data.object.count].key = key;
    object_value->data.object.entries[object_value->data.object.count].value = item;
    object_value->data.object.count = new_count;
    return true;
}

static bool mode_json_object_contains_key(const mode_json_value_t *object_value, const char *key)
{
    if ((object_value == NULL) || (object_value->type != MODE_JSON_VALUE_OBJECT) || (key == NULL)) {
        return false;
    }

    for (size_t i = 0; i < object_value->data.object.count; ++i) {
        if (strcmp(object_value->data.object.entries[i].key, key) == 0) {
            return true;
        }
    }

    return false;
}

static bool mode_json_buffer_append(char **buffer, size_t *length, size_t *capacity, char ch)
{
    if ((*length + 1) >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 32 : (*capacity * 2);
        char *new_buffer = (char *)realloc(*buffer, new_capacity);
        if (new_buffer == NULL) {
            return false;
        }

        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    (*buffer)[(*length)++] = ch;
    (*buffer)[*length] = '\0';
    return true;
}

static bool mode_json_builder_append_char(mode_json_string_builder_t *builder, char ch)
{
    return mode_json_buffer_append(&builder->data, &builder->length, &builder->capacity, ch);
}

static bool mode_json_builder_append_text(mode_json_string_builder_t *builder, const char *text)
{
    if ((builder == NULL) || (text == NULL)) {
        return false;
    }

    while (*text != '\0') {
        if (!mode_json_builder_append_char(builder, *text++)) {
            return false;
        }
    }

    return true;
}

static bool mode_json_builder_append_json_string(mode_json_string_builder_t *builder, const char *text)
{
    if ((builder == NULL) || (text == NULL) || !mode_json_builder_append_char(builder, '"')) {
        return false;
    }

    while (*text != '\0') {
        char ch = *text++;
        switch (ch) {
        case '"':
        case '\\':
            if (!mode_json_builder_append_char(builder, '\\') ||
                !mode_json_builder_append_char(builder, ch)) {
                return false;
            }
            break;
        case '\b':
            if (!mode_json_builder_append_text(builder, "\\b")) {
                return false;
            }
            break;
        case '\f':
            if (!mode_json_builder_append_text(builder, "\\f")) {
                return false;
            }
            break;
        case '\n':
            if (!mode_json_builder_append_text(builder, "\\n")) {
                return false;
            }
            break;
        case '\r':
            if (!mode_json_builder_append_text(builder, "\\r")) {
                return false;
            }
            break;
        case '\t':
            if (!mode_json_builder_append_text(builder, "\\t")) {
                return false;
            }
            break;
        default:
            if (((unsigned char)ch < 0x20) || ((unsigned char)ch > 0x7E)) {
                return false;
            }
            if (!mode_json_builder_append_char(builder, ch)) {
                return false;
            }
            break;
        }
    }

    return mode_json_builder_append_char(builder, '"');
}

static bool mode_json_builder_append_u32(mode_json_string_builder_t *builder, uint32_t value)
{
    char buffer[16];
    (void)snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)value);
    return mode_json_builder_append_text(builder, buffer);
}

static bool mode_json_builder_append_bool(mode_json_string_builder_t *builder, bool value)
{
    return mode_json_builder_append_text(builder, value ? "true" : "false");
}

static bool mode_json_parse_hex4(mode_json_parser_t *parser, unsigned int *value_out)
{
    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) {
        char ch = *parser->cursor++;
        value <<= 4;
        if ((ch >= '0') && (ch <= '9')) {
            value |= (unsigned int)(ch - '0');
        } else if ((ch >= 'a') && (ch <= 'f')) {
            value |= (unsigned int)(10 + ch - 'a');
        } else if ((ch >= 'A') && (ch <= 'F')) {
            value |= (unsigned int)(10 + ch - 'A');
        } else {
            return mode_json_fail(parser, "Invalid unicode escape");
        }
    }

    *value_out = value;
    return true;
}

static char *mode_json_parse_string_raw(mode_json_parser_t *parser)
{
    if (*parser->cursor != '"') {
        mode_json_fail(parser, "Expected string");
        return NULL;
    }

    parser->cursor++;

    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;

    while (*parser->cursor != '\0') {
        char ch = *parser->cursor++;
        if (ch == '"') {
            if (!mode_json_buffer_append(&buffer, &length, &capacity, '\0')) {
                free(buffer);
                mode_json_fail(parser, "Out of memory");
                return NULL;
            }
            buffer[length - 1] = '\0';
            return buffer;
        }

        if (ch == '\\') {
            char escaped = *parser->cursor++;
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                ch = escaped;
                break;
            case 'b':
                ch = '\b';
                break;
            case 'f':
                ch = '\f';
                break;
            case 'n':
                ch = '\n';
                break;
            case 'r':
                ch = '\r';
                break;
            case 't':
                ch = '\t';
                break;
            case 'u': {
                unsigned int codepoint = 0;
                if (!mode_json_parse_hex4(parser, &codepoint)) {
                    free(buffer);
                    return NULL;
                }
                if (codepoint > 0x7F) {
                    free(buffer);
                    mode_json_fail(parser, "Only ASCII unicode escapes are supported");
                    return NULL;
                }
                ch = (char)codepoint;
                break;
            }
            default:
                free(buffer);
                mode_json_fail(parser, "Invalid string escape");
                return NULL;
            }
        }

        if (!mode_json_buffer_append(&buffer, &length, &capacity, ch)) {
            free(buffer);
            mode_json_fail(parser, "Out of memory");
            return NULL;
        }
    }

    free(buffer);
    mode_json_fail(parser, "Unterminated string");
    return NULL;
}

static mode_json_value_t *mode_json_parse_value(mode_json_parser_t *parser);

static mode_json_value_t *mode_json_parse_number(mode_json_parser_t *parser)
{
    const char *start = parser->cursor;
    if (*parser->cursor == '-') {
        parser->cursor++;
    }

    if (!isdigit((unsigned char)*parser->cursor)) {
        mode_json_fail(parser, "Expected number");
        return NULL;
    }

    while (isdigit((unsigned char)*parser->cursor)) {
        parser->cursor++;
    }

    if ((*parser->cursor == '.') || (*parser->cursor == 'e') || (*parser->cursor == 'E')) {
        mode_json_fail(parser, "Only integer numbers are supported");
        return NULL;
    }

    char buffer[32];
    size_t length = (size_t)(parser->cursor - start);
    if (length >= sizeof(buffer)) {
        mode_json_fail(parser, "Number too long");
        return NULL;
    }

    memcpy(buffer, start, length);
    buffer[length] = '\0';

    char *end_ptr = NULL;
    long long number = strtoll(buffer, &end_ptr, 10);
    if ((end_ptr == NULL) || (*end_ptr != '\0')) {
        mode_json_fail(parser, "Invalid integer number");
        return NULL;
    }

    mode_json_value_t *value = mode_json_new_value(MODE_JSON_VALUE_NUMBER);
    if (value == NULL) {
        mode_json_fail(parser, "Out of memory");
        return NULL;
    }

    value->data.number = number;
    return value;
}

static mode_json_value_t *mode_json_parse_array(mode_json_parser_t *parser)
{
    if (*parser->cursor != '[') {
        mode_json_fail(parser, "Expected array");
        return NULL;
    }

    parser->cursor++;
    mode_json_skip_ws(parser);

    mode_json_value_t *array_value = mode_json_new_value(MODE_JSON_VALUE_ARRAY);
    if (array_value == NULL) {
        mode_json_fail(parser, "Out of memory");
        return NULL;
    }

    if (*parser->cursor == ']') {
        parser->cursor++;
        return array_value;
    }

    while (1) {
        mode_json_skip_ws(parser);
        mode_json_value_t *item = mode_json_parse_value(parser);
        if (item == NULL) {
            mode_json_free_value(array_value);
            return NULL;
        }

        if (!mode_json_append_array_item(array_value, item)) {
            mode_json_free_value(item);
            mode_json_free_value(array_value);
            mode_json_fail(parser, "Out of memory");
            return NULL;
        }

        mode_json_skip_ws(parser);
        if (*parser->cursor == ']') {
            parser->cursor++;
            return array_value;
        }

        if (*parser->cursor != ',') {
            mode_json_free_value(array_value);
            mode_json_fail(parser, "Expected ',' or ']'");
            return NULL;
        }

        parser->cursor++;
    }
}

static mode_json_value_t *mode_json_parse_object(mode_json_parser_t *parser)
{
    if (*parser->cursor != '{') {
        mode_json_fail(parser, "Expected object");
        return NULL;
    }

    parser->cursor++;
    mode_json_skip_ws(parser);

    mode_json_value_t *object_value = mode_json_new_value(MODE_JSON_VALUE_OBJECT);
    if (object_value == NULL) {
        mode_json_fail(parser, "Out of memory");
        return NULL;
    }

    if (*parser->cursor == '}') {
        parser->cursor++;
        return object_value;
    }

    while (1) {
        mode_json_skip_ws(parser);
        char *key = mode_json_parse_string_raw(parser);
        if (key == NULL) {
            mode_json_free_value(object_value);
            return NULL;
        }

        if (mode_json_object_contains_key(object_value, key)) {
            free(key);
            mode_json_free_value(object_value);
            mode_json_set_error(parser->error,
                                (size_t)(parser->cursor - parser->start),
                                "",
                                "Duplicate object key",
                                MODE_JSON_ERROR_DUPLICATE_KEY);
            return NULL;
        }

        mode_json_skip_ws(parser);
        if (*parser->cursor != ':') {
            free(key);
            mode_json_free_value(object_value);
            mode_json_fail(parser, "Expected ':'");
            return NULL;
        }
        parser->cursor++;

        mode_json_skip_ws(parser);
        mode_json_value_t *item = mode_json_parse_value(parser);
        if (item == NULL) {
            free(key);
            mode_json_free_value(object_value);
            return NULL;
        }

        if (!mode_json_append_object_entry(object_value, key, item)) {
            free(key);
            mode_json_free_value(item);
            mode_json_free_value(object_value);
            mode_json_fail(parser, "Out of memory");
            return NULL;
        }

        mode_json_skip_ws(parser);
        if (*parser->cursor == '}') {
            parser->cursor++;
            return object_value;
        }

        if (*parser->cursor != ',') {
            mode_json_free_value(object_value);
            mode_json_fail(parser, "Expected ',' or '}'");
            return NULL;
        }

        parser->cursor++;
    }
}

static mode_json_value_t *mode_json_parse_value(mode_json_parser_t *parser)
{
    mode_json_skip_ws(parser);

    char ch = *parser->cursor;
    if (ch == '\0') {
        mode_json_fail(parser, "Unexpected end of JSON");
        return NULL;
    }

    if (ch == '"') {
        char *text = mode_json_parse_string_raw(parser);
        if (text == NULL) {
            return NULL;
        }

        mode_json_value_t *value = mode_json_new_value(MODE_JSON_VALUE_STRING);
        if (value == NULL) {
            free(text);
            mode_json_fail(parser, "Out of memory");
            return NULL;
        }

        value->data.string = text;
        return value;
    }

    if (ch == '{') {
        return mode_json_parse_object(parser);
    }

    if (ch == '[') {
        return mode_json_parse_array(parser);
    }

    if ((ch == '-') || isdigit((unsigned char)ch)) {
        return mode_json_parse_number(parser);
    }

    if (mode_json_match_literal(parser, "true")) {
        mode_json_value_t *value = mode_json_new_value(MODE_JSON_VALUE_BOOL);
        if (value == NULL) {
            mode_json_fail(parser, "Out of memory");
            return NULL;
        }
        value->data.boolean = true;
        return value;
    }

    if (mode_json_match_literal(parser, "false")) {
        mode_json_value_t *value = mode_json_new_value(MODE_JSON_VALUE_BOOL);
        if (value == NULL) {
            mode_json_fail(parser, "Out of memory");
            return NULL;
        }
        value->data.boolean = false;
        return value;
    }

    if (mode_json_match_literal(parser, "null")) {
        mode_json_value_t *value = mode_json_new_value(MODE_JSON_VALUE_NULL);
        if (value == NULL) {
            mode_json_fail(parser, "Out of memory");
            return NULL;
        }
        return value;
    }

    mode_json_fail(parser, "Unexpected JSON token");
    return NULL;
}

static const mode_json_value_t *mode_json_object_get(const mode_json_value_t *object_value, const char *key)
{
    if ((object_value == NULL) || (object_value->type != MODE_JSON_VALUE_OBJECT) || (key == NULL)) {
        return NULL;
    }

    for (size_t i = 0; i < object_value->data.object.count; ++i) {
        if (strcmp(object_value->data.object.entries[i].key, key) == 0) {
            return object_value->data.object.entries[i].value;
        }
    }

    return NULL;
}

static bool mode_json_validate_object_keys(const mode_json_value_t *object_value,
                                           const char *const *allowed_keys,
                                           size_t allowed_key_count,
                                           mode_json_error_t *error,
                                           const char *path)
{
    if ((object_value == NULL) || (object_value->type != MODE_JSON_VALUE_OBJECT)) {
        return true;
    }

    for (size_t i = 0; i < object_value->data.object.count; ++i) {
        bool allowed = false;
        for (size_t j = 0; j < allowed_key_count; ++j) {
            if (strcmp(object_value->data.object.entries[i].key, allowed_keys[j]) == 0) {
                allowed = true;
                break;
            }
        }

        if (!allowed) {
            char field_path[128];
            (void)snprintf(field_path,
                           sizeof(field_path),
                           "%s.%s",
                           (path != NULL) ? path : "",
                           object_value->data.object.entries[i].key);
            return mode_json_fail_compile_at_code(error,
                                                  field_path,
                                                  "Unknown field",
                                                  MODE_JSON_ERROR_UNKNOWN_FIELD);
        }
    }

    return true;
}

static bool mode_json_equals_ignore_case(const char *left, const char *right)
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

static bool mode_json_get_string_required(const mode_json_value_t *object_value,
                                          const char *key,
                                          const char **out_text,
                                          mode_json_error_t *error)
{
    const mode_json_value_t *value = mode_json_object_get(object_value, key);
    if ((value == NULL) || (value->type != MODE_JSON_VALUE_STRING)) {
        return mode_json_fail_compile(error, "Missing required string field");
    }

    *out_text = value->data.string;
    return true;
}

static bool mode_json_get_string_optional(const mode_json_value_t *object_value,
                                          const char *key,
                                          const char **out_text)
{
    const mode_json_value_t *value = mode_json_object_get(object_value, key);
    if ((value == NULL) || (value->type != MODE_JSON_VALUE_STRING)) {
        return false;
    }

    *out_text = value->data.string;
    return true;
}

static bool mode_json_copy_optional_string(const mode_json_value_t *object_value,
                                           const char *key,
                                           const char **out_text,
                                           mode_json_error_t *error,
                                           const char *path)
{
    const mode_json_value_t *value = mode_json_object_get(object_value, key);
    if (value == NULL) {
        return true;
    }

    if (value->type != MODE_JSON_VALUE_STRING) {
        return mode_json_fail_compile_at(error, path, "Expected string field");
    }

    char *copy = mode_json_strdup(value->data.string);
    if (copy == NULL) {
        return mode_json_fail_compile_at(error, path, "Out of memory while copying string field");
    }

    free((void *)*out_text);
    *out_text = copy;
    return true;
}

static bool mode_json_get_bool_optional(const mode_json_value_t *object_value,
                                        const char *key,
                                        bool *out_value)
{
    const mode_json_value_t *value = mode_json_object_get(object_value, key);
    if ((value == NULL) || (value->type != MODE_JSON_VALUE_BOOL)) {
        return false;
    }

    *out_value = value->data.boolean;
    return true;
}

static bool mode_json_get_u32_optional(const mode_json_value_t *object_value,
                                       const char *key,
                                       uint32_t *out_value,
                                       mode_json_error_t *error)
{
    const mode_json_value_t *value = mode_json_object_get(object_value, key);
    if (value == NULL) {
        return false;
    }

    if ((value->type != MODE_JSON_VALUE_NUMBER) || (value->data.number < 0)) {
        return mode_json_fail_compile(error, "Expected non-negative integer field");
    }

    *out_value = (uint32_t)value->data.number;
    return true;
}

static bool mode_json_get_u16_optional(const mode_json_value_t *object_value,
                                       const char *key,
                                       uint16_t *out_value,
                                       mode_json_error_t *error)
{
    uint32_t value = 0;
    if (!mode_json_get_u32_optional(object_value, key, &value, error)) {
        return false;
    }

    if (value > UINT16_MAX) {
        return mode_json_fail_compile(error, "Integer field is out of range");
    }

    *out_value = (uint16_t)value;
    return true;
}

static bool mode_json_push_pending_mode_ref(mode_json_pending_mode_refs_t *pending_refs,
                                            mode_action_t *action,
                                            const char *mode_name)
{
    char *mode_name_copy = mode_json_strdup(mode_name);
    if (mode_name_copy == NULL) {
        return false;
    }

    size_t new_count = pending_refs->count + 1;
    mode_json_pending_mode_ref_t *new_items = (mode_json_pending_mode_ref_t *)realloc(
        pending_refs->items,
        new_count * sizeof(*new_items));
    if (new_items == NULL) {
        free(mode_name_copy);
        return false;
    }

    pending_refs->items = new_items;
    pending_refs->items[pending_refs->count].action = action;
    pending_refs->items[pending_refs->count].mode_name = mode_name_copy;
    pending_refs->count = new_count;
    return true;
}

static void mode_json_free_pending_mode_refs(mode_json_pending_mode_refs_t *pending_refs)
{
    if (pending_refs == NULL) {
        return;
    }

    for (size_t i = 0; i < pending_refs->count; ++i) {
        free(pending_refs->items[i].mode_name);
    }

    free(pending_refs->items);
    pending_refs->items = NULL;
    pending_refs->count = 0;
}

static bool mode_json_parse_input(const char *text, mode_input_t *input)
{
    if (mode_json_equals_ignore_case(text, "boot_button")) {
        *input = MODE_INPUT_BOOT_BUTTON;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "touch")) {
        *input = MODE_INPUT_TOUCH;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "encoder")) {
        *input = MODE_INPUT_ENCODER;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "usb_host_key")) {
        *input = MODE_INPUT_USB_HOST_KEY;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "timer")) {
        *input = MODE_INPUT_TIMER;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "imu")) {
        *input = MODE_INPUT_IMU;
        return true;
    }

    return false;
}

static bool mode_json_parse_trigger(const char *text, mode_trigger_t *trigger)
{
    if (mode_json_equals_ignore_case(text, "press")) {
        *trigger = MODE_TRIGGER_PRESS;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "release")) {
        *trigger = MODE_TRIGGER_RELEASE;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "tap")) {
        *trigger = MODE_TRIGGER_TAP;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "double_tap")) {
        *trigger = MODE_TRIGGER_DOUBLE_TAP;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "long_press")) {
        *trigger = MODE_TRIGGER_LONG_PRESS;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "hold_start")) {
        *trigger = MODE_TRIGGER_HOLD_START;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "hold_end")) {
        *trigger = MODE_TRIGGER_HOLD_END;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "swipe_up")) {
        *trigger = MODE_TRIGGER_SWIPE_UP;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "swipe_down")) {
        *trigger = MODE_TRIGGER_SWIPE_DOWN;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "swipe_left")) {
        *trigger = MODE_TRIGGER_SWIPE_LEFT;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "swipe_right")) {
        *trigger = MODE_TRIGGER_SWIPE_RIGHT;
        return true;
    }

    return false;
}

static bool mode_json_parse_cycle_direction(const char *text, mode_cycle_direction_t *direction)
{
    if (mode_json_equals_ignore_case(text, "next")) {
        *direction = MODE_CYCLE_DIRECTION_NEXT;
        return true;
    }
    if (mode_json_equals_ignore_case(text, "previous")) {
        *direction = MODE_CYCLE_DIRECTION_PREVIOUS;
        return true;
    }

    return false;
}

static bool mode_json_parse_modifier(const char *text, mode_hid_modifier_t *modifier)
{
    return mode_hid_parse_modifier_token(text, modifier);
}

static bool mode_json_parse_modifiers_value(const mode_json_value_t *value,
                                            mode_hid_modifier_t *modifiers,
                                            mode_json_error_t *error,
                                            const char *path)
{
    if ((value == NULL) || (modifiers == NULL)) {
        return true;
    }

    *modifiers = MODE_HID_MODIFIER_NONE;
    if (value->type == MODE_JSON_VALUE_STRING) {
        return mode_json_parse_modifier(value->data.string, modifiers) ||
               mode_json_fail_compile_at(error, path, "Invalid modifier token");
    }

    if (value->type != MODE_JSON_VALUE_ARRAY) {
        return mode_json_fail_compile_at(error, path, "modifiers must be a string or array");
    }

    for (size_t i = 0; i < value->data.array.count; ++i) {
        const mode_json_value_t *item = value->data.array.items[i];
        if ((item == NULL) || (item->type != MODE_JSON_VALUE_STRING)) {
            return mode_json_fail_compile_at(error, path, "modifiers entries must be strings");
        }

        mode_hid_modifier_t modifier = MODE_HID_MODIFIER_NONE;
        if (!mode_json_parse_modifier(item->data.string, &modifier)) {
            return mode_json_fail_compile_at(error, path, "Invalid modifier token");
        }
        *modifiers |= modifier;
    }

    return true;
}

static bool mode_json_parse_usage_value(const mode_json_value_t *action_value,
                                        const char *field_name,
                                        mode_hid_usage_t *usage,
                                        mode_json_error_t *error,
                                        const char *path)
{
    const mode_json_value_t *value = mode_json_object_get(action_value, field_name);
    if (value == NULL) {
        return mode_json_fail_compile_at(error, path, "Missing HID usage field");
    }

    if (value->type == MODE_JSON_VALUE_STRING) {
        if (!mode_hid_parse_usage_token(value->data.string, usage)) {
            return mode_json_fail_compile_at(error, path, "Invalid HID token");
        }
        return true;
    }

    if (value->type != MODE_JSON_VALUE_OBJECT) {
        return mode_json_fail_compile_at(error, path, "HID usage must be a string or object");
    }

    const char *kind_text = NULL;
    mode_hid_report_kind_t report_kind = MODE_HID_REPORT_KIND_KEYBOARD;
    uint32_t usage_page = 0;
    uint32_t usage_id = 0;
    const mode_json_value_t *usage_page_value = mode_json_object_get(value, "usagePage");
    const mode_json_value_t *usage_id_value = mode_json_object_get(value, "usage");
    if (!mode_json_get_string_required(value, "report", &kind_text, error) ||
        !mode_hid_parse_report_kind(kind_text, &report_kind)) {
        return mode_json_fail_compile_at(error, path, "Invalid HID report kind");
    }
    if ((usage_page_value == NULL) || (usage_id_value == NULL) ||
        !mode_json_get_u32_optional(value, "usagePage", &usage_page, error) ||
        !mode_json_get_u32_optional(value, "usage", &usage_id, error)) {
        return mode_json_fail_compile_at(error, path, "HID usage object requires usagePage and usage");
    }

    usage->report_kind = report_kind;
    usage->usage_page = (mode_hid_usage_page_t)usage_page;
    usage->modifiers = MODE_HID_MODIFIER_NONE;
    usage->usage_id = (uint16_t)usage_id;
    return true;
}

static bool mode_json_parse_actions(const mode_json_value_t *actions_value,
                                    mode_action_t **out_actions,
                                    size_t *out_action_count,
                                    mode_json_pending_mode_refs_t *pending_refs,
                                    mode_json_error_t *error)
{
    if ((actions_value == NULL) || (actions_value->type != MODE_JSON_VALUE_ARRAY)) {
        return mode_json_fail_compile(error, "Binding actions must be an array");
    }

    size_t action_count = actions_value->data.array.count;
    mode_action_t *actions = NULL;
    if (action_count > 0) {
        actions = (mode_action_t *)mode_json_calloc(action_count, sizeof(*actions));
        if (actions == NULL) {
            return mode_json_fail_compile(error, "Out of memory while parsing actions");
        }
    }

    for (size_t i = 0; i < action_count; ++i) {
        const mode_json_value_t *action_value = actions_value->data.array.items[i];
        char action_path[64];
        (void)snprintf(action_path, sizeof(action_path), "actions[%u]", (unsigned)i);
        if ((action_value == NULL) || (action_value->type != MODE_JSON_VALUE_OBJECT)) {
            mode_json_free_actions(actions, action_count);
            return mode_json_fail_compile_at(error, action_path, "Each action must be an object");
        }

        static const char *const allowed_action_keys[] = {
            "type", "key", "modifier", "modifiers", "duration_ms", "enabled",
            "text", "mode", "direction", "usage", "report", "usagePage"
        };
        if (!mode_json_validate_object_keys(action_value,
                                            allowed_action_keys,
                                            sizeof(allowed_action_keys) / sizeof(allowed_action_keys[0]),
                                            error,
                                            action_path)) {
            mode_json_free_actions(actions, action_count);
            return false;
        }

        const char *action_type = NULL;
        if (!mode_json_get_string_required(action_value, "type", &action_type, error)) {
            mode_json_free_actions(actions, action_count);
            return false;
        }

        mode_action_t *action = &actions[i];
        if (mode_json_equals_ignore_case(action_type, "hid_key_down")) {
            if (!mode_json_parse_usage_value(action_value, "key", &action->data.hid_usage, error, action_path) ||
                !mode_hid_usage_is_keyboard(&action->data.hid_usage) ||
                !mode_json_parse_modifiers_value(mode_json_object_get(action_value, "modifiers"),
                                                 &action->data.hid_usage.modifiers,
                                                 error,
                                                 action_path)) {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "Invalid hid_key_down payload");
            }
            action->type = MODE_ACTION_HID_KEY_DOWN;
        } else if (mode_json_equals_ignore_case(action_type, "hid_key_up")) {
            if (!mode_json_parse_usage_value(action_value, "key", &action->data.hid_usage, error, action_path) ||
                !mode_hid_usage_is_keyboard(&action->data.hid_usage) ||
                !mode_json_parse_modifiers_value(mode_json_object_get(action_value, "modifiers"),
                                                 &action->data.hid_usage.modifiers,
                                                 error,
                                                 action_path)) {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "Invalid hid_key_up payload");
            }
            action->type = MODE_ACTION_HID_KEY_UP;
        } else if (mode_json_equals_ignore_case(action_type, "hid_key_tap")) {
            const char *key = NULL;
            if (!mode_json_get_string_required(action_value, "key", &key, error)) {
                mode_json_free_actions(actions, action_count);
                return false;
            }
            if (mode_hid_parse_shortcut_string(key, &action->data.hid_usage)) {
                action->type = MODE_ACTION_HID_SHORTCUT_TAP;
            } else if (mode_hid_parse_usage_token(key, &action->data.hid_usage) &&
                       mode_hid_usage_is_keyboard(&action->data.hid_usage)) {
                if (!mode_json_parse_modifiers_value(mode_json_object_get(action_value, "modifiers"),
                                                     &action->data.hid_usage.modifiers,
                                                     error,
                                                     action_path)) {
                    mode_json_free_actions(actions, action_count);
                    return false;
                }
                action->type = MODE_ACTION_HID_KEY_TAP;
            } else {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "Invalid hid_key_tap key");
            }
        } else if (mode_json_equals_ignore_case(action_type, "hid_shortcut_tap")) {
            const char *key = NULL;
            const mode_json_value_t *modifier_value = mode_json_object_get(action_value, "modifiers");
            if (modifier_value == NULL) {
                modifier_value = mode_json_object_get(action_value, "modifier");
            }
            if (!mode_json_get_string_required(action_value, "key", &key, error) ||
                !mode_hid_parse_usage_token(key, &action->data.hid_usage) ||
                !mode_hid_usage_is_keyboard(&action->data.hid_usage) ||
                !mode_json_parse_modifiers_value(modifier_value,
                                                 &action->data.hid_usage.modifiers,
                                                 error,
                                                 action_path)) {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "Invalid hid_shortcut_tap fields");
            }
            action->type = MODE_ACTION_HID_SHORTCUT_TAP;
        } else if (mode_json_equals_ignore_case(action_type, "hid_modifier_down")) {
            const mode_json_value_t *modifier_value = mode_json_object_get(action_value, "modifiers");
            if (modifier_value == NULL) {
                modifier_value = mode_json_object_get(action_value, "modifier");
            }
            if (!mode_json_parse_modifiers_value(modifier_value, &action->data.modifier, error, action_path)) {
                mode_json_free_actions(actions, action_count);
                return false;
            }
            action->type = MODE_ACTION_HID_MODIFIER_DOWN;
        } else if (mode_json_equals_ignore_case(action_type, "hid_modifier_up")) {
            const mode_json_value_t *modifier_value = mode_json_object_get(action_value, "modifiers");
            if (modifier_value == NULL) {
                modifier_value = mode_json_object_get(action_value, "modifier");
            }
            if (!mode_json_parse_modifiers_value(modifier_value, &action->data.modifier, error, action_path)) {
                mode_json_free_actions(actions, action_count);
                return false;
            }
            action->type = MODE_ACTION_HID_MODIFIER_UP;
        } else if (mode_json_equals_ignore_case(action_type, "hid_usage_down")) {
            if (!mode_json_parse_usage_value(action_value, "usage", &action->data.hid_usage, error, action_path) ||
                !mode_json_parse_modifiers_value(mode_json_object_get(action_value, "modifiers"),
                                                 &action->data.hid_usage.modifiers,
                                                 error,
                                                 action_path)) {
                mode_json_free_actions(actions, action_count);
                return false;
            }
            action->type = MODE_ACTION_HID_USAGE_DOWN;
        } else if (mode_json_equals_ignore_case(action_type, "hid_usage_up")) {
            if (!mode_json_parse_usage_value(action_value, "usage", &action->data.hid_usage, error, action_path) ||
                !mode_json_parse_modifiers_value(mode_json_object_get(action_value, "modifiers"),
                                                 &action->data.hid_usage.modifiers,
                                                 error,
                                                 action_path)) {
                mode_json_free_actions(actions, action_count);
                return false;
            }
            action->type = MODE_ACTION_HID_USAGE_UP;
        } else if (mode_json_equals_ignore_case(action_type, "hid_usage_tap")) {
            if (!mode_json_parse_usage_value(action_value, "usage", &action->data.hid_usage, error, action_path) ||
                !mode_json_parse_modifiers_value(mode_json_object_get(action_value, "modifiers"),
                                                 &action->data.hid_usage.modifiers,
                                                 error,
                                                 action_path)) {
                mode_json_free_actions(actions, action_count);
                return false;
            }
            action->type = MODE_ACTION_HID_USAGE_TAP;
        } else if (mode_json_equals_ignore_case(action_type, "sleep_ms")) {
            uint32_t duration_ms = 0;
            if (!mode_json_get_u32_optional(action_value, "duration_ms", &duration_ms, error)) {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "Invalid sleep_ms duration");
            }
            action->type = MODE_ACTION_SLEEP_MS;
            action->data.duration_ms = duration_ms;
        } else if (mode_json_equals_ignore_case(action_type, "enter_boot_mode")) {
            action->type = MODE_ACTION_ENTER_BOOT_MODE;
        } else if (mode_json_equals_ignore_case(action_type, "exit_boot_mode")) {
            action->type = MODE_ACTION_EXIT_BOOT_MODE;
        } else if (mode_json_equals_ignore_case(action_type, "mic_gate")) {
            const mode_json_value_t *enabled_value = mode_json_object_get(action_value, "enabled");
            if ((enabled_value == NULL) || (enabled_value->type != MODE_JSON_VALUE_BOOL)) {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "mic_gate requires a boolean enabled field");
            }
            action->type = MODE_ACTION_MIC_GATE;
            action->data.enabled = enabled_value->data.boolean;
        } else if (mode_json_equals_ignore_case(action_type, "mic_gate_toggle")) {
            action->type = MODE_ACTION_MIC_GATE_TOGGLE;
        } else if (mode_json_equals_ignore_case(action_type, "ui_hint")) {
            const char *text = NULL;
            if (!mode_json_get_string_required(action_value, "text", &text, error)) {
                mode_json_free_actions(actions, action_count);
                return false;
            }
            action->type = MODE_ACTION_UI_HINT;
            action->data.text = mode_json_strdup(text);
            if (action->data.text == NULL) {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "Out of memory while copying ui_hint text");
            }
        } else if (mode_json_equals_ignore_case(action_type, "ui_show_mode")) {
            action->type = MODE_ACTION_UI_SHOW_MODE;
        } else if (mode_json_equals_ignore_case(action_type, "set_mode")) {
            const char *mode_name = NULL;
            if (!mode_json_get_string_required(action_value, "mode", &mode_name, error)) {
                mode_json_free_actions(actions, action_count);
                return false;
            }
            action->type = MODE_ACTION_SET_MODE;
            action->data.mode = MODE_ID_INVALID;
            if (!mode_json_push_pending_mode_ref(pending_refs, action, mode_name)) {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "Out of memory while recording set_mode");
            }
        } else if (mode_json_equals_ignore_case(action_type, "cycle_mode")) {
            const char *direction = NULL;
            if (!mode_json_get_string_required(action_value, "direction", &direction, error) ||
                !mode_json_parse_cycle_direction(direction, &action->data.direction)) {
                mode_json_free_actions(actions, action_count);
                return mode_json_fail_compile_at(error, action_path, "Invalid cycle_mode direction");
            }
            action->type = MODE_ACTION_CYCLE_MODE;
        } else if (mode_json_equals_ignore_case(action_type, "noop")) {
            action->type = MODE_ACTION_NOOP;
        } else {
            mode_json_free_actions(actions, action_count);
            return mode_json_fail_compile_at(error, action_path, "Unsupported action type");
        }
    }

    *out_actions = actions;
    *out_action_count = action_count;
    return true;
}

static void mode_json_free_actions(mode_action_t *actions, size_t action_count)
{
    if (actions == NULL) {
        return;
    }

    for (size_t i = 0; i < action_count; ++i) {
        if (actions[i].type == MODE_ACTION_UI_HINT) {
            free((void *)actions[i].data.text);
        }
    }

    free(actions);
}

static void mode_json_free_bindings(mode_binding_t *bindings, size_t binding_count)
{
    if (bindings == NULL) {
        return;
    }

    for (size_t i = 0; i < binding_count; ++i) {
        mode_json_free_actions((mode_action_t *)bindings[i].actions, bindings[i].action_count);
    }

    free(bindings);
}

static bool mode_json_parse_bindings(const mode_json_value_t *bindings_value,
                                     mode_binding_t **out_bindings,
                                     size_t *out_binding_count,
                                     mode_json_pending_mode_refs_t *pending_refs,
                                     mode_json_error_t *error)
{
    if ((bindings_value == NULL) || (bindings_value->type != MODE_JSON_VALUE_ARRAY)) {
        return mode_json_fail_compile(error, "Bindings must be an array");
    }

    size_t binding_count = bindings_value->data.array.count;
    mode_binding_t *bindings = NULL;
    if (binding_count > 0) {
        bindings = (mode_binding_t *)mode_json_calloc(binding_count, sizeof(*bindings));
        if (bindings == NULL) {
            return mode_json_fail_compile(error, "Out of memory while parsing bindings");
        }
    }

    for (size_t i = 0; i < binding_count; ++i) {
        const mode_json_value_t *binding_value = bindings_value->data.array.items[i];
        char binding_path[64];
        (void)snprintf(binding_path, sizeof(binding_path), "bindings[%u]", (unsigned)i);
        if ((binding_value == NULL) || (binding_value->type != MODE_JSON_VALUE_OBJECT)) {
            mode_json_free_bindings(bindings, i);
            return mode_json_fail_compile_at(error, binding_path, "Each binding must be an object");
        }

        static const char *const allowed_binding_keys[] = {
            "input", "trigger", "actions"
        };
        if (!mode_json_validate_object_keys(binding_value,
                                            allowed_binding_keys,
                                            sizeof(allowed_binding_keys) / sizeof(allowed_binding_keys[0]),
                                            error,
                                            binding_path)) {
            mode_json_free_bindings(bindings, i);
            return false;
        }

        const char *input = NULL;
        const char *trigger = NULL;
        const mode_json_value_t *actions_value = mode_json_object_get(binding_value, "actions");

        if (!mode_json_get_string_required(binding_value, "input", &input, error) ||
            !mode_json_get_string_required(binding_value, "trigger", &trigger, error) ||
            !mode_json_parse_input(input, &bindings[i].input) ||
            !mode_json_parse_trigger(trigger, &bindings[i].trigger)) {
            mode_json_free_bindings(bindings, i);
            return mode_json_fail_compile_at(error, binding_path, "Invalid binding input or trigger");
        }

        if (!mode_json_parse_actions(actions_value,
                                     (mode_action_t **)&bindings[i].actions,
                                     &bindings[i].action_count,
                                     pending_refs,
                                     error)) {
            mode_json_free_bindings(bindings, i + 1);
            return false;
        }
    }

    *out_bindings = bindings;
    *out_binding_count = binding_count;
    return true;
}

static void mode_json_free_modes(mode_definition_t *modes, size_t mode_count)
{
    if (modes == NULL) {
        return;
    }

    for (size_t i = 0; i < mode_count; ++i) {
        free((void *)modes[i].name);
        free((void *)modes[i].label);
        mode_json_free_bindings((mode_binding_t *)modes[i].bindings, modes[i].binding_count);
    }

    free(modes);
}

static bool mode_json_parse_mode_definition(const mode_json_value_t *mode_value,
                                            const char *mode_id,
                                            size_t index,
                                            mode_definition_t *mode,
                                            mode_json_pending_mode_refs_t *pending_refs,
                                            mode_json_error_t *error)
{
    if ((mode_value == NULL) || (mode_value->type != MODE_JSON_VALUE_OBJECT)) {
        return mode_json_fail_compile_at(error, "modes", "Each mode definition must be an object");
    }

    static const char *const allowed_mode_keys[] = {
        "id", "label", "bindings", "cycleOrder"
    };
    if (!mode_json_validate_object_keys(mode_value,
                                        allowed_mode_keys,
                                        sizeof(allowed_mode_keys) / sizeof(allowed_mode_keys[0]),
                                        error,
                                        "modes")) {
        return false;
    }

    const char *label = NULL;
    const mode_json_value_t *bindings_value = mode_json_object_get(mode_value, "bindings");
    if (!mode_json_get_string_required(mode_value, "label", &label, error)) {
        return false;
    }

    mode->id = (mode_id_t)index;
    mode->name = mode_json_strdup(mode_id);
    mode->label = mode_json_strdup(label);
    mode->cycle_order = (uint32_t)index;
    if ((mode->name == NULL) || (mode->label == NULL)) {
        return mode_json_fail_compile_at(error, "modes", "Out of memory while copying mode strings");
    }

    (void)mode_json_get_u32_optional(mode_value, "cycleOrder", &mode->cycle_order, error);
    return mode_json_parse_bindings(bindings_value,
                                    (mode_binding_t **)&mode->bindings,
                                    &mode->binding_count,
                                    pending_refs,
                                    error);
}

static bool mode_json_parse_modes(const mode_json_value_t *modes_value,
                                  mode_definition_t **out_modes,
                                  size_t *out_mode_count,
                                  mode_json_pending_mode_refs_t *pending_refs,
                                  mode_json_error_t *error)
{
    if ((modes_value == NULL) ||
        ((modes_value->type != MODE_JSON_VALUE_OBJECT) && (modes_value->type != MODE_JSON_VALUE_ARRAY))) {
        return mode_json_fail_compile(error, "modes must be an object or array");
    }

    size_t mode_count = (modes_value->type == MODE_JSON_VALUE_ARRAY)
        ? modes_value->data.array.count
        : modes_value->data.object.count;
    if (mode_count == 0) {
        return mode_json_fail_compile(error, "modes must contain at least one mode");
    }

    mode_definition_t *modes = (mode_definition_t *)mode_json_calloc(mode_count, sizeof(*modes));
    if (modes == NULL) {
        return mode_json_fail_compile(error, "Out of memory while parsing modes");
    }

    for (size_t i = 0; i < mode_count; ++i) {
        if (modes_value->type == MODE_JSON_VALUE_ARRAY) {
            const mode_json_value_t *mode_value = modes_value->data.array.items[i];
            const char *mode_id = NULL;
            if ((mode_value == NULL) || (mode_value->type != MODE_JSON_VALUE_OBJECT) ||
                !mode_json_get_string_required(mode_value, "id", &mode_id, error) ||
                !mode_json_parse_mode_definition(mode_value, mode_id, i, &modes[i], pending_refs, error)) {
                mode_json_free_modes(modes, i + 1);
                return false;
            }
        } else {
            const mode_json_object_entry_t *entry = &modes_value->data.object.entries[i];
            if (!mode_json_parse_mode_definition(entry->value, entry->key, i, &modes[i], pending_refs, error)) {
                mode_json_free_modes(modes, i + 1);
                return false;
            }
        }
    }

    for (size_t i = 0; i < mode_count; ++i) {
        for (size_t j = i + 1; j < mode_count; ++j) {
            if (modes[j].cycle_order < modes[i].cycle_order) {
                mode_definition_t temp = modes[i];
                modes[i] = modes[j];
                modes[j] = temp;
            }
        }
    }

    for (size_t i = 0; i < mode_count; ++i) {
        for (size_t j = i + 1; j < mode_count; ++j) {
            if ((modes[i].name != NULL) && (modes[j].name != NULL) &&
                mode_json_equals_ignore_case(modes[i].name, modes[j].name)) {
                mode_json_free_modes(modes, mode_count);
                return mode_json_fail_compile_at(error, "modes", "Duplicate mode id");
            }
        }
    }

    *out_modes = modes;
    *out_mode_count = mode_count;
    return true;
}

static mode_id_t mode_json_find_mode_id(const mode_config_t *config, const char *mode_name)
{
    if ((config == NULL) || (mode_name == NULL)) {
        return MODE_ID_INVALID;
    }

    if (mode_json_equals_ignore_case(mode_name, "mouse")) {
        return MODE_ID_MOUSE;
    }

    for (size_t i = 0; i < config->mode_count; ++i) {
        if ((config->modes[i].name != NULL) && mode_json_equals_ignore_case(config->modes[i].name, mode_name)) {
            return config->modes[i].id;
        }
    }

    return MODE_ID_INVALID;
}

static bool mode_json_parse_boot_ui(const mode_json_value_t *boot_mode_value,
                                    mode_boot_ui_t *ui,
                                    mode_json_error_t *error)
{
    const mode_json_value_t *ui_value = mode_json_object_get(boot_mode_value, "ui");
    if (ui_value == NULL) {
        return true;
    }

    if (ui_value->type != MODE_JSON_VALUE_OBJECT) {
        return mode_json_fail_compile(error, "bootMode.ui must be an object");
    }

    static const char *const allowed_boot_ui_keys[] = {
        "title", "subtitle", "showModeList", "showGestureHints", "showCurrentModeCard"
    };
    if (!mode_json_validate_object_keys(ui_value,
                                        allowed_boot_ui_keys,
                                        sizeof(allowed_boot_ui_keys) / sizeof(allowed_boot_ui_keys[0]),
                                        error,
                                        "bootMode.ui")) {
        return false;
    }

    const char *text = NULL;
    if (mode_json_get_string_optional(ui_value, "title", &text)) {
        ui->title = mode_json_strdup(text);
    }
    if (mode_json_get_string_optional(ui_value, "subtitle", &text)) {
        ui->subtitle = mode_json_strdup(text);
    }
    if (mode_json_get_bool_optional(ui_value, "showModeList", &ui->show_mode_list)) {
    }
    if (mode_json_get_bool_optional(ui_value, "showGestureHints", &ui->show_gesture_hints)) {
    }
    if (mode_json_get_bool_optional(ui_value, "showCurrentModeCard", &ui->show_current_mode_card)) {
    }

    if (((ui->title == NULL) && mode_json_object_get(ui_value, "title") != NULL) ||
        ((ui->subtitle == NULL) && mode_json_object_get(ui_value, "subtitle") != NULL)) {
        return mode_json_fail_compile(error, "Out of memory while copying bootMode.ui text");
    }

    return true;
}

static bool mode_json_parse_wifi_config(const mode_json_value_t *root_value,
                                        mode_wifi_config_t *wifi,
                                        mode_json_error_t *error)
{
    const mode_json_value_t *wifi_value = mode_json_object_get(root_value, "wifi");
    if (wifi_value == NULL) {
        return true;
    }

    if (wifi_value->type != MODE_JSON_VALUE_OBJECT) {
        return mode_json_fail_compile(error, "wifi must be an object");
    }

    static const char *const allowed_wifi_keys[] = {
        "sta", "ap", "hostname", "localUrl"
    };
    if (!mode_json_validate_object_keys(wifi_value,
                                        allowed_wifi_keys,
                                        sizeof(allowed_wifi_keys) / sizeof(allowed_wifi_keys[0]),
                                        error,
                                        "wifi")) {
        return false;
    }

    const mode_json_value_t *sta_value = mode_json_object_get(wifi_value, "sta");
    if (sta_value != NULL) {
        if (sta_value->type != MODE_JSON_VALUE_OBJECT) {
            return mode_json_fail_compile_at(error, "wifi.sta", "wifi.sta must be an object");
        }

        static const char *const allowed_wifi_credential_keys[] = {
            "ssid", "password"
        };
        if (!mode_json_validate_object_keys(sta_value,
                                            allowed_wifi_credential_keys,
                                            sizeof(allowed_wifi_credential_keys) / sizeof(allowed_wifi_credential_keys[0]),
                                            error,
                                            "wifi.sta") ||
            !mode_json_copy_optional_string(sta_value, "ssid", &wifi->sta.ssid, error, "wifi.sta.ssid") ||
            !mode_json_copy_optional_string(sta_value, "password", &wifi->sta.password, error, "wifi.sta.password")) {
            return false;
        }
    }

    const mode_json_value_t *ap_value = mode_json_object_get(wifi_value, "ap");
    if (ap_value != NULL) {
        if (ap_value->type != MODE_JSON_VALUE_OBJECT) {
            return mode_json_fail_compile_at(error, "wifi.ap", "wifi.ap must be an object");
        }

        static const char *const allowed_wifi_credential_keys[] = {
            "ssid", "password"
        };
        if (!mode_json_validate_object_keys(ap_value,
                                            allowed_wifi_credential_keys,
                                            sizeof(allowed_wifi_credential_keys) / sizeof(allowed_wifi_credential_keys[0]),
                                            error,
                                            "wifi.ap") ||
            !mode_json_copy_optional_string(ap_value, "ssid", &wifi->ap.ssid, error, "wifi.ap.ssid") ||
            !mode_json_copy_optional_string(ap_value, "password", &wifi->ap.password, error, "wifi.ap.password")) {
            return false;
        }
    }

    return mode_json_copy_optional_string(wifi_value, "hostname", &wifi->hostname, error, "wifi.hostname") &&
           mode_json_copy_optional_string(wifi_value, "localUrl", &wifi->local_url, error, "wifi.localUrl");
}

static void mode_json_free_boot_mode(mode_boot_mode_t *boot_mode)
{
    if (boot_mode == NULL) {
        return;
    }

    free((void *)boot_mode->label);
    free((void *)boot_mode->ui.title);
    free((void *)boot_mode->ui.subtitle);
    mode_json_free_bindings((mode_binding_t *)boot_mode->bindings, boot_mode->binding_count);
}

static void mode_json_free_wifi_config(mode_wifi_config_t *wifi)
{
    if (wifi == NULL) {
        return;
    }

    free((void *)wifi->sta.ssid);
    free((void *)wifi->sta.password);
    free((void *)wifi->ap.ssid);
    free((void *)wifi->ap.password);
    free((void *)wifi->hostname);
    free((void *)wifi->local_url);
}

static size_t mode_json_count_matching_bindings(const mode_binding_t *bindings,
                                                size_t binding_count,
                                                mode_input_t input,
                                                mode_trigger_t trigger)
{
    size_t count = 0;
    for (size_t i = 0; i < binding_count; ++i) {
        if ((bindings[i].input == input) && (bindings[i].trigger == trigger)) {
            count++;
        }
    }

    return count;
}

static bool mode_json_validate_binding_limit(const mode_binding_t *global_bindings,
                                             size_t global_binding_count,
                                             const mode_binding_t *context_bindings,
                                             size_t context_binding_count,
                                             const char *path,
                                             mode_json_error_t *error)
{
    for (int input = MODE_INPUT_BOOT_BUTTON; input <= MODE_INPUT_IMU; ++input) {
        for (int trigger = MODE_TRIGGER_PRESS; trigger <= MODE_TRIGGER_SWIPE_RIGHT; ++trigger) {
            size_t total = mode_json_count_matching_bindings(global_bindings,
                                                             global_binding_count,
                                                             (mode_input_t)input,
                                                             (mode_trigger_t)trigger) +
                           mode_json_count_matching_bindings(context_bindings,
                                                             context_binding_count,
                                                             (mode_input_t)input,
                                                             (mode_trigger_t)trigger);
            if (total <= MODE_CONFIG_MAX_MATCHED_BINDINGS) {
                continue;
            }

            return mode_json_fail_compile_at_code(error,
                                                  path,
                                                  "Binding fan-out exceeds runtime match limit",
                                                  MODE_JSON_ERROR_RUNTIME_LIMIT);
        }
    }

    return true;
}

static bool mode_json_validate_runtime_limits(const mode_config_t *config, mode_json_error_t *error)
{
    if (config == NULL) {
        return true;
    }

    if (!mode_json_validate_binding_limit(config->global_bindings,
                                          config->global_binding_count,
                                          config->boot_mode.bindings,
                                          config->boot_mode.binding_count,
                                          "bootMode.bindings",
                                          error)) {
        return false;
    }

    for (size_t i = 0; i < config->mode_count; ++i) {
        char path[128];
        (void)snprintf(path,
                       sizeof(path),
                       "modes[%s].bindings",
                       (config->modes[i].name != NULL) ? config->modes[i].name : "unknown");
        if (!mode_json_validate_binding_limit(config->global_bindings,
                                              config->global_binding_count,
                                              config->modes[i].bindings,
                                              config->modes[i].binding_count,
                                              path,
                                              error)) {
            return false;
        }
    }

    return true;
}

static bool mode_json_compile_config(const mode_json_value_t *root_value,
                                     mode_config_t **out_config,
                                     mode_json_error_t *error)
{
    if ((root_value == NULL) || (root_value->type != MODE_JSON_VALUE_OBJECT)) {
        return mode_json_fail_compile(error, "Top-level JSON must be an object");
    }

    static const char *const allowed_top_level_keys[] = {
        "version", "activeMode", "defaults", "wifi", "globalBindings", "bootMode", "modes"
    };
    if (!mode_json_validate_object_keys(root_value,
                                        allowed_top_level_keys,
                                        sizeof(allowed_top_level_keys) / sizeof(allowed_top_level_keys[0]),
                                        error,
                                        "root")) {
        return false;
    }

    mode_config_t *config = (mode_config_t *)mode_json_calloc(1, sizeof(*config));
    if (config == NULL) {
        return mode_json_fail_compile(error, "Out of memory while allocating config");
    }

    config->defaults.touch.hold_ms = MODE_JSON_DEFAULT_HOLD_MS;
    config->defaults.touch.double_tap_ms = MODE_JSON_DEFAULT_DOUBLE_TAP_MS;
    config->defaults.touch.swipe_min_distance = MODE_JSON_DEFAULT_SWIPE_MIN_DISTANCE;
    config->wifi.ap.ssid = mode_json_strdup(MODE_JSON_DEFAULT_WIFI_AP_SSID);
    config->wifi.ap.password = mode_json_strdup(MODE_JSON_DEFAULT_WIFI_AP_PASSWORD);
    config->wifi.hostname = mode_json_strdup(MODE_JSON_DEFAULT_WIFI_HOSTNAME);
    config->wifi.local_url = mode_json_strdup(MODE_JSON_DEFAULT_WIFI_LOCAL_URL);
    config->wifi.sta.ssid = mode_json_strdup(MODE_JSON_DEFAULT_WIFI_STA_SSID);
    config->wifi.sta.password = mode_json_strdup(MODE_JSON_DEFAULT_WIFI_STA_PASSWORD);
    if ((config->wifi.ap.ssid == NULL) ||
        (config->wifi.ap.password == NULL) ||
        (config->wifi.hostname == NULL) ||
        (config->wifi.local_url == NULL) ||
        (config->wifi.sta.ssid == NULL) ||
        (config->wifi.sta.password == NULL)) {
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return mode_json_fail_compile(error, "Out of memory while allocating wifi defaults");
    }

    mode_json_pending_mode_refs_t pending_refs = {0};
    const mode_json_value_t *version_value = mode_json_object_get(root_value, "version");
    if ((version_value == NULL) || (version_value->type != MODE_JSON_VALUE_NUMBER) || (version_value->data.number < 0)) {
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return mode_json_fail_compile(error, "version must be a non-negative integer");
    }
    config->version = (uint32_t)version_value->data.number;

    const mode_json_value_t *defaults_value = mode_json_object_get(root_value, "defaults");
    if ((defaults_value != NULL) && (defaults_value->type != MODE_JSON_VALUE_OBJECT)) {
        free(config);
        return mode_json_fail_compile(error, "defaults must be an object");
    }
    if (defaults_value != NULL) {
        static const char *const allowed_defaults_keys[] = {
            "touch"
        };
        if (!mode_json_validate_object_keys(defaults_value,
                                            allowed_defaults_keys,
                                            sizeof(allowed_defaults_keys) / sizeof(allowed_defaults_keys[0]),
                                            error,
                                            "defaults")) {
            mode_json_free_wifi_config(&config->wifi);
            free(config);
            return false;
        }
        const mode_json_value_t *touch_value = mode_json_object_get(defaults_value, "touch");
        if ((touch_value != NULL) && (touch_value->type != MODE_JSON_VALUE_OBJECT)) {
            mode_json_free_wifi_config(&config->wifi);
            free(config);
            return mode_json_fail_compile(error, "defaults.touch must be an object");
        }
        if (touch_value != NULL) {
            static const char *const allowed_touch_keys[] = {
                "holdMs", "doubleTapMs", "swipeMinDistance"
            };
            if (!mode_json_validate_object_keys(touch_value,
                                                allowed_touch_keys,
                                                sizeof(allowed_touch_keys) / sizeof(allowed_touch_keys[0]),
                                                error,
                                                "defaults.touch")) {
                mode_json_free_wifi_config(&config->wifi);
                free(config);
                return false;
            }
            if (!mode_json_get_u32_optional(touch_value, "holdMs", &config->defaults.touch.hold_ms, error) &&
                (mode_json_object_get(touch_value, "holdMs") != NULL)) {
                mode_json_free_wifi_config(&config->wifi);
                free(config);
                return false;
            }
            if (!mode_json_get_u32_optional(touch_value, "doubleTapMs", &config->defaults.touch.double_tap_ms, error) &&
                (mode_json_object_get(touch_value, "doubleTapMs") != NULL)) {
                mode_json_free_wifi_config(&config->wifi);
                free(config);
                return false;
            }
            if (!mode_json_get_u16_optional(touch_value, "swipeMinDistance", &config->defaults.touch.swipe_min_distance, error) &&
                (mode_json_object_get(touch_value, "swipeMinDistance") != NULL)) {
                mode_json_free_wifi_config(&config->wifi);
                free(config);
                return false;
            }
        }
    }

    if (!mode_json_parse_wifi_config(root_value, &config->wifi, error)) {
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return false;
    }

    const mode_json_value_t *global_bindings_value = mode_json_object_get(root_value, "globalBindings");
    if (global_bindings_value == NULL) {
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return mode_json_fail_compile(error, "globalBindings is required");
    }
    if (!mode_json_parse_bindings(global_bindings_value,
                                  (mode_binding_t **)&config->global_bindings,
                                  &config->global_binding_count,
                                  &pending_refs,
                                  error)) {
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return false;
    }

    const mode_json_value_t *boot_mode_value = mode_json_object_get(root_value, "bootMode");
    if ((boot_mode_value == NULL) || (boot_mode_value->type != MODE_JSON_VALUE_OBJECT)) {
        mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return mode_json_fail_compile(error, "bootMode must be an object");
    }

    static const char *const allowed_boot_mode_keys[] = {
        "label", "ui", "bindings"
    };
    if (!mode_json_validate_object_keys(boot_mode_value,
                                        allowed_boot_mode_keys,
                                        sizeof(allowed_boot_mode_keys) / sizeof(allowed_boot_mode_keys[0]),
                                        error,
                                        "bootMode")) {
        mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return false;
    }

    const char *boot_label = NULL;
    const mode_json_value_t *boot_bindings_value = mode_json_object_get(boot_mode_value, "bindings");
    if (!mode_json_get_string_required(boot_mode_value, "label", &boot_label, error)) {
        mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return false;
    }

    config->boot_mode.label = mode_json_strdup(boot_label);
    if (config->boot_mode.label == NULL) {
        mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return mode_json_fail_compile(error, "Out of memory while copying bootMode label");
    }

    if (!mode_json_parse_boot_ui(boot_mode_value, &config->boot_mode.ui, error) ||
        !mode_json_parse_bindings(boot_bindings_value,
                                  (mode_binding_t **)&config->boot_mode.bindings,
                                  &config->boot_mode.binding_count,
                                  &pending_refs,
                                  error)) {
        mode_json_free_boot_mode(&config->boot_mode);
        mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return false;
    }

    const mode_json_value_t *modes_value = mode_json_object_get(root_value, "modes");
    if (!mode_json_parse_modes(modes_value,
                               (mode_definition_t **)&config->modes,
                               &config->mode_count,
                               &pending_refs,
                               error)) {
        mode_json_free_boot_mode(&config->boot_mode);
        mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        return false;
    }

    const char *active_mode_name = NULL;
    if (mode_json_get_string_optional(root_value, "activeMode", &active_mode_name)) {
        config->active_mode = mode_json_find_mode_id(config, active_mode_name);
        if (config->active_mode == MODE_ID_INVALID) {
            mode_json_free_modes((mode_definition_t *)config->modes, config->mode_count);
            mode_json_free_boot_mode(&config->boot_mode);
            mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
            mode_json_free_wifi_config(&config->wifi);
            free(config);
            mode_json_free_pending_mode_refs(&pending_refs);
            return mode_json_fail_compile_at_code(error,
                                                  "activeMode",
                                                  "activeMode does not match a configured mode",
                                                  MODE_JSON_ERROR_INVALID_REFERENCE);
        }
    } else {
        config->active_mode = config->modes[0].id;
    }

    for (size_t i = 0; i < pending_refs.count; ++i) {
        mode_id_t mode_id = mode_json_find_mode_id(config, pending_refs.items[i].mode_name);
        if (mode_id == MODE_ID_INVALID) {
            mode_json_free_modes((mode_definition_t *)config->modes, config->mode_count);
            mode_json_free_boot_mode(&config->boot_mode);
            mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
            mode_json_free_wifi_config(&config->wifi);
            free(config);
            mode_json_free_pending_mode_refs(&pending_refs);
            return mode_json_fail_compile_at_code(error,
                                                  "actions[].mode",
                                                  "set_mode references an unknown mode",
                                                  MODE_JSON_ERROR_INVALID_REFERENCE);
        }

        pending_refs.items[i].action->data.mode = mode_id;
    }

    if (!mode_json_validate_runtime_limits(config, error)) {
        mode_json_free_modes((mode_definition_t *)config->modes, config->mode_count);
        mode_json_free_boot_mode(&config->boot_mode);
        mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
        mode_json_free_wifi_config(&config->wifi);
        free(config);
        mode_json_free_pending_mode_refs(&pending_refs);
        return false;
    }

    mode_json_free_pending_mode_refs(&pending_refs);
    *out_config = config;
    return true;
}

static const char *mode_json_find_mode_name_by_id(const mode_config_t *config, mode_id_t mode_id)
{
    if (config == NULL) {
        return NULL;
    }

    if (mode_id == MODE_ID_MOUSE) {
        return "mouse";
    }

    for (size_t i = 0; i < config->mode_count; ++i) {
        if (config->modes[i].id == mode_id) {
            return config->modes[i].name;
        }
    }

    return NULL;
}

static bool mode_json_export_modifiers(mode_json_string_builder_t *builder, mode_hid_modifier_t modifiers)
{
    static const mode_hid_modifier_t ordered_modifiers[] = {
        MODE_HID_MODIFIER_LEFT_CTRL,
        MODE_HID_MODIFIER_LEFT_SHIFT,
        MODE_HID_MODIFIER_LEFT_ALT,
        MODE_HID_MODIFIER_LEFT_GUI,
        MODE_HID_MODIFIER_RIGHT_CTRL,
        MODE_HID_MODIFIER_RIGHT_SHIFT,
        MODE_HID_MODIFIER_RIGHT_ALT,
        MODE_HID_MODIFIER_RIGHT_GUI,
    };

    if (!mode_json_builder_append_text(builder, "\"modifiers\":[")) {
        return false;
    }

    bool first = true;
    for (size_t i = 0; i < sizeof(ordered_modifiers) / sizeof(ordered_modifiers[0]); ++i) {
        if ((modifiers & ordered_modifiers[i]) == 0) {
            continue;
        }

        const char *token = mode_hid_canonical_modifier_token(ordered_modifiers[i]);
        if (token == NULL) {
            return false;
        }

        if (!first && !mode_json_builder_append_char(builder, ',')) {
            return false;
        }
        if (!mode_json_builder_append_json_string(builder, token)) {
            return false;
        }
        first = false;
    }

    return mode_json_builder_append_char(builder, ']');
}

static bool mode_json_export_usage_object(mode_json_string_builder_t *builder, const mode_hid_usage_t *usage)
{
    const char *report = "keyboard";
    if ((builder == NULL) || (usage == NULL)) {
        return false;
    }

    if (usage->report_kind == MODE_HID_REPORT_KIND_CONSUMER) {
        report = "consumer";
    } else if (usage->report_kind == MODE_HID_REPORT_KIND_SYSTEM) {
        report = "system";
    }

    return mode_json_builder_append_text(builder, "{") &&
           mode_json_builder_append_text(builder, "\"report\":") &&
           mode_json_builder_append_json_string(builder, report) &&
           mode_json_builder_append_text(builder, ",\"usagePage\":") &&
           mode_json_builder_append_u32(builder, usage->usage_page) &&
           mode_json_builder_append_text(builder, ",\"usage\":") &&
           mode_json_builder_append_u32(builder, usage->usage_id) &&
           mode_json_builder_append_text(builder, "}");
}

static bool mode_json_export_usage_field(mode_json_string_builder_t *builder,
                                         const char *field_name,
                                         const mode_hid_usage_t *usage)
{
    char token[32];

    if (!mode_json_builder_append_json_string(builder, field_name) ||
        !mode_json_builder_append_char(builder, ':')) {
        return false;
    }

    if (mode_hid_usage_to_canonical_token(usage, token, sizeof(token))) {
        return mode_json_builder_append_json_string(builder, token);
    }

    return mode_json_export_usage_object(builder, usage);
}

static bool mode_json_export_action(mode_json_string_builder_t *builder,
                                    const mode_action_t *action,
                                    const mode_config_t *config)
{
    if ((builder == NULL) || (action == NULL)) {
        return false;
    }

    const char *type = NULL;
    bool export_modifiers = false;
    bool use_key_field = false;
    bool use_usage_field = false;
    mode_hid_usage_t usage = action->data.hid_usage;

    switch (action->type) {
    case MODE_ACTION_HID_KEY_DOWN:
        type = "hid_key_down";
        use_key_field = true;
        export_modifiers = (usage.modifiers != MODE_HID_MODIFIER_NONE);
        break;
    case MODE_ACTION_HID_KEY_UP:
        type = "hid_key_up";
        use_key_field = true;
        export_modifiers = (usage.modifiers != MODE_HID_MODIFIER_NONE);
        break;
    case MODE_ACTION_HID_KEY_TAP:
        if (usage.modifiers != MODE_HID_MODIFIER_NONE) {
            char token[32];
            if (mode_hid_usage_to_canonical_token(&usage, token, sizeof(token))) {
                type = "hid_shortcut_tap";
                use_key_field = true;
                export_modifiers = true;
            } else {
                type = "hid_usage_tap";
                use_usage_field = true;
                export_modifiers = true;
            }
        } else {
            type = "hid_key_tap";
            use_key_field = true;
        }
        break;
    case MODE_ACTION_HID_SHORTCUT_TAP:
    {
        char token[32];
        if (mode_hid_usage_to_canonical_token(&usage, token, sizeof(token))) {
            type = "hid_shortcut_tap";
            use_key_field = true;
        } else {
            type = "hid_usage_tap";
            use_usage_field = true;
        }
        export_modifiers = true;
        break;
    }
    case MODE_ACTION_HID_MODIFIER_DOWN:
        type = "hid_modifier_down";
        break;
    case MODE_ACTION_HID_MODIFIER_UP:
        type = "hid_modifier_up";
        break;
    case MODE_ACTION_HID_USAGE_DOWN:
        type = "hid_usage_down";
        use_usage_field = true;
        export_modifiers = (usage.modifiers != MODE_HID_MODIFIER_NONE);
        break;
    case MODE_ACTION_HID_USAGE_UP:
        type = "hid_usage_up";
        use_usage_field = true;
        export_modifiers = (usage.modifiers != MODE_HID_MODIFIER_NONE);
        break;
    case MODE_ACTION_HID_USAGE_TAP:
        type = "hid_usage_tap";
        use_usage_field = true;
        export_modifiers = (usage.modifiers != MODE_HID_MODIFIER_NONE);
        break;
    case MODE_ACTION_SLEEP_MS:
        return mode_json_builder_append_text(builder, "{\"type\":\"sleep_ms\",\"duration_ms\":") &&
               mode_json_builder_append_u32(builder, action->data.duration_ms) &&
               mode_json_builder_append_char(builder, '}');
    case MODE_ACTION_ENTER_BOOT_MODE:
        return mode_json_builder_append_text(builder, "{\"type\":\"enter_boot_mode\"}");
    case MODE_ACTION_EXIT_BOOT_MODE:
        return mode_json_builder_append_text(builder, "{\"type\":\"exit_boot_mode\"}");
    case MODE_ACTION_MIC_GATE:
        return mode_json_builder_append_text(builder, "{\"type\":\"mic_gate\",\"enabled\":") &&
               mode_json_builder_append_bool(builder, action->data.enabled) &&
               mode_json_builder_append_char(builder, '}');
    case MODE_ACTION_MIC_GATE_TOGGLE:
        return mode_json_builder_append_text(builder, "{\"type\":\"mic_gate_toggle\"}");
    case MODE_ACTION_UI_HINT:
        return mode_json_builder_append_text(builder, "{\"type\":\"ui_hint\",\"text\":") &&
               mode_json_builder_append_json_string(builder, action->data.text != NULL ? action->data.text : "") &&
               mode_json_builder_append_char(builder, '}');
    case MODE_ACTION_UI_SHOW_MODE:
        return mode_json_builder_append_text(builder, "{\"type\":\"ui_show_mode\"}");
    case MODE_ACTION_SET_MODE: {
        const char *mode_name = mode_json_find_mode_name_by_id(config, action->data.mode);
        if (mode_name == NULL) {
            return false;
        }
        return mode_json_builder_append_text(builder, "{\"type\":\"set_mode\",\"mode\":") &&
               mode_json_builder_append_json_string(builder, mode_name) &&
               mode_json_builder_append_char(builder, '}');
    }
    case MODE_ACTION_CYCLE_MODE:
        return mode_json_builder_append_text(builder, "{\"type\":\"cycle_mode\",\"direction\":") &&
               mode_json_builder_append_json_string(builder,
                                                    (action->data.direction == MODE_CYCLE_DIRECTION_PREVIOUS)
                                                        ? "previous"
                                                        : "next") &&
               mode_json_builder_append_char(builder, '}');
    case MODE_ACTION_NOOP:
        return mode_json_builder_append_text(builder, "{\"type\":\"noop\"}");
    default:
        return false;
    }

    if (!mode_json_builder_append_text(builder, "{\"type\":") ||
        !mode_json_builder_append_json_string(builder, type)) {
        return false;
    }

    if (use_key_field) {
        if (!mode_json_builder_append_char(builder, ',') ||
            !mode_json_export_usage_field(builder, "key", &usage)) {
            return false;
        }
    }

    if (use_usage_field) {
        if (!mode_json_builder_append_char(builder, ',') ||
            !mode_json_export_usage_field(builder, "usage", &usage)) {
            return false;
        }
    }

    if ((action->type == MODE_ACTION_HID_MODIFIER_DOWN) || (action->type == MODE_ACTION_HID_MODIFIER_UP)) {
        if (!mode_json_builder_append_char(builder, ',') ||
            !mode_json_export_modifiers(builder, action->data.modifier)) {
            return false;
        }
    } else if (export_modifiers) {
        if (!mode_json_builder_append_char(builder, ',') ||
            !mode_json_export_modifiers(builder, usage.modifiers)) {
            return false;
        }
    }

    return mode_json_builder_append_char(builder, '}');
}

static bool mode_json_export_bindings(mode_json_string_builder_t *builder,
                                      const mode_binding_t *bindings,
                                      size_t binding_count,
                                      const mode_config_t *config)
{
    static const char *const input_names[] = {
        "boot_button", "touch", "encoder", "usb_host_key", "timer", "imu"
    };
    static const char *const trigger_names[] = {
        "press", "release", "tap", "double_tap", "long_press",
        "hold_start", "hold_end", "swipe_up", "swipe_down", "swipe_left", "swipe_right"
    };

    if (!mode_json_builder_append_char(builder, '[')) {
        return false;
    }

    for (size_t i = 0; i < binding_count; ++i) {
        if ((bindings[i].input > MODE_INPUT_IMU) || (bindings[i].trigger > MODE_TRIGGER_SWIPE_RIGHT)) {
            return false;
        }
        if ((i > 0) && !mode_json_builder_append_char(builder, ',')) {
            return false;
        }
        if (!mode_json_builder_append_text(builder, "{\"input\":") ||
            !mode_json_builder_append_json_string(builder, input_names[bindings[i].input]) ||
            !mode_json_builder_append_text(builder, ",\"trigger\":") ||
            !mode_json_builder_append_json_string(builder, trigger_names[bindings[i].trigger]) ||
            !mode_json_builder_append_text(builder, ",\"actions\":[")) {
            return false;
        }

        for (size_t j = 0; j < bindings[i].action_count; ++j) {
            if ((j > 0) && !mode_json_builder_append_char(builder, ',')) {
                return false;
            }
            if (!mode_json_export_action(builder, &bindings[i].actions[j], config)) {
                return false;
            }
        }

        if (!mode_json_builder_append_text(builder, "]}")) {
            return false;
        }
    }

    return mode_json_builder_append_char(builder, ']');
}

static bool mode_json_boot_ui_has_fields(const mode_boot_ui_t *ui)
{
    return (ui != NULL) &&
           ((ui->title != NULL) ||
            (ui->subtitle != NULL) ||
            ui->show_mode_list ||
            ui->show_gesture_hints ||
            ui->show_current_mode_card);
}

static bool mode_json_export_wifi(mode_json_string_builder_t *builder, const mode_wifi_config_t *wifi)
{
    if ((builder == NULL) || (wifi == NULL)) {
        return false;
    }

    return mode_json_builder_append_text(builder, ",\"wifi\":{\"sta\":{\"ssid\":") &&
           mode_json_builder_append_json_string(builder, (wifi->sta.ssid != NULL) ? wifi->sta.ssid : "") &&
           mode_json_builder_append_text(builder, ",\"password\":") &&
           mode_json_builder_append_json_string(builder, (wifi->sta.password != NULL) ? wifi->sta.password : "") &&
           mode_json_builder_append_text(builder, "},\"ap\":{\"ssid\":") &&
           mode_json_builder_append_json_string(builder, (wifi->ap.ssid != NULL) ? wifi->ap.ssid : "") &&
           mode_json_builder_append_text(builder, ",\"password\":") &&
           mode_json_builder_append_json_string(builder, (wifi->ap.password != NULL) ? wifi->ap.password : "") &&
           mode_json_builder_append_text(builder, "},\"hostname\":") &&
           mode_json_builder_append_json_string(builder, (wifi->hostname != NULL) ? wifi->hostname : "") &&
           mode_json_builder_append_text(builder, ",\"localUrl\":") &&
           mode_json_builder_append_json_string(builder, (wifi->local_url != NULL) ? wifi->local_url : "") &&
           mode_json_builder_append_char(builder, '}');
}

char *mode_json_export_canonical_string(const mode_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    mode_json_string_builder_t builder = {0};
    const char *active_mode_name = mode_json_find_mode_name_by_id(config, config->active_mode);
    if (active_mode_name == NULL) {
        return NULL;
    }

    bool ok = mode_json_builder_append_char(&builder, '{') &&
              mode_json_builder_append_text(&builder, "\"version\":") &&
              mode_json_builder_append_u32(&builder, config->version) &&
              mode_json_builder_append_text(&builder, ",\"activeMode\":") &&
              mode_json_builder_append_json_string(&builder, active_mode_name) &&
              mode_json_builder_append_text(&builder,
                                            ",\"defaults\":{\"touch\":{\"holdMs\":") &&
              mode_json_builder_append_u32(&builder, config->defaults.touch.hold_ms) &&
              mode_json_builder_append_text(&builder, ",\"doubleTapMs\":") &&
              mode_json_builder_append_u32(&builder, config->defaults.touch.double_tap_ms) &&
              mode_json_builder_append_text(&builder, ",\"swipeMinDistance\":") &&
              mode_json_builder_append_u32(&builder, config->defaults.touch.swipe_min_distance) &&
              mode_json_builder_append_text(&builder, "}}") &&
              mode_json_export_wifi(&builder, &config->wifi) &&
              mode_json_builder_append_text(&builder, ",\"globalBindings\":") &&
              mode_json_export_bindings(&builder,
                                        config->global_bindings,
                                        config->global_binding_count,
                                        config);
    if (!ok) {
        free(builder.data);
        return NULL;
    }

    if (!mode_json_builder_append_text(&builder, ",\"bootMode\":{\"label\":") ||
        !mode_json_builder_append_json_string(&builder,
                                              config->boot_mode.label != NULL ? config->boot_mode.label : "")) {
        free(builder.data);
        return NULL;
    }

    if (mode_json_boot_ui_has_fields(&config->boot_mode.ui)) {
        if (!mode_json_builder_append_text(&builder, ",\"ui\":{")) {
            free(builder.data);
            return NULL;
        }

        bool first_field = true;
        if (config->boot_mode.ui.title != NULL) {
            ok = mode_json_builder_append_text(&builder, "\"title\":") &&
                 mode_json_builder_append_json_string(&builder, config->boot_mode.ui.title);
            first_field = false;
        } else {
            ok = true;
        }
        if (ok && (config->boot_mode.ui.subtitle != NULL)) {
            ok = (!first_field ? mode_json_builder_append_char(&builder, ',') : true) &&
                 mode_json_builder_append_text(&builder, "\"subtitle\":") &&
                 mode_json_builder_append_json_string(&builder, config->boot_mode.ui.subtitle);
            first_field = false;
        }
        if (ok && config->boot_mode.ui.show_mode_list) {
            ok = (!first_field ? mode_json_builder_append_char(&builder, ',') : true) &&
                 mode_json_builder_append_text(&builder, "\"showModeList\":true");
            first_field = false;
        }
        if (ok && config->boot_mode.ui.show_gesture_hints) {
            ok = (!first_field ? mode_json_builder_append_char(&builder, ',') : true) &&
                 mode_json_builder_append_text(&builder, "\"showGestureHints\":true");
            first_field = false;
        }
        if (ok && config->boot_mode.ui.show_current_mode_card) {
            ok = (!first_field ? mode_json_builder_append_char(&builder, ',') : true) &&
                 mode_json_builder_append_text(&builder, "\"showCurrentModeCard\":true");
        }
        if (!ok || !mode_json_builder_append_char(&builder, '}')) {
            free(builder.data);
            return NULL;
        }
    }

    if (!mode_json_builder_append_text(&builder, ",\"bindings\":") ||
        !mode_json_export_bindings(&builder,
                                   config->boot_mode.bindings,
                                   config->boot_mode.binding_count,
                                   config) ||
        !mode_json_builder_append_text(&builder, "},\"modes\":[")) {
        free(builder.data);
        return NULL;
    }

    for (size_t i = 0; i < config->mode_count; ++i) {
        if ((i > 0) && !mode_json_builder_append_char(&builder, ',')) {
            free(builder.data);
            return NULL;
        }

        if (!mode_json_builder_append_text(&builder, "{\"id\":") ||
            !mode_json_builder_append_json_string(&builder,
                                                 config->modes[i].name != NULL ? config->modes[i].name : "") ||
            !mode_json_builder_append_text(&builder, ",\"cycleOrder\":") ||
            !mode_json_builder_append_u32(&builder, config->modes[i].cycle_order) ||
            !mode_json_builder_append_text(&builder, ",\"label\":") ||
            !mode_json_builder_append_json_string(&builder,
                                                  config->modes[i].label != NULL ? config->modes[i].label : "") ||
            !mode_json_builder_append_text(&builder, ",\"bindings\":") ||
            !mode_json_export_bindings(&builder,
                                       config->modes[i].bindings,
                                       config->modes[i].binding_count,
                                       config) ||
            !mode_json_builder_append_char(&builder, '}')) {
            free(builder.data);
            return NULL;
        }
    }

    if (!mode_json_builder_append_text(&builder, "]}")) {
        free(builder.data);
        return NULL;
    }

    return builder.data;
}

bool mode_json_load_from_string(const char *json_text,
                                mode_config_t **out_config,
                                mode_json_error_t *error)
{
    if (error != NULL) {
        error->offset = 0;
        error->path[0] = '\0';
        error->message[0] = '\0';
        error->code = MODE_JSON_ERROR_NONE;
    }

    if ((json_text == NULL) || (out_config == NULL)) {
        return mode_json_fail_compile(error, "mode_json_load_from_string requires input and output pointers");
    }

    *out_config = NULL;

    mode_json_parser_t parser = {
        .start = json_text,
        .cursor = json_text,
        .error = error,
    };

    mode_json_value_t *root = mode_json_parse_value(&parser);
    if (root == NULL) {
        return false;
    }

    mode_json_skip_ws(&parser);
    if (*parser.cursor != '\0') {
        mode_json_free_value(root);
        return mode_json_fail(&parser, "Trailing JSON content");
    }

    mode_config_t *config = NULL;
    bool ok = mode_json_compile_config(root, &config, error);
    mode_json_free_value(root);
    if (!ok) {
        return false;
    }

    *out_config = config;
    return true;
}

void mode_json_free_config(mode_config_t *config)
{
    if (config == NULL) {
        return;
    }

    mode_json_free_modes((mode_definition_t *)config->modes, config->mode_count);
    mode_json_free_boot_mode(&config->boot_mode);
    mode_json_free_bindings((mode_binding_t *)config->global_bindings, config->global_binding_count);
    mode_json_free_wifi_config(&config->wifi);
    free(config);
}
