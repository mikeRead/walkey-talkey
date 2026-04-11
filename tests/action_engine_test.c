#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "action_engine.h"
#include "mode_config.h"

typedef struct {
    int key_down_count;
    int key_up_count;
    int mic_gate_changes;
    int hint_changes;
    int refresh_count;
    int reset_count;
    bool mic_enabled;
    bool fail_hid_send;
    mode_keycode_t last_keycode;
    const char *last_hint;
} action_test_state_t;

static void expect(bool condition, const char *message)
{
    if (condition) {
        return;
    }

    fprintf(stderr, "test failure: %s\n", message);
    exit(1);
}

static bool test_send_hid_key(bool pressed,
                              const mode_hid_usage_t *usage,
                              void *user_data)
{
    action_test_state_t *state = (action_test_state_t *)user_data;
    if (usage == NULL) {
        return false;
    }
    if (state->fail_hid_send) {
        return false;
    }

    state->last_keycode = usage->usage_id;
    if (pressed) {
        state->key_down_count++;
    } else {
        state->key_up_count++;
    }
    return true;
}

static void test_set_mic_gate(bool enabled, void *user_data)
{
    action_test_state_t *state = (action_test_state_t *)user_data;
    state->mic_enabled = enabled;
    state->mic_gate_changes++;
}

static void test_set_hint(const char *text, void *user_data)
{
    action_test_state_t *state = (action_test_state_t *)user_data;
    state->last_hint = text;
    state->hint_changes++;
}

static void test_reset_outputs(void *user_data)
{
    action_test_state_t *state = (action_test_state_t *)user_data;
    state->reset_count++;
    state->mic_enabled = false;
}

static void test_refresh_ui(void *user_data)
{
    action_test_state_t *state = (action_test_state_t *)user_data;
    state->refresh_count++;
}

static void test_cursor_hold_actions_execute_in_order(void)
{
    action_test_state_t state = {0};
    mode_controller_t controller;
    action_engine_context_t context;
    const mode_config_t *config = mode_config_get();
    const mode_definition_t *cursor_mode = &config->modes[0];

    mode_controller_init(&controller, config);
    context = (action_engine_context_t){
        .controller = &controller,
        .send_hid_usage = test_send_hid_key,
        .set_mic_gate = test_set_mic_gate,
        .set_hint_text = test_set_hint,
        .reset_active_outputs = test_reset_outputs,
        .refresh_ui = test_refresh_ui,
        .user_data = &state,
    };

    expect(action_engine_execute_actions(cursor_mode->bindings[0].actions,
                                         cursor_mode->bindings[0].action_count,
                                         &context),
           "cursor hold start actions should succeed");
    expect(state.key_down_count == 1, "cursor hold should press one key");
    expect(state.last_keycode == MODE_KEY_F13, "cursor hold should use F13");
    expect(state.mic_enabled, "cursor hold should enable mic gate");
    expect(state.hint_changes == 1, "cursor hold should update hint");
}

static void test_cycle_mode_changes_controller_state(void)
{
    action_test_state_t state = {0};
    mode_controller_t controller;
    action_engine_context_t context;
    const mode_action_t actions[] = {
        { .type = MODE_ACTION_CYCLE_MODE, .data.direction = MODE_CYCLE_DIRECTION_NEXT },
        { .type = MODE_ACTION_UI_SHOW_MODE },
    };

    mode_controller_init(&controller, mode_config_get());
    context = (action_engine_context_t){
        .controller = &controller,
        .send_hid_usage = test_send_hid_key,
        .set_mic_gate = test_set_mic_gate,
        .set_hint_text = test_set_hint,
        .reset_active_outputs = test_reset_outputs,
        .refresh_ui = test_refresh_ui,
        .user_data = &state,
    };

    expect(action_engine_execute_actions(actions, 2, &context), "cycle mode actions should succeed");
    expect(mode_controller_get_active_mode(&controller) == MODE_ID_PRESENTATION, "cycle mode should advance active mode");
    expect(state.reset_count == 1, "cycle mode should reset active outputs");
    expect(state.last_hint != NULL, "ui show mode should set a hint");
}

static void test_failed_key_down_does_not_enable_mic_gate(void)
{
    action_test_state_t state = {
        .fail_hid_send = true,
    };
    mode_controller_t controller;
    action_engine_context_t context;
    const mode_config_t *config = mode_config_get();
    const mode_definition_t *cursor_mode = &config->modes[0];

    mode_controller_init(&controller, config);
    context = (action_engine_context_t){
        .controller = &controller,
        .send_hid_usage = test_send_hid_key,
        .set_mic_gate = test_set_mic_gate,
        .set_hint_text = test_set_hint,
        .reset_active_outputs = test_reset_outputs,
        .refresh_ui = test_refresh_ui,
        .user_data = &state,
    };

    expect(!action_engine_execute_actions(cursor_mode->bindings[0].actions,
                                          cursor_mode->bindings[0].action_count,
                                          &context),
           "cursor hold start should fail when HID key down fails");
    expect(state.mic_gate_changes == 0, "failed key down must not enable mic gate");
    expect(!state.mic_enabled, "mic gate must stay disabled on HID failure");
}

int main(void)
{
    test_cursor_hold_actions_execute_in_order();
    test_cycle_mode_changes_controller_state();
    test_failed_key_down_does_not_enable_mic_gate();

    puts("action_engine tests passed");
    return 0;
}
