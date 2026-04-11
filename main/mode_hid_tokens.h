#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "mode_types.h"

typedef struct {
    const char *token;
    const char *canonical_token;
    mode_hid_usage_t usage;
} mode_hid_token_t;

typedef struct {
    const char *token;
    const char *canonical_token;
    mode_hid_modifier_t modifier_mask;
} mode_hid_modifier_token_t;

bool mode_hid_usage_is_keyboard(const mode_hid_usage_t *usage);
bool mode_hid_usage_is_consumer(const mode_hid_usage_t *usage);
bool mode_hid_usage_is_system(const mode_hid_usage_t *usage);

size_t mode_hid_modifier_token_count(void);
const mode_hid_modifier_token_t *mode_hid_modifier_token_at(size_t index);
size_t mode_hid_usage_token_count(void);
const mode_hid_token_t *mode_hid_usage_token_at(size_t index);
const char *mode_hid_canonical_modifier_token(mode_hid_modifier_t modifier_mask);
bool mode_hid_usage_to_canonical_token(const mode_hid_usage_t *usage, char *buffer, size_t buffer_size);

bool mode_hid_parse_modifier_token(const char *text, mode_hid_modifier_t *modifier_mask);
bool mode_hid_parse_usage_token(const char *text, mode_hid_usage_t *usage);
bool mode_hid_parse_shortcut_string(const char *text, mode_hid_usage_t *usage);
bool mode_hid_parse_report_kind(const char *text, mode_hid_report_kind_t *report_kind);
