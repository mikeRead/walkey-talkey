#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "mode_types.h"

/**
 * Initialise the air-mouse subsystem.
 * Starts the QMI8658 IMU driver and creates the gyro polling task (suspended).
 * Call once during startup, before air_mouse_set_active().
 */
esp_err_t air_mouse_init(const air_mouse_config_t *cfg);

/**
 * Enable or disable the air-mouse gyro task.
 * Called when entering/leaving mouse mode (MODE_ID_MOUSE).
 * When disabled the task is suspended and tracking is cleared.
 */
void air_mouse_set_active(bool active);

/** Returns true while the air-mouse task is enabled. */
bool air_mouse_is_active(void);

/**
 * Gate gyro-to-cursor conversion.
 * true  = finger is on screen, gyro samples become HID mouse reports.
 * false = finger lifted, gyro samples are discarded.
 */
void air_mouse_set_tracking(bool tracking);

/** Returns true while gyro tracking is active (finger on screen). */
bool air_mouse_is_tracking(void);

/**
 * Set the HID mouse button state included in every gyro report.
 * Used by mouse_mode.c to communicate drag state (e.g. left-button held).
 */
void air_mouse_set_buttons(uint8_t buttons);

/**
 * Get the cumulative absolute gyro movement since tracking started.
 * Used by mouse_mode.c to distinguish a tap (minimal movement) from a drag.
 */
float air_mouse_get_accumulated_movement(void);
