#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mode_config.h"
#include "mode_hid_tokens.h"
#include "mode_json_loader.h"

static void expect(bool condition, const char *message)
{
    if (condition) {
        return;
    }

    fprintf(stderr, "test failure: %s\n", message);
    exit(1);
}

static const mode_definition_t *find_mode(const mode_config_t *config, const char *name)
{
    for (size_t i = 0; i < config->mode_count; ++i) {
        if ((config->modes[i].name != NULL) && (strcmp(config->modes[i].name, name) == 0)) {
            return &config->modes[i];
        }
    }

    return NULL;
}

static const mode_binding_t *find_binding(const mode_definition_t *mode, mode_trigger_t trigger)
{
    for (size_t i = 0; i < mode->binding_count; ++i) {
        if ((mode->bindings[i].input == MODE_INPUT_TOUCH) && (mode->bindings[i].trigger == trigger)) {
            return &mode->bindings[i];
        }
    }

    return NULL;
}

static void test_builtin_json_compiles_expected_bindings(void)
{
    mode_config_t *config = NULL;
    mode_json_error_t error = {0};

    expect(mode_json_load_from_string(mode_config_builtin_json(), &config, &error),
           "built-in JSON should compile");
    expect(config != NULL, "built-in JSON should return a config");
    expect(config->mode_count == 4, "built-in JSON should define four modes");
    expect(config->global_binding_count == 2, "built-in JSON should keep boot globals");
    expect(config->boot_mode.binding_count == 4, "built-in JSON should include boot tap/swipe/long-press bindings");

    const mode_definition_t *cursor = find_mode(config, "cursor");
    expect(cursor != NULL, "cursor mode should exist");

    const mode_binding_t *double_tap = find_binding(cursor, MODE_TRIGGER_DOUBLE_TAP);
    expect(double_tap != NULL, "cursor double_tap binding should exist");
    expect(double_tap->actions[0].type == MODE_ACTION_HID_KEY_TAP, "cursor double_tap should compile to key tap");
    expect(double_tap->actions[0].data.hid_usage.usage_id == MODE_KEY_ENTER, "cursor double_tap should send enter");

    const mode_binding_t *swipe_left = find_binding(cursor, MODE_TRIGGER_SWIPE_LEFT);
    expect(swipe_left != NULL, "cursor swipe_left binding should exist");
    expect(swipe_left->actions[0].type == MODE_ACTION_HID_SHORTCUT_TAP,
           "CTRL+N should compile into hid_shortcut_tap");
    expect(swipe_left->actions[0].data.hid_usage.modifiers == MODE_HID_MODIFIER_LEFT_CTRL,
           "CTRL+N should keep CTRL modifier");
    expect(swipe_left->actions[0].data.hid_usage.usage_id == MODE_KEY_N,
           "CTRL+N should keep N key");

    mode_json_free_config(config);
}

static void test_invalid_set_mode_is_rejected(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"activeMode\":\"cursor\","
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":{"
        "\"cursor\":{\"label\":\"Cursor\",\"bindings\":["
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"set_mode\",\"mode\":\"missing\"}]}"
        "]}"
        "}"
        "}";

    mode_config_t *config = NULL;
    mode_json_error_t error = {0};
    expect(!mode_json_load_from_string(json_text, &config, &error),
           "unknown set_mode target should fail");
    expect(config == NULL, "failed parse should not return a config");
}

static void test_modes_array_and_usage_actions_compile(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"activeMode\":\"media\","
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":["
        "{"
        "\"id\":\"media\","
        "\"cycleOrder\":1,"
        "\"label\":\"Media\","
        "\"bindings\":["
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"hid_usage_tap\",\"usage\":\"VOLUME_UP\"}]}"
        "]"
        "},"
        "{"
        "\"id\":\"cursor\","
        "\"cycleOrder\":0,"
        "\"label\":\"Cursor\","
        "\"bindings\":["
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"hid_shortcut_tap\",\"modifiers\":[\"CTRL\",\"SHIFT\"],\"key\":\"A\"}]}"
        "]"
        "}"
        "]"
        "}";

    mode_config_t *config = NULL;
    mode_json_error_t error = {0};
    expect(mode_json_load_from_string(json_text, &config, &error),
           "modes array and hid usage actions should compile");
    expect(strcmp(config->modes[0].name, "cursor") == 0,
           "cycleOrder should control runtime mode ordering");

    const mode_definition_t *media = find_mode(config, "media");
    expect(media != NULL, "media mode should exist");
    expect(media->bindings[0].actions[0].type == MODE_ACTION_HID_USAGE_TAP,
           "hid_usage_tap should stay distinct");
    expect(media->bindings[0].actions[0].data.hid_usage.report_kind == MODE_HID_REPORT_KIND_CONSUMER,
           "volume token should compile to consumer HID");

    const mode_definition_t *cursor = find_mode(config, "cursor");
    expect(cursor != NULL, "cursor mode should exist");
    expect(cursor->bindings[0].actions[0].data.hid_usage.modifiers ==
               (MODE_HID_MODIFIER_LEFT_CTRL | MODE_HID_MODIFIER_LEFT_SHIFT),
           "modifier arrays should compile into a bitmask");

    mode_json_free_config(config);
}

static void test_duplicate_keys_are_rejected(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"version\":2,"
        "\"activeMode\":\"cursor\","
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":{\"cursor\":{\"label\":\"Cursor\",\"bindings\":[]}}"
        "}";

    mode_config_t *config = NULL;
    mode_json_error_t error = {0};
    expect(!mode_json_load_from_string(json_text, &config, &error),
           "duplicate JSON keys should be rejected");
    expect(config == NULL, "duplicate-key parse should not return a config");
    expect(error.code == MODE_JSON_ERROR_DUPLICATE_KEY, "duplicate keys should return a structured error code");
}

static void test_canonical_export_normalizes_legacy_forms(void)
{
    mode_config_t *config = NULL;
    mode_json_error_t error = {0};
    expect(mode_json_load_from_string(mode_config_builtin_json(), &config, &error),
           "built-in JSON should load before export");

    char *canonical_json = mode_json_export_canonical_string(config);
    expect(canonical_json != NULL, "canonical export should return JSON");
    expect(strstr(canonical_json, "\"modes\":[") != NULL, "canonical export should emit modes as an array");
    expect(strstr(canonical_json, "\"modifier\":") == NULL, "canonical export should not emit singular modifier fields");
    expect(strstr(canonical_json, "CTRL+N") == NULL, "canonical export should normalize embedded shortcut strings");
    expect(strstr(canonical_json, "CTRL+.") == NULL, "canonical export should normalize punctuation shortcuts");

    mode_config_t *roundtrip = NULL;
    expect(mode_json_load_from_string(canonical_json, &roundtrip, &error),
           "canonical export should be parseable");

    free(canonical_json);
    mode_json_free_config(roundtrip);
    mode_json_free_config(config);
}

static void test_wifi_config_fields_compile_and_export(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"activeMode\":\"cursor\","
        "\"wifi\":{"
        "\"sta\":{\"ssid\":\"OfficeNet\",\"password\":\"secretpass\"},"
        "\"ap\":{\"ssid\":\"walkey-talkey\",\"password\":\"secretKEY\"},"
        "\"hostname\":\"desk-walkie\","
        "\"localUrl\":\"desk-walkie.local\""
        "},"
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":[{\"id\":\"cursor\",\"label\":\"Cursor\",\"bindings\":[]}]"
        "}";

    mode_config_t *config = NULL;
    mode_json_error_t error = {0};
    expect(mode_json_load_from_string(json_text, &config, &error),
           "wifi config fields should compile");
    expect(strcmp(config->wifi.sta.ssid, "OfficeNet") == 0, "STA ssid should compile");
    expect(strcmp(config->wifi.ap.ssid, "walkey-talkey") == 0, "AP ssid should compile");
    expect(strcmp(config->wifi.hostname, "desk-walkie") == 0, "hostname should compile");
    expect(strcmp(config->wifi.local_url, "desk-walkie.local") == 0, "localUrl should compile");

    char *canonical_json = mode_json_export_canonical_string(config);
    expect(canonical_json != NULL, "wifi config should export");
    expect(strstr(canonical_json, "\"hostname\":\"desk-walkie\"") != NULL,
           "canonical export should keep hostname");
    expect(strstr(canonical_json, "\"localUrl\":\"desk-walkie.local\"") != NULL,
           "canonical export should keep localUrl");

    free(canonical_json);
    mode_json_free_config(config);
}

static void test_runtime_binding_limit_is_enforced(void)
{
    const char *json_text =
        "{"
        "\"version\":1,"
        "\"activeMode\":\"cursor\","
        "\"globalBindings\":[],"
        "\"bootMode\":{\"label\":\"Mode Control\",\"bindings\":[]},"
        "\"modes\":["
        "{"
        "\"id\":\"cursor\","
        "\"label\":\"Cursor\","
        "\"bindings\":["
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]},"
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]},"
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]},"
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]},"
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]},"
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]},"
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]},"
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]},"
        "{\"input\":\"touch\",\"trigger\":\"tap\",\"actions\":[{\"type\":\"noop\"}]}"
        "]"
        "}"
        "]"
        "}";

    mode_config_t *config = NULL;
    mode_json_error_t error = {0};
    expect(!mode_json_load_from_string(json_text, &config, &error),
           "configs that exceed the binding match limit should fail");
    expect(config == NULL, "runtime-limit parse should not return a config");
    expect(error.code == MODE_JSON_ERROR_RUNTIME_LIMIT, "runtime limit should use a structured error code");
}

static void test_hid_catalog_exposes_canonical_aliases(void)
{
    expect(mode_hid_modifier_token_count() > 0, "modifier catalog should be exposed");
    expect(mode_hid_usage_token_count() > 0, "usage catalog should be exposed");

    const mode_hid_modifier_token_t *modifier_entry = mode_hid_modifier_token_at(0);
    expect(modifier_entry != NULL, "modifier catalog should provide entries");
    expect(modifier_entry->canonical_token != NULL, "modifier catalog entries should expose canonical tokens");

    mode_hid_usage_t usage = {0};
    expect(mode_hid_parse_usage_token("NEXT_TRACK", &usage), "usage alias should parse");

    char token[32];
    expect(mode_hid_usage_to_canonical_token(&usage, token, sizeof(token)),
           "canonical usage token export should succeed");
    expect(strcmp(token, "MEDIA_NEXT_TRACK") == 0, "usage aliases should round-trip to canonical tokens");
}

int main(void)
{
    test_builtin_json_compiles_expected_bindings();
    test_invalid_set_mode_is_rejected();
    test_modes_array_and_usage_actions_compile();
    test_duplicate_keys_are_rejected();
    test_canonical_export_normalizes_legacy_forms();
    test_wifi_config_fields_compile_and_export();
    test_runtime_binding_limit_is_enforced();
    test_hid_catalog_exposes_canonical_aliases();

    puts("mode_json_loader tests passed");
    return 0;
}
