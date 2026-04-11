#include "input_router.h"

static size_t input_router_emit(mode_binding_event_t *events,
                                size_t max_events,
                                size_t count,
                                mode_input_t input,
                                mode_trigger_t trigger)
{
    if ((events != NULL) && (count < max_events)) {
        events[count].input = input;
        events[count].trigger = trigger;
    }

    return count + 1;
}

static bool input_router_deadline_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void input_router_clear_pending_tap(input_router_t *router)
{
    if (router == NULL) {
        return;
    }

    router->tap_pending = false;
    router->tap_deadline_ms = 0;
}

static bool input_router_touch_hold_pending(const input_router_t *router)
{
    return (router != NULL) &&
           router->touch_pressed &&
           !router->hold_active &&
           !router->gesture_seen;
}

static uint32_t input_router_touch_hold_deadline_ms(const input_router_t *router)
{
    return router->touch_pressed_at_ms + router->defaults.hold_ms;
}

static size_t input_router_emit_pending_tap(input_router_t *router,
                                            mode_binding_event_t *events,
                                            size_t max_events,
                                            size_t count)
{
    if ((router == NULL) || !router->tap_pending) {
        return count;
    }

    count = input_router_emit(events, max_events, count, MODE_INPUT_TOUCH, MODE_TRIGGER_TAP);
    input_router_clear_pending_tap(router);
    return count;
}

void input_router_init(input_router_t *router, const mode_touch_defaults_t *defaults)
{
    if (router == NULL) {
        return;
    }

    router->defaults = (defaults != NULL) ? *defaults : (mode_touch_defaults_t){
        .hold_ms = 400,
        .double_tap_ms = 350,
        .swipe_min_distance = 40,
    };
    input_router_clear_pending_tap(router);
    router->last_tap_released_at_ms = 0;
    input_router_cancel_touch(router);
}

void input_router_cancel_touch(input_router_t *router)
{
    if (router == NULL) {
        return;
    }

    router->touch_pressed = false;
    router->hold_active = false;
    router->gesture_seen = false;
    router->touch_pressed_at_ms = 0;
}

bool input_router_next_deadline_ms(const input_router_t *router, uint32_t *deadline_ms)
{
    if ((router == NULL) || (deadline_ms == NULL)) {
        return false;
    }

    bool has_deadline = false;

    if (router->tap_pending) {
        *deadline_ms = router->tap_deadline_ms;
        has_deadline = true;
    }

    if (input_router_touch_hold_pending(router)) {
        uint32_t hold_deadline_ms = input_router_touch_hold_deadline_ms(router);
        if (!has_deadline || input_router_deadline_reached(*deadline_ms, hold_deadline_ms)) {
            *deadline_ms = hold_deadline_ms;
        }
        has_deadline = true;
    }

    return has_deadline;
}

size_t input_router_flush_timeouts(input_router_t *router,
                                   uint32_t tick_ms,
                                   mode_binding_event_t *events,
                                   size_t max_events)
{
    if (router == NULL) {
        return 0;
    }

    size_t count = 0;

    if (input_router_touch_hold_pending(router) &&
        input_router_deadline_reached(tick_ms, input_router_touch_hold_deadline_ms(router))) {
        router->hold_active = true;
        count = input_router_emit(events, max_events, count, MODE_INPUT_TOUCH, MODE_TRIGGER_LONG_PRESS);
        count = input_router_emit(events, max_events, count, MODE_INPUT_TOUCH, MODE_TRIGGER_HOLD_START);
    }

    if (router->tap_pending &&
        input_router_deadline_reached(tick_ms, router->tap_deadline_ms)) {
        count = input_router_emit_pending_tap(router, events, max_events, count);
    }

    return count;
}

size_t input_router_handle_button(input_router_t *router,
                                  bool pressed,
                                  mode_binding_event_t *events,
                                  size_t max_events)
{
    (void)router;

    return input_router_emit(events,
                             max_events,
                             0,
                             MODE_INPUT_BOOT_BUTTON,
                             pressed ? MODE_TRIGGER_PRESS : MODE_TRIGGER_RELEASE);
}

size_t input_router_handle_touch(input_router_t *router,
                                 input_router_touch_event_t event,
                                 uint32_t tick_ms,
                                 mode_binding_event_t *events,
                                 size_t max_events)
{
    if (router == NULL) {
        return 0;
    }

    size_t count = 0;

    switch (event) {
    case INPUT_ROUTER_TOUCH_EVENT_PRESSED:
        router->touch_pressed = true;
        router->hold_active = false;
        router->gesture_seen = false;
        router->touch_pressed_at_ms = tick_ms;
        break;

    case INPUT_ROUTER_TOUCH_EVENT_LONG_PRESSED:
        /* Hold recognition is timeout-driven so one layer owns it. */
        break;

    case INPUT_ROUTER_TOUCH_EVENT_GESTURE_UP:
        if (router->touch_pressed && !router->gesture_seen && !router->hold_active) {
            count = input_router_emit_pending_tap(router, events, max_events, count);
            router->gesture_seen = true;
            count = input_router_emit(events, max_events, count, MODE_INPUT_TOUCH, MODE_TRIGGER_SWIPE_UP);
        }
        break;

    case INPUT_ROUTER_TOUCH_EVENT_GESTURE_DOWN:
        if (router->touch_pressed && !router->gesture_seen && !router->hold_active) {
            count = input_router_emit_pending_tap(router, events, max_events, count);
            router->gesture_seen = true;
            count = input_router_emit(events, max_events, count, MODE_INPUT_TOUCH, MODE_TRIGGER_SWIPE_DOWN);
        }
        break;

    case INPUT_ROUTER_TOUCH_EVENT_GESTURE_LEFT:
        if (router->touch_pressed && !router->gesture_seen && !router->hold_active) {
            count = input_router_emit_pending_tap(router, events, max_events, count);
            router->gesture_seen = true;
            count = input_router_emit(events, max_events, count, MODE_INPUT_TOUCH, MODE_TRIGGER_SWIPE_LEFT);
        }
        break;

    case INPUT_ROUTER_TOUCH_EVENT_GESTURE_RIGHT:
        if (router->touch_pressed && !router->gesture_seen && !router->hold_active) {
            count = input_router_emit_pending_tap(router, events, max_events, count);
            router->gesture_seen = true;
            count = input_router_emit(events, max_events, count, MODE_INPUT_TOUCH, MODE_TRIGGER_SWIPE_RIGHT);
        }
        break;

    case INPUT_ROUTER_TOUCH_EVENT_RELEASED:
        if (!router->touch_pressed) {
            input_router_cancel_touch(router);
            break;
        }

        if (router->hold_active) {
            count = input_router_emit(events,
                                      max_events,
                                      count,
                                      MODE_INPUT_TOUCH,
                                      MODE_TRIGGER_HOLD_END);
        } else if (!router->gesture_seen && ((tick_ms - router->touch_pressed_at_ms) < router->defaults.hold_ms)) {
            if (router->tap_pending &&
                ((tick_ms - router->last_tap_released_at_ms) <= router->defaults.double_tap_ms)) {
                input_router_clear_pending_tap(router);
                count = input_router_emit(events,
                                          max_events,
                                          count,
                                          MODE_INPUT_TOUCH,
                                          MODE_TRIGGER_DOUBLE_TAP);
            } else {
                router->tap_pending = true;
                router->last_tap_released_at_ms = tick_ms;
                router->tap_deadline_ms = tick_ms + router->defaults.double_tap_ms;
            }
        }

        input_router_cancel_touch(router);
        break;
    }

    return count;
}
