#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mode_types.h"

typedef enum {
    INPUT_ROUTER_TOUCH_EVENT_PRESSED = 0,
    INPUT_ROUTER_TOUCH_EVENT_LONG_PRESSED,
    INPUT_ROUTER_TOUCH_EVENT_RELEASED,
    INPUT_ROUTER_TOUCH_EVENT_GESTURE_UP,
    INPUT_ROUTER_TOUCH_EVENT_GESTURE_DOWN,
    INPUT_ROUTER_TOUCH_EVENT_GESTURE_LEFT,
    INPUT_ROUTER_TOUCH_EVENT_GESTURE_RIGHT,
} input_router_touch_event_t;

typedef struct {
    mode_touch_defaults_t defaults;
    bool touch_pressed;
    bool hold_active;
    bool gesture_seen;
    bool tap_pending;
    uint32_t touch_pressed_at_ms;
    uint32_t tap_deadline_ms;
    uint32_t last_tap_released_at_ms;
} input_router_t;

void input_router_init(input_router_t *router, const mode_touch_defaults_t *defaults);
void input_router_cancel_touch(input_router_t *router);
bool input_router_next_deadline_ms(const input_router_t *router, uint32_t *deadline_ms);
size_t input_router_flush_timeouts(input_router_t *router,
                                   uint32_t tick_ms,
                                   mode_binding_event_t *events,
                                   size_t max_events);
size_t input_router_handle_button(input_router_t *router,
                                  bool pressed,
                                  mode_binding_event_t *events,
                                  size_t max_events);
size_t input_router_handle_touch(input_router_t *router,
                                 input_router_touch_event_t event,
                                 uint32_t tick_ms,
                                 mode_binding_event_t *events,
                                 size_t max_events);
