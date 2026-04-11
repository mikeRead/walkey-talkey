#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "mode_types.h"

typedef enum {
    MODE_JSON_ERROR_NONE = 0,
    MODE_JSON_ERROR_PARSE,
    MODE_JSON_ERROR_DUPLICATE_KEY,
    MODE_JSON_ERROR_UNKNOWN_FIELD,
    MODE_JSON_ERROR_OUT_OF_MEMORY,
    MODE_JSON_ERROR_INVALID_VALUE,
    MODE_JSON_ERROR_INVALID_REFERENCE,
    MODE_JSON_ERROR_RUNTIME_LIMIT,
} mode_json_error_code_t;

typedef struct {
    size_t offset;
    char path[128];
    char message[128];
    mode_json_error_code_t code;
} mode_json_error_t;

bool mode_json_load_from_string(const char *json_text,
                                mode_config_t **out_config,
                                mode_json_error_t *error);
char *mode_json_export_canonical_string(const mode_config_t *config);
void mode_json_free_config(mode_config_t *config);
