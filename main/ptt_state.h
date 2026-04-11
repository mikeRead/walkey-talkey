#pragma once

#include <stdbool.h>

typedef enum {
    PTT_SOURCE_BOOT_BUTTON = 0,
    PTT_SOURCE_TOUCH_HOLD,
    PTT_SOURCE_COUNT,
} ptt_source_t;

typedef enum {
    PTT_TRANSITION_NONE = 0,
    PTT_TRANSITION_ACTIVATE,
    PTT_TRANSITION_DEACTIVATE,
} ptt_transition_t;

typedef struct {
    bool source_active[PTT_SOURCE_COUNT];
    bool ptt_active;
} ptt_state_t;

void ptt_state_init(ptt_state_t *state);
ptt_transition_t ptt_state_set_source(ptt_state_t *state, ptt_source_t source, bool pressed);
void ptt_state_mark_active(ptt_state_t *state);
void ptt_state_mark_idle(ptt_state_t *state);
bool ptt_state_is_active(const ptt_state_t *state);
bool ptt_state_wants_active(const ptt_state_t *state);
