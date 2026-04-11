#include "action_engine.h"

#include <stdint.h>

#define ACTION_ENGINE_TAP_GAP_MS 20

static void action_engine_refresh_ui(action_engine_context_t *context)
{
    if ((context != NULL) && (context->refresh_ui != NULL)) {
        context->refresh_ui(context->user_data);
    }
}

static void action_engine_set_hint(action_engine_context_t *context, const char *text)
{
    if ((context != NULL) && (context->set_hint_text != NULL)) {
        context->set_hint_text(text, context->user_data);
    }
}

static bool action_engine_get_mic_gate(action_engine_context_t *context)
{
    if ((context == NULL) || (context->get_mic_gate == NULL)) {
        return false;
    }

    return context->get_mic_gate(context->user_data);
}

static void action_engine_set_mic_gate(action_engine_context_t *context, bool enabled)
{
    if ((context != NULL) && (context->set_mic_gate != NULL)) {
        context->set_mic_gate(enabled, context->user_data);
    }
}

static bool action_engine_send_key(action_engine_context_t *context,
                                   bool pressed,
                                   const mode_hid_usage_t *usage)
{
    if ((context == NULL) || (context->send_hid_usage == NULL) || (usage == NULL)) {
        return false;
    }

    return context->send_hid_usage(pressed, usage, context->user_data);
}

static void action_engine_reset_outputs(action_engine_context_t *context)
{
    if ((context != NULL) && (context->reset_active_outputs != NULL)) {
        context->reset_active_outputs(context->user_data);
    }
}

static void action_engine_sleep(action_engine_context_t *context, uint32_t duration_ms)
{
    if ((context != NULL) && (context->sleep_ms != NULL) && (duration_ms > 0)) {
        context->sleep_ms(duration_ms, context->user_data);
    }
}

bool action_engine_execute_actions(const mode_action_t *actions,
                                   size_t action_count,
                                   action_engine_context_t *context)
{
    if ((actions == NULL) || (context == NULL) || (context->controller == NULL)) {
        return false;
    }

    for (size_t i = 0; i < action_count; ++i) {
        const mode_action_t *action = &actions[i];

        switch (action->type) {
        case MODE_ACTION_HID_KEY_DOWN:
            if (!action_engine_send_key(context, true, &action->data.hid_usage)) {
                return false;
            }
            break;

        case MODE_ACTION_HID_KEY_UP:
            if (!action_engine_send_key(context, false, &action->data.hid_usage)) {
                return false;
            }
            break;

        case MODE_ACTION_HID_KEY_TAP:
            if (!action_engine_send_key(context, true, &action->data.hid_usage)) {
                return false;
            }
            action_engine_sleep(context, ACTION_ENGINE_TAP_GAP_MS);
            if (!action_engine_send_key(context, false, &action->data.hid_usage)) {
                return false;
            }
            break;

        case MODE_ACTION_HID_SHORTCUT_TAP:
            if (!action_engine_send_key(context, true, &action->data.hid_usage)) {
                return false;
            }
            action_engine_sleep(context, ACTION_ENGINE_TAP_GAP_MS);
            if (!action_engine_send_key(context, false, &action->data.hid_usage)) {
                return false;
            }
            break;

        case MODE_ACTION_HID_USAGE_DOWN:
            if (!action_engine_send_key(context, true, &action->data.hid_usage)) {
                return false;
            }
            break;

        case MODE_ACTION_HID_USAGE_UP:
            if (!action_engine_send_key(context, false, &action->data.hid_usage)) {
                return false;
            }
            break;

        case MODE_ACTION_HID_USAGE_TAP:
            if (!action_engine_send_key(context, true, &action->data.hid_usage)) {
                return false;
            }
            action_engine_sleep(context, ACTION_ENGINE_TAP_GAP_MS);
            if (!action_engine_send_key(context, false, &action->data.hid_usage)) {
                return false;
            }
            break;

        case MODE_ACTION_HID_MODIFIER_DOWN:
            if (!action_engine_send_key(context,
                                        true,
                                        &(mode_hid_usage_t){
                                            .report_kind = MODE_HID_REPORT_KIND_KEYBOARD,
                                            .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD,
                                            .modifiers = action->data.modifier,
                                            .usage_id = MODE_KEY_NONE,
                                        })) {
                return false;
            }
            break;

        case MODE_ACTION_HID_MODIFIER_UP:
            if (!action_engine_send_key(context,
                                        false,
                                        &(mode_hid_usage_t){
                                            .report_kind = MODE_HID_REPORT_KIND_KEYBOARD,
                                            .usage_page = MODE_HID_USAGE_PAGE_KEYBOARD,
                                            .modifiers = action->data.modifier,
                                            .usage_id = MODE_KEY_NONE,
                                        })) {
                return false;
            }
            break;

        case MODE_ACTION_SLEEP_MS:
            action_engine_sleep(context, action->data.duration_ms);
            break;

        case MODE_ACTION_ENTER_BOOT_MODE:
            action_engine_reset_outputs(context);
            mode_controller_enter_boot_mode(context->controller);
            action_engine_refresh_ui(context);
            break;

        case MODE_ACTION_EXIT_BOOT_MODE:
            mode_controller_exit_boot_mode(context->controller);
            action_engine_refresh_ui(context);
            break;

        case MODE_ACTION_MIC_GATE:
            action_engine_set_mic_gate(context, action->data.enabled);
            action_engine_refresh_ui(context);
            break;

        case MODE_ACTION_MIC_GATE_TOGGLE: {
            bool enabled = !action_engine_get_mic_gate(context);
            action_engine_set_mic_gate(context, enabled);
            action_engine_set_hint(context, enabled ? "Mic active" : "Mic off");
            action_engine_refresh_ui(context);
            break;
        }

        case MODE_ACTION_UI_HINT:
            action_engine_set_hint(context, action->data.text);
            action_engine_refresh_ui(context);
            break;

        case MODE_ACTION_UI_SHOW_MODE:
            action_engine_set_hint(context, mode_controller_get_active_mode_label(context->controller));
            action_engine_refresh_ui(context);
            break;

        case MODE_ACTION_SET_MODE:
            action_engine_reset_outputs(context);
            mode_controller_set_mode(context->controller, action->data.mode);
            action_engine_refresh_ui(context);
            break;

        case MODE_ACTION_CYCLE_MODE:
            action_engine_reset_outputs(context);
            mode_controller_cycle_mode(context->controller, action->data.direction);
            action_engine_refresh_ui(context);
            break;

        case MODE_ACTION_NOOP:
            break;
        }
    }

    return true;
}
