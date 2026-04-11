#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_api_service.h"
#include "mode_config.h"

#if defined(_WIN32)
#include <direct.h>
#define TEST_MKDIR(path) _mkdir(path)
#define TEST_RMDIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define TEST_MKDIR(path) mkdir(path, 0777)
#define TEST_RMDIR(path) rmdir(path)
#endif

static void expect(bool condition, const char *message)
{
    if (condition) {
        return;
    }

    fprintf(stderr, "test failure: %s\n", message);
    exit(1);
}

static void cleanup_external_json(void)
{
    (void)remove("mode-config.json");
    (void)TEST_RMDIR("mode-config.json");
    mode_config_reset();
}

static void test_validate_returns_canonical_json(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"activeMode\":\"cursor\","
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":{\"cursor\":{\"label\":\"Cursor\",\"bindings\":["
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"hid_key_tap\",\"key\":\"CTRL+N\"}]}"
        "]}}"
        "}";

    config_api_service_result_t result = {0};
    expect(config_api_service_validate_json(json_text, &result),
           "validate should succeed for legacy-compatible input");
    expect(result.json_text != NULL, "validate should return canonical json");
    expect(strstr(result.json_text, "\"modes\":[") != NULL, "validate should normalize modes to an array");
    expect(strstr(result.json_text, "CTRL+N") == NULL, "validate should normalize embedded shortcuts");
    expect(strstr(result.json_text, "\"sta\":{\"ssid\":\"YourNetworkName\",\"password\":\"YourPassword\"}") != NULL,
           "validate should inject default STA settings");
    expect(strstr(result.json_text, "\"ap\":{\"ssid\":\"walkey-talkey\",\"password\":\"secretKEY\"}") != NULL,
           "validate should inject default fallback AP settings");
    expect(strstr(result.json_text, "\"localUrl\":\"walkey-talkey.local\"") != NULL,
           "validate should inject the default localUrl");
    config_api_service_result_clear(&result);
}

static void test_validate_preserves_wifi_settings(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"activeMode\":\"cursor\","
        "\"wifi\":{"
        "\"sta\":{\"ssid\":\"OfficeNet\",\"password\":\"secretpass\"},"
        "\"hostname\":\"desk-walkie\","
        "\"localUrl\":\"desk-walkie.local\""
        "},"
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":[{\"id\":\"cursor\",\"label\":\"Cursor\",\"bindings\":[]}]"
        "}";

    config_api_service_result_t result = {0};
    expect(config_api_service_validate_json(json_text, &result),
           "validate should keep explicit wifi settings");
    expect(strstr(result.json_text, "\"ssid\":\"OfficeNet\"") != NULL,
           "validate should keep STA ssid");
    expect(strstr(result.json_text, "\"localUrl\":\"desk-walkie.local\"") != NULL,
           "validate should keep localUrl");
    config_api_service_result_clear(&result);
}

static void test_save_writes_external_json(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"activeMode\":\"cursor\","
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":[{\"id\":\"cursor\",\"label\":\"Cursor\",\"bindings\":[]}]"
        "}";

    config_api_service_result_t result = {0};
    cleanup_external_json();
    expect(config_api_service_save_json(json_text, &result),
           "save should validate and write external json");
    expect(result.json_text != NULL, "save should return canonical json");
    config_api_service_result_clear(&result);

    expect(mode_config_reload(), "reload after save should succeed");
    expect(mode_config_get_source() == MODE_CONFIG_SOURCE_EXTERNAL,
           "saved json should become the active external source");
    cleanup_external_json();
}

static void test_restore_builtin_writes_reset_target(void)
{
    config_api_service_result_t result = {0};
    cleanup_external_json();
    expect(config_api_service_restore_builtin(&result),
           "restore builtin should canonicalize and write the built-in config");
    expect(result.source == MODE_CONFIG_SOURCE_BUILTIN,
           "restore builtin result should report builtin as the reset target");
    config_api_service_result_clear(&result);

    expect(mode_config_reload(), "reload after restoring built-in should succeed");
    expect(mode_config_get_source() == MODE_CONFIG_SOURCE_EXTERNAL,
           "reloaded built-in reset should come from the external persisted file");
    cleanup_external_json();
}

static void test_save_reports_helpful_storage_error(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"activeMode\":\"cursor\","
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":[{\"id\":\"cursor\",\"label\":\"Cursor\",\"bindings\":[]}]"
        "}";

    config_api_service_result_t result = {0};
    cleanup_external_json();
    expect(TEST_MKDIR("mode-config.json") == 0, "test setup should create directory that blocks file writes");
    expect(!config_api_service_save_json(json_text, &result),
           "save should fail when the external config path is a directory");
    expect(result.status == CONFIG_API_SERVICE_STORAGE_FAILED,
           "save should report storage failure when writing is blocked");
    expect(result.storage_error.stage == MODE_CONFIG_STORAGE_STAGE_OPEN,
           "blocked writes should report the open stage");
    expect(strstr(result.storage_error.path, "mode-config.json") != NULL,
           "storage failure should name the blocked config path");
    expect(result.storage_error.message[0] != '\0',
           "storage failure should include a helpful message");
    config_api_service_result_clear(&result);
    cleanup_external_json();
}

int main(void)
{
    test_validate_returns_canonical_json();
    test_validate_preserves_wifi_settings();
    test_save_writes_external_json();
    test_restore_builtin_writes_reset_target();
    test_save_reports_helpful_storage_error();

    puts("config_api_service tests passed");
    return 0;
}
