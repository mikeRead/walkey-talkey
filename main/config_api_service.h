#pragma once

#include <stdbool.h>

#include "mode_config.h"
#include "mode_json_loader.h"

typedef enum {
    CONFIG_API_SERVICE_OK = 0,
    CONFIG_API_SERVICE_INVALID_ARGUMENT,
    CONFIG_API_SERVICE_PARSE_FAILED,
    CONFIG_API_SERVICE_EXPORT_FAILED,
    CONFIG_API_SERVICE_STORAGE_FAILED,
} config_api_service_status_t;

typedef struct {
    config_api_service_status_t status;
    mode_config_source_t source;
    mode_json_error_t json_error;
    mode_config_storage_error_t storage_error;
    char *json_text;
} config_api_service_result_t;

void config_api_service_result_clear(config_api_service_result_t *result);
bool config_api_service_export_active(config_api_service_result_t *result);
bool config_api_service_export_active_canonical(config_api_service_result_t *result);
bool config_api_service_validate_json(const char *json_text, config_api_service_result_t *result);
bool config_api_service_save_json(const char *json_text, config_api_service_result_t *result);
bool config_api_service_restore_builtin(config_api_service_result_t *result);
