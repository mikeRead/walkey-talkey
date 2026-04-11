#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_STATUS_TOUCH_RAW_PRESSED = 0,
    UI_STATUS_TOUCH_RAW_LONG_PRESSED,
    UI_STATUS_TOUCH_RAW_RELEASED,
    UI_STATUS_TOUCH_RAW_GESTURE_UP,
    UI_STATUS_TOUCH_RAW_GESTURE_DOWN,
    UI_STATUS_TOUCH_RAW_GESTURE_LEFT,
    UI_STATUS_TOUCH_RAW_GESTURE_RIGHT,
} ui_status_touch_raw_event_t;

typedef void (*ui_status_touch_event_cb_t)(ui_status_touch_raw_event_t event, void *user_data);

typedef struct {
    const char *mode_label;
    const char *top_status_text;
    const char *status_text;
    bool primary_active;
    bool boot_overlay_visible;
    const char *boot_title;
    const char *boot_subtitle;
    const char *touch_debug_text;
} ui_status_view_model_t;

void ui_status_init(ui_status_touch_event_cb_t touch_event_cb, void *user_data, uint32_t hold_delay_ms);
void ui_status_render(const ui_status_view_model_t *view_model);
void ui_status_set_swipe_min_distance(uint16_t swipe_min_distance);
/**
 * When mouse mode is enabled, swipe detection (GESTURE / PRESSING handlers)
 * and lv_indev_wait_release() are suppressed so that the mouse_mode LVGL
 * timer can poll the indev without interference.
 */
void ui_status_set_mouse_mode(bool enabled);

/**
 * Toggle the multi-touch visual indicator.
 *
 * When active (and mouse mode is also active), the background and card
 * switch to a distinct purple colour scheme so the user can confirm that
 * the hardware is reporting multiple simultaneous contacts.
 *
 * Safe to call from inside the LVGL event callback (display lock held).
 */
void ui_status_set_multitouch_indicator(bool active);
