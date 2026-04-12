#include "mode_controller.h"

#include "device_log.h"

/* Virtual mouse mode: hardcoded, not in JSON config. */
static const mode_definition_t s_mouse_mode_def = {
    .id = MODE_ID_MOUSE,
    .name = "mouse",
    .label = "Mouse",
    .cycle_order = 0xFFFE,
    .bindings = NULL,
    .binding_count = 0,
};

static const mode_definition_t *mode_controller_find_mode(const mode_controller_t *controller, mode_id_t mode)
{
    if (mode == MODE_ID_MOUSE) {
        return &s_mouse_mode_def;
    }

    if ((controller == NULL) || (controller->config == NULL)) {
        return NULL;
    }

    for (size_t i = 0; i < controller->config->mode_count; ++i) {
        if (controller->config->modes[i].id == mode) {
            return &controller->config->modes[i];
        }
    }

    return NULL;
}

static size_t mode_controller_collect_from_list(mode_input_t input,
                                                mode_trigger_t trigger,
                                                const mode_binding_t *source,
                                                size_t source_count,
                                                const mode_binding_t **bindings,
                                                size_t max_bindings,
                                                size_t count)
{
    if ((bindings == NULL) || (source == NULL)) {
        return count;
    }

    for (size_t i = 0; i < source_count; ++i) {
        if ((source[i].input != input) || (source[i].trigger != trigger)) {
            continue;
        }

        if (count < max_bindings) {
            bindings[count] = &source[i];
        }
        count++;
    }

    return count;
}

static mode_id_t mode_controller_neighbor_mode(const mode_controller_t *controller, int offset)
{
    if ((controller == NULL) || (controller->config == NULL) || (controller->config->mode_count == 0)) {
        return MODE_ID_CURSOR;
    }

    /* Virtual list: config modes followed by mouse mode as one extra slot. */
    int config_count = (int)controller->config->mode_count;
    int effective_count = config_count + 1; /* +1 for mouse mode */

    /* Find current index: first search config modes, then check mouse mode. */
    int current_index = config_count; /* default to mouse mode slot */
    if (controller->active_mode != MODE_ID_MOUSE) {
        for (int i = 0; i < config_count; ++i) {
            if (controller->config->modes[i].id == controller->active_mode) {
                current_index = i;
                break;
            }
        }
    }

    int next_index = current_index + offset;
    while (next_index < 0) {
        next_index += effective_count;
    }
    next_index %= effective_count;

    if (next_index == config_count) {
        return MODE_ID_MOUSE;
    }
    return controller->config->modes[next_index].id;
}

void mode_controller_init(mode_controller_t *controller, const mode_config_t *config)
{
    if (controller == NULL) {
        return;
    }

    controller->config = config;
    controller->boot_mode_active = false;
    controller->active_mode = (config != NULL) ? config->active_mode : MODE_ID_CURSOR;
}

bool mode_controller_enter_boot_mode(mode_controller_t *controller)
{
    if (controller == NULL) {
        return false;
    }

    bool changed = !controller->boot_mode_active;
    controller->boot_mode_active = true;
    return changed;
}

bool mode_controller_exit_boot_mode(mode_controller_t *controller)
{
    if (controller == NULL) {
        return false;
    }

    bool changed = controller->boot_mode_active;
    controller->boot_mode_active = false;
    return changed;
}

bool mode_controller_set_mode(mode_controller_t *controller, mode_id_t mode)
{
    if ((controller == NULL) || (mode_controller_find_mode(controller, mode) == NULL)) {
        return false;
    }

    bool changed = controller->active_mode != mode;
    controller->active_mode = mode;

    if (changed) {
        const mode_definition_t *def = mode_controller_find_mode(controller, mode);
        device_log("ACTION", "Mode switched to %s", (def && def->label) ? def->label : "Unknown");
    }

    return changed;
}

bool mode_controller_cycle_mode(mode_controller_t *controller, mode_cycle_direction_t direction)
{
    if (controller == NULL) {
        return false;
    }

    mode_id_t next_mode = mode_controller_neighbor_mode(
        controller,
        (direction == MODE_CYCLE_DIRECTION_PREVIOUS) ? -1 : 1
    );
    return mode_controller_set_mode(controller, next_mode);
}

bool mode_controller_is_boot_mode_active(const mode_controller_t *controller)
{
    return (controller != NULL) && controller->boot_mode_active;
}

mode_id_t mode_controller_get_active_mode(const mode_controller_t *controller)
{
    return (controller != NULL) ? controller->active_mode : MODE_ID_CURSOR;
}

const char *mode_controller_get_active_mode_label(const mode_controller_t *controller)
{
    const mode_definition_t *mode = mode_controller_find_mode(controller, mode_controller_get_active_mode(controller));
    return (mode != NULL) ? mode->label : "Unknown";
}

const char *mode_controller_get_active_mode_name(const mode_controller_t *controller)
{
    const mode_definition_t *mode = mode_controller_find_mode(controller, mode_controller_get_active_mode(controller));
    return (mode != NULL && mode->name != NULL) ? mode->name : "unknown";
}

const char *mode_controller_get_previous_mode_label(const mode_controller_t *controller)
{
    const mode_definition_t *mode = mode_controller_find_mode(controller, mode_controller_neighbor_mode(controller, -1));
    return (mode != NULL) ? mode->label : "Unknown";
}

const char *mode_controller_get_next_mode_label(const mode_controller_t *controller)
{
    const mode_definition_t *mode = mode_controller_find_mode(controller, mode_controller_neighbor_mode(controller, 1));
    return (mode != NULL) ? mode->label : "Unknown";
}

const mode_boot_mode_t *mode_controller_get_boot_mode(const mode_controller_t *controller)
{
    if ((controller == NULL) || (controller->config == NULL)) {
        return NULL;
    }

    return &controller->config->boot_mode;
}

size_t mode_controller_collect_bindings(const mode_controller_t *controller,
                                        mode_input_t input,
                                        mode_trigger_t trigger,
                                        const mode_binding_t **bindings,
                                        size_t max_bindings)
{
    if ((controller == NULL) || (controller->config == NULL)) {
        return 0;
    }

    size_t count = 0;
    count = mode_controller_collect_from_list(input,
                                              trigger,
                                              controller->config->global_bindings,
                                              controller->config->global_binding_count,
                                              bindings,
                                              max_bindings,
                                              count);

    if (controller->boot_mode_active) {
        return mode_controller_collect_from_list(input,
                                                 trigger,
                                                 controller->config->boot_mode.bindings,
                                                 controller->config->boot_mode.binding_count,
                                                 bindings,
                                                 max_bindings,
                                                 count);
    }

    const mode_definition_t *active_mode = mode_controller_find_mode(controller, controller->active_mode);
    if (active_mode == NULL) {
        return count;
    }

    return mode_controller_collect_from_list(input,
                                             trigger,
                                             active_mode->bindings,
                                             active_mode->binding_count,
                                             bindings,
                                             max_bindings,
                                             count);
}
