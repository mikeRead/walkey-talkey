#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ptt_state.h"

static void expect(bool condition, const char *message)
{
    if (condition) {
        return;
    }

    fprintf(stderr, "test failure: %s\n", message);
    exit(1);
}

static void test_init_starts_idle(void)
{
    ptt_state_t state;
    ptt_state_init(&state);

    expect(!ptt_state_is_active(&state), "state should start idle");
    expect(!ptt_state_wants_active(&state), "state should not want active on init");
}

static void test_activation_requires_commit(void)
{
    ptt_state_t state;
    ptt_state_init(&state);

    expect(ptt_state_set_source(&state, PTT_SOURCE_BOOT_BUTTON, true) == PTT_TRANSITION_ACTIVATE,
           "boot press should request activation");
    expect(!ptt_state_is_active(&state), "state should stay idle until commit");
    expect(ptt_state_wants_active(&state), "state should want active after press");

    ptt_state_mark_active(&state);
    expect(ptt_state_is_active(&state), "state should become active after commit");
}

static void test_release_deactivates_after_commit(void)
{
    ptt_state_t state;
    ptt_state_init(&state);

    expect(ptt_state_set_source(&state, PTT_SOURCE_BOOT_BUTTON, true) == PTT_TRANSITION_ACTIVATE,
           "boot press should request activation before release");
    ptt_state_mark_active(&state);

    expect(ptt_state_set_source(&state, PTT_SOURCE_BOOT_BUTTON, false) == PTT_TRANSITION_DEACTIVATE,
           "boot release should request deactivation");
    ptt_state_mark_idle(&state);

    expect(!ptt_state_is_active(&state), "state should be idle after deactivation");
    expect(!ptt_state_wants_active(&state), "state should not want active after release");
}

static void test_multi_source_keeps_ptt_active(void)
{
    ptt_state_t state;
    ptt_state_init(&state);

    expect(ptt_state_set_source(&state, PTT_SOURCE_BOOT_BUTTON, true) == PTT_TRANSITION_ACTIVATE,
           "boot press should request activation");
    ptt_state_mark_active(&state);

    expect(ptt_state_set_source(&state, PTT_SOURCE_TOUCH_HOLD, true) == PTT_TRANSITION_NONE,
           "second active source should not change active state");
    expect(ptt_state_set_source(&state, PTT_SOURCE_BOOT_BUTTON, false) == PTT_TRANSITION_NONE,
           "one source release should keep PTT active");
    expect(ptt_state_is_active(&state), "PTT should stay active while another source is pressed");
    expect(ptt_state_wants_active(&state), "PTT should still want active while another source is pressed");

    expect(ptt_state_set_source(&state, PTT_SOURCE_TOUCH_HOLD, false) == PTT_TRANSITION_DEACTIVATE,
           "final source release should request deactivation");
}

static void test_failed_activation_rolls_back_cleanly(void)
{
    ptt_state_t state;
    ptt_state_init(&state);

    expect(ptt_state_set_source(&state, PTT_SOURCE_BOOT_BUTTON, true) == PTT_TRANSITION_ACTIVATE,
           "boot press should request activation");
    expect(!ptt_state_is_active(&state), "failed activation should leave state idle");

    expect(ptt_state_set_source(&state, PTT_SOURCE_BOOT_BUTTON, false) == PTT_TRANSITION_NONE,
           "releasing without commit should not request deactivation");
    expect(!ptt_state_is_active(&state), "state should remain idle after failed activation release");
    expect(!ptt_state_wants_active(&state), "state should no longer want active after release");
}

int main(void)
{
    test_init_starts_idle();
    test_activation_requires_commit();
    test_release_deactivates_after_commit();
    test_multi_source_keeps_ptt_active();
    test_failed_activation_rolls_back_cleanly();

    puts("ptt_state tests passed");
    return 0;
}
