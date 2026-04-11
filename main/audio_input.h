#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define AUDIO_INPUT_SAMPLE_RATE_HZ 48000
#define AUDIO_INPUT_BITS_PER_SAMPLE 16
#define AUDIO_INPUT_CHANNEL_COUNT 1
#define AUDIO_INPUT_FRAME_MS 1
#define AUDIO_INPUT_FRAME_BYTES ((AUDIO_INPUT_SAMPLE_RATE_HZ / 1000) * (AUDIO_INPUT_BITS_PER_SAMPLE / 8) * AUDIO_INPUT_CHANNEL_COUNT * AUDIO_INPUT_FRAME_MS)

esp_err_t audio_input_init(void);
bool audio_input_ready(void);
esp_err_t audio_input_read_raw(void *buffer, size_t len);
esp_err_t audio_input_read_frame(void *buffer, size_t len, bool ptt_active);
