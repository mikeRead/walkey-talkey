#include "config_api_service.h"

#include <stdlib.h>

static void config_api_service_set_error(config_api_service_result_t *result,
                                         config_api_service_status_t status)
{
    if (result == NULL) {
        return;
    }

    result->status = status;
}

static void config_api_service_capture_storage_error(config_api_service_result_t *result)
{
    if (result == NULL) {
        return;
    }

    const mode_config_storage_error_t *storage_error = mode_config_last_storage_error();
    if (storage_error != NULL) {
        result->storage_error = *storage_error;
    }
}

void config_api_service_result_clear(config_api_service_result_t *result)
{
    if (result == NULL) {
        return;
    }

    free(result->json_text);
    result->status = CONFIG_API_SERVICE_OK;
    result->source = MODE_CONFIG_SOURCE_FAILSAFE;
    result->json_text = NULL;
    result->json_error = (mode_json_error_t){0};
    result->storage_error = (mode_config_storage_error_t){0};
}

bool config_api_service_export_active(config_api_service_result_t *result)
{
    if (result == NULL) {
        return false;
    }

    config_api_service_result_clear(result);
    result->json_text = NULL;
    if (!mode_config_read_active_json(&result->json_text, &result->source)) {
        config_api_service_set_error(result, CONFIG_API_SERVICE_EXPORT_FAILED);
        return false;
    }
    return true;
}

bool config_api_service_export_active_canonical(config_api_service_result_t *result)
{
    if (result == NULL) {
        return false;
    }

    config_api_service_result_clear(result);
    result->source = mode_config_get_source();
    result->json_text = mode_json_export_canonical_string(mode_config_get());
    if (result->json_text == NULL) {
        config_api_service_set_error(result, CONFIG_API_SERVICE_EXPORT_FAILED);
        return false;
    }
    return true;
}

bool config_api_service_validate_json(const char *json_text, config_api_service_result_t *result)
{
    if ((json_text == NULL) || (result == NULL)) {
        if (result != NULL) {
            config_api_service_result_clear(result);
            config_api_service_set_error(result, CONFIG_API_SERVICE_INVALID_ARGUMENT);
        }
        return false;
    }

    config_api_service_result_clear(result);

    mode_config_t *config = NULL;
    if (!mode_json_load_from_string(json_text, &config, &result->json_error)) {
        config_api_service_set_error(result, CONFIG_API_SERVICE_PARSE_FAILED);
        return false;
    }

    result->json_text = mode_json_export_canonical_string(config);
    mode_json_free_config(config);
    if (result->json_text == NULL) {
        config_api_service_set_error(result, CONFIG_API_SERVICE_EXPORT_FAILED);
        return false;
    }

    return true;
}

bool config_api_service_save_json(const char *json_text, config_api_service_result_t *result)
{
    if (!config_api_service_validate_json(json_text, result)) {
        return false;
    }

    if (!mode_config_write_external_json(result->json_text)) {
        config_api_service_capture_storage_error(result);
        config_api_service_set_error(result, CONFIG_API_SERVICE_STORAGE_FAILED);
        return false;
    }

    result->source = MODE_CONFIG_SOURCE_EXTERNAL;
    return true;
}

bool config_api_service_restore_builtin(config_api_service_result_t *result)
{
    if (!config_api_service_validate_json(mode_config_builtin_json(), result)) {
        return false;
    }

    if (!mode_config_write_external_json(result->json_text)) {
        config_api_service_capture_storage_error(result);
        config_api_service_set_error(result, CONFIG_API_SERVICE_STORAGE_FAILED);
        return false;
    }

    result->source = MODE_CONFIG_SOURCE_BUILTIN;
    return true;
}
