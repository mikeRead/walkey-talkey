#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * Initialise the mouse/trackpad subsystem.
 * Creates a one-shot click-release timer (paused). Call once after
 * ui_status_init().
 */
void mouse_mode_init(void);

/**
 * Enable or disable mouse mode.
 * When disabled any held button is released and internal state is reset.
 */
void mouse_mode_set_active(bool active);

/** Returns true while mouse mode is active. */
bool mouse_mode_is_active(void);

/** Called from ui_status on LV_EVENT_PRESSED with the touch-down point. */
void mouse_mode_handle_press(int32_t x, int32_t y);

/** Called from ui_status on LV_EVENT_PRESSING with the current point. */
void mouse_mode_handle_move(int32_t x, int32_t y);

/** Called from ui_status on LV_EVENT_LONG_PRESSED. */
void mouse_mode_handle_long_press(void);

/** Called from ui_status on LV_EVENT_RELEASED / LV_EVENT_PRESS_LOST. */
void mouse_mode_handle_release(void);
