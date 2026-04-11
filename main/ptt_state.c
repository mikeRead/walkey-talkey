#include "ptt_state.h"

#include <stddef.h>

static bool ptt_state_any_source_active(const ptt_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    for (int i = 0; i < PTT_SOURCE_COUNT; ++i) {
        if (state->source_active[i]) {
            return true;
        }
    }

    return false;
}

void ptt_state_init(ptt_state_t *state)
{
    if (state == NULL) {
        return;
    }

    for (int i = 0; i < PTT_SOURCE_COUNT; ++i) {
        state->source_active[i] = false;
    }

    state->ptt_active = false;
}

ptt_transition_t ptt_state_set_source(ptt_state_t *state, ptt_source_t source, bool pressed)
{
    if ((state == NULL) || (source < 0) || (source >= PTT_SOURCE_COUNT)) {
        return PTT_TRANSITION_NONE;
    }

    state->source_active[source] = pressed;

    bool wants_active = ptt_state_any_source_active(state);
    if (!state->ptt_active && wants_active) {
        return PTT_TRANSITION_ACTIVATE;
    }

    if (state->ptt_active && !wants_active) {
        return PTT_TRANSITION_DEACTIVATE;
    }

    return PTT_TRANSITION_NONE;
}

void ptt_state_mark_active(ptt_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->ptt_active = true;
}

void ptt_state_mark_idle(ptt_state_t *state)
{
    if (state == NULL) {
        return;
    }

    state->ptt_active = false;
}

bool ptt_state_is_active(const ptt_state_t *state)
{
    return (state != NULL) && state->ptt_active;
}

bool ptt_state_wants_active(const ptt_state_t *state)
{
    return ptt_state_any_source_active(state);
}
