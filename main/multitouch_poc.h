#pragma once

#include <stdint.h>

#include "esp_err.h"

/**
 * Initialise the multi-touch proof-of-concept reader.
 *
 * Opens a second I2C device handle to the CST9217 (address 0x5A) on the
 * existing BSP I2C bus and spawns a FreeRTOS task that polls register 0xD000
 * every ~30 ms.  The raw touch-count field (data[5] & 0x7F) is stored —
 * with a 150 ms hold-high debounce — for lock-free retrieval via
 * multitouch_poc_get_touch_count().
 */
esp_err_t multitouch_poc_init(void);

/**
 * Return the last-observed raw touch-point count from the CST9217.
 *
 * The value is updated by the background polling task.  A return value >= 2
 * means the hardware reported multiple simultaneous contacts in its most
 * recent register snapshot.
 */
uint8_t multitouch_poc_get_touch_count(void);
