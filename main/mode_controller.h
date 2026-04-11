#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "mode_types.h"

typedef struct {
    const mode_config_t *config;
    mode_id_t active_mode;
    bool boot_mode_active;
} mode_controller_t;

void mode_controller_init(mode_controller_t *controller, const mode_config_t *config);
bool mode_controller_enter_boot_mode(mode_controller_t *controller);
bool mode_controller_exit_boot_mode(mode_controller_t *controller);
bool mode_controller_set_mode(mode_controller_t *controller, mode_id_t mode);
bool mode_controller_cycle_mode(mode_controller_t *controller, mode_cycle_direction_t direction);
bool mode_controller_is_boot_mode_active(const mode_controller_t *controller);
mode_id_t mode_controller_get_active_mode(const mode_controller_t *controller);
const char *mode_controller_get_active_mode_label(const mode_controller_t *controller);
const char *mode_controller_get_previous_mode_label(const mode_controller_t *controller);
const char *mode_controller_get_next_mode_label(const mode_controller_t *controller);
const mode_boot_mode_t *mode_controller_get_boot_mode(const mode_controller_t *controller);
size_t mode_controller_collect_bindings(const mode_controller_t *controller,
                                        mode_input_t input,
                                        mode_trigger_t trigger,
                                        const mode_binding_t **bindings,
                                        size_t max_bindings);
