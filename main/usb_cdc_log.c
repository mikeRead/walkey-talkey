#include "usb_cdc_log.h"

#include <stdarg.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "class/cdc/cdc_device.h"
#include "esp_log.h"
#include "tusb.h"

#define CDC_LOG_BUF_SIZE 512

static SemaphoreHandle_t s_mutex;
static char s_buf[CDC_LOG_BUF_SIZE];

static int usb_cdc_log_vprintf(const char *fmt, va_list args)
{
    if (!tud_cdc_connected()) {
        return 0;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(5)) != pdTRUE) {
        return 0;
    }

    int len = vsnprintf(s_buf, sizeof(s_buf), fmt, args);
    if (len > 0) {
        uint32_t to_write = (len < (int)sizeof(s_buf)) ? (uint32_t)len : (uint32_t)(sizeof(s_buf) - 1);
        tud_cdc_write(s_buf, to_write);
        tud_cdc_write_flush();
    }

    xSemaphoreGive(s_mutex);
    return len;
}

void usb_cdc_log_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return;
    }

    esp_log_set_vprintf(usb_cdc_log_vprintf);
}
