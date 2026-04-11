#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mode_types.h"

/**
 * Initialise the mouse/trackpad subsystem.
 * Creates a one-shot click-release timer (paused). Call once after
 * ui_status_init().
 */
void mouse_mode_init(void);

/**
 * Apply touch mouse configuration at runtime.
 * Values take effect immediately for subsequent touch events.
 */
void mouse_mode_set_touch_config(const touch_mouse_config_t *cfg);

/**
 * Set the mouse backend type (touch trackpad vs air mouse).
 * Takes effect next time mouse_mode_set_active(true) is called, or
 * immediately if mouse mode is already active.
 */
void mouse_mode_set_type(mouse_mode_type_t type);

/** Returns the current mouse backend type. */
mouse_mode_type_t mouse_mode_get_type(void);

/**
 * Enable or disable mouse mode.
 * When disabled any held button is released and internal state is reset.
 */
void mouse_mode_set_active(bool active);

/** Returns true while mouse mode is active. */
bool mouse_mode_is_active(void);

/**
 * Immediately start cursor tracking for the overlay use case.
 * Air: begins gyro-to-cursor. Touch: marks as pressed/moved.
 * Release will NOT produce a click -- just stops tracking.
 */
void mouse_mode_start_tracking(void);

/**
 * Cleanly stop any active tracking and release held buttons.
 * Call before mouse_mode_set_active(false) in overlay teardown.
 */
void mouse_mode_force_release(void);

/** Called from ui_status on LV_EVENT_PRESSED with the touch-down point. */
void mouse_mode_handle_press(int32_t x, int32_t y);

/** Called from ui_status on LV_EVENT_PRESSING with the current point. */
void mouse_mode_handle_move(int32_t x, int32_t y);

/** Called from ui_status on LV_EVENT_LONG_PRESSED. */
void mouse_mode_handle_long_press(void);

/** Called from ui_status on LV_EVENT_RELEASED / LV_EVENT_PRESS_LOST. */
void mouse_mode_handle_release(void);
