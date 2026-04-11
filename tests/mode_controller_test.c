#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "mode_config.h"
#include "mode_controller.h"

static void expect(bool condition, const char *message)
{
    if (condition) {
        return;
    }

    fprintf(stderr, "test failure: %s\n", message);
    exit(1);
}

static void test_defaults_to_cursor_mode(void)
{
    mode_controller_t controller;
    mode_controller_init(&controller, mode_config_get());

    expect(mode_controller_get_active_mode(&controller) == MODE_ID_CURSOR, "default mode should be cursor");
    expect(!mode_controller_is_boot_mode_active(&controller), "boot mode should start inactive");
}

static void test_collects_global_and_mode_bindings(void)
{
    mode_controller_t controller;
    const mode_binding_t *bindings[4] = {0};

    mode_controller_init(&controller, mode_config_get());

    size_t count = mode_controller_collect_bindings(&controller,
                                                    MODE_INPUT_BOOT_BUTTON,
                                                    MODE_TRIGGER_PRESS,
                                                    bindings,
                                                    4);
    expect(count == 1, "boot press should resolve one global binding");
    expect(bindings[0]->actions[0].type == MODE_ACTION_ENTER_BOOT_MODE, "boot press should enter boot mode");

    count = mode_controller_collect_bindings(&controller,
                                             MODE_INPUT_TOUCH,
                                             MODE_TRIGGER_HOLD_START,
                                             bindings,
                                             4);
    expect(count == 1, "cursor hold should resolve one binding");
    expect(bindings[0]->actions[0].type == MODE_ACTION_HID_KEY_DOWN, "cursor hold should start with key down");
}

static void test_boot_mode_overrides_active_mode_bindings(void)
{
    mode_controller_t controller;
    const mode_binding_t *bindings[4] = {0};

    mode_controller_init(&controller, mode_config_get());
    mode_controller_enter_boot_mode(&controller);

    size_t count = mode_controller_collect_bindings(&controller,
                                                    MODE_INPUT_TOUCH,
                                                    MODE_TRIGGER_SWIPE_RIGHT,
                                                    bindings,
                                                    4);
    expect(count == 1, "boot swipe right should resolve one boot binding");
    expect(bindings[0]->actions[0].type == MODE_ACTION_CYCLE_MODE, "boot swipe should cycle mode");

    count = mode_controller_collect_bindings(&controller,
                                             MODE_INPUT_TOUCH,
                                             MODE_TRIGGER_HOLD_START,
                                             bindings,
                                             4);
    expect(count == 0, "boot mode should suppress cursor hold binding");
}

static void test_cycle_mode_wraps(void)
{
    mode_controller_t controller;
    mode_controller_init(&controller, mode_config_get());

    expect(mode_controller_cycle_mode(&controller, MODE_CYCLE_DIRECTION_PREVIOUS), "cycle previous should change mode");
    expect(mode_controller_get_active_mode(&controller) == MODE_ID_NAVIGATION, "cycling previous from cursor should wrap");
    expect(mode_controller_cycle_mode(&controller, MODE_CYCLE_DIRECTION_NEXT), "cycle next should change mode");
    expect(mode_controller_get_active_mode(&controller) == MODE_ID_CURSOR, "cycling next should return to cursor");
}

int main(void)
{
    test_defaults_to_cursor_mode();
    test_collects_global_and_mode_bindings();
    test_boot_mode_overrides_active_mode_bindings();
    test_cycle_mode_wraps();

    puts("mode_controller tests passed");
    return 0;
}
