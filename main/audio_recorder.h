#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

esp_err_t audio_recorder_init(void);
esp_err_t audio_recorder_start(const char *mode_name);
esp_err_t audio_recorder_feed(const void *pcm, size_t len);
esp_err_t audio_recorder_stop(void);
bool audio_recorder_is_recording(void);
