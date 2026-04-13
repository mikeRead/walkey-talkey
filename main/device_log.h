#pragma once

#include <stddef.h>

#include "esp_err.h"

#define DEVICE_LOG_MAX_ENTRIES  32
#define DEVICE_LOG_MAX_TYPE     12
#define DEVICE_LOG_MAX_MESSAGE  80

esp_err_t device_log_init(void);

void device_log(const char *type, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/**
 * Serialize the circular buffer as JSON into buf.
 * Returns the number of bytes written (excluding NUL), or -1 on error.
 */
int device_log_get_json(char *buf, size_t buf_size);
