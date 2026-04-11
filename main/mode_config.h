#pragma once

#include <stdbool.h>

#include "mode_types.h"

#define MODE_CONFIG_MAX_MATCHED_BINDINGS 8

typedef enum {
    MODE_CONFIG_SOURCE_FAILSAFE = 0,
    MODE_CONFIG_SOURCE_BUILTIN,
    MODE_CONFIG_SOURCE_EXTERNAL,
} mode_config_source_t;

typedef enum {
    MODE_CONFIG_STORAGE_STAGE_NONE = 0,
    MODE_CONFIG_STORAGE_STAGE_MOUNT,
    MODE_CONFIG_STORAGE_STAGE_OPEN,
    MODE_CONFIG_STORAGE_STAGE_WRITE,
    MODE_CONFIG_STORAGE_STAGE_FLUSH,
} mode_config_storage_stage_t;

typedef struct {
    mode_config_storage_stage_t stage;
    bool format_attempted;
    int errno_value;
    char path[96];
    char partition_label[24];
    char esp_error[32];
    char errno_message[96];
    char message[256];
} mode_config_storage_error_t;

bool mode_config_init(void);
bool mode_config_reload(void);
void mode_config_reset(void);
const mode_config_t *mode_config_get(void);
const mode_definition_t *mode_config_find_mode(mode_id_t mode);
mode_config_source_t mode_config_get_source(void);
const char *mode_config_source_name(mode_config_source_t source);
const char *mode_config_mode_label(mode_id_t mode);
const char *mode_config_builtin_json(void);
bool mode_config_read_active_json(char **out_text, mode_config_source_t *out_source);
bool mode_config_write_external_json(const char *json_text);
const mode_config_storage_error_t *mode_config_last_storage_error(void);
