#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "mode_controller.h"
#include "mode_types.h"

typedef struct {
    mode_controller_t *controller;
    bool (*send_hid_usage)(bool pressed, const mode_hid_usage_t *usage, void *user_data);
    void (*sleep_ms)(uint32_t duration_ms, void *user_data);
    bool (*get_mic_gate)(void *user_data);
    void (*set_mic_gate)(bool enabled, int8_t recording_override, void *user_data);
    void (*set_hint_text)(const char *text, void *user_data);
    void (*reset_active_outputs)(void *user_data);
    void (*refresh_ui)(void *user_data);
    bool (*get_mouse_overlay)(void *user_data);
    void (*set_mouse_overlay)(bool enabled, mouse_mode_type_t type,
                              bool immediate_tracking, void *user_data);
    void *user_data;
} action_engine_context_t;

bool action_engine_execute_actions(const mode_action_t *actions,
                                   size_t action_count,
                                   action_engine_context_t *context);
