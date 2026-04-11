#pragma once

#include <stdbool.h>

#include "esp_err.h"

/**
 * Write default config JSON and documentation files to the SD card
 * if they do not already exist. Call while the SD card FAT filesystem
 * is mounted.
 */
esp_err_t sd_card_config_populate(void);

/**
 * Read mode-config.json from the SD card into a heap-allocated string.
 * Caller must free the returned buffer. Call while mounted.
 * Returns true on success, false if file is missing or unreadable.
 */
bool sd_card_config_read(char **out_json);
