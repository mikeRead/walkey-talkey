#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "input_router.h"

static void expect(bool condition, const char *message)
{
    if (condition) {
        return;
    }

    fprintf(stderr, "test failure: %s\n", message);
    exit(1);
}

static input_router_t make_router(void)
{
    input_router_t router;
    input_router_init(&router, &(mode_touch_defaults_t){
        .hold_ms = 400,
        .double_tap_ms = 350,
        .swipe_min_distance = 40,
    });
    return router;
}

static void test_button_maps_to_press_and_release(void)
{
    input_router_t router = make_router();
    mode_binding_event_t events[2] = {0};

    size_t count = input_router_handle_button(&router, true, events, 2);
    expect(count == 1, "button press should emit one event");
    expect(events[0].input == MODE_INPUT_BOOT_BUTTON, "button press should use boot input");
    expect(events[0].trigger == MODE_TRIGGER_PRESS, "button press should emit press trigger");

    count = input_router_handle_button(&router, false, events, 2);
    expect(count == 1, "button release should emit one event");
    expect(events[0].trigger == MODE_TRIGGER_RELEASE, "button release should emit release trigger");
}

static void test_touch_tap_emits_tap(void)
{
    input_router_t router = make_router();
    mode_binding_event_t events[2] = {0};

    expect(input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_PRESSED, 100, events, 2) == 0,
           "touch press should not emit immediately");
    size_t count = input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_RELEASED, 200, events, 2);
    expect(count == 0, "single tap should wait for double-tap timeout");
    count = input_router_flush_timeouts(&router, 600, events, 2);
    expect(count == 1, "touch tap should emit after timeout");
    expect(events[0].trigger == MODE_TRIGGER_TAP, "touch tap should map to tap");
}

static void test_touch_hold_emits_start_and_end(void)
{
    input_router_t router = make_router();
    mode_binding_event_t events[3] = {0};

    expect(input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_PRESSED, 100, events, 2) == 0,
           "touch press should not emit immediately");
    size_t count = input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_LONG_PRESSED, 550, events, 2);
    expect(count == 2, "long press should emit long_press and hold_start");
    expect(events[0].trigger == MODE_TRIGGER_LONG_PRESS, "long press should emit long_press first");
    expect(events[1].trigger == MODE_TRIGGER_HOLD_START, "long press should start hold");

    count = input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_RELEASED, 650, events, 2);
    expect(count == 1, "hold release should emit one event");
    expect(events[0].trigger == MODE_TRIGGER_HOLD_END, "release after hold should emit hold end");
}

static void test_touch_double_tap_emits_double_tap(void)
{
    input_router_t router = make_router();
    mode_binding_event_t events[2] = {0};

    expect(input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_PRESSED, 100, events, 2) == 0,
           "first press should not emit");
    expect(input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_RELEASED, 160, events, 2) == 0,
           "first tap should become pending");
    expect(input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_PRESSED, 260, events, 2) == 0,
           "second press should not emit");

    size_t count = input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_RELEASED, 320, events, 2);
    expect(count == 1, "second tap inside timeout should emit double_tap");
    expect(events[0].trigger == MODE_TRIGGER_DOUBLE_TAP, "double tap should map to double_tap");
}

static void test_swipe_suppresses_tap_on_release(void)
{
    input_router_t router = make_router();
    mode_binding_event_t events[2] = {0};

    expect(input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_PRESSED, 100, events, 2) == 0,
           "touch press should not emit immediately");
    size_t count = input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_GESTURE_LEFT, 150, events, 2);
    expect(count == 1, "swipe should emit one event");
    expect(events[0].trigger == MODE_TRIGGER_SWIPE_LEFT, "gesture left should emit swipe left");

    count = input_router_handle_touch(&router, INPUT_ROUTER_TOUCH_EVENT_RELEASED, 180, events, 2);
    expect(count == 0, "release after swipe should not emit tap");
}

int main(void)
{
    test_button_maps_to_press_and_release();
    test_touch_tap_emits_tap();
    test_touch_hold_emits_start_and_end();
    test_touch_double_tap_emits_double_tap();
    test_swipe_suppresses_tap_on_release();

    puts("input_router tests passed");
    return 0;
}
