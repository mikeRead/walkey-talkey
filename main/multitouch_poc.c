#include "multitouch_poc.h"

#include <string.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MT_POC_I2C_ADDR         0x5A
#define MT_POC_DATA_REG_HI      0xD0
#define MT_POC_DATA_REG_LO      0x00
#define MT_POC_ACK_VALUE        0xAB
#define MT_POC_READ_LEN         10
#define MT_POC_POLL_INTERVAL_MS 30
#define MT_POC_REG_SETTLE_MS    3
#define MT_POC_HOLD_HIGH_MS     150
#define MT_POC_TASK_STACK       2048
#define MT_POC_TASK_PRIORITY    3
#define MT_POC_I2C_TIMEOUT_MS   50

static const char *TAG = "multitouch_poc";

static i2c_master_dev_handle_t s_dev = NULL;
static volatile uint8_t s_touch_count = 0;
static bool s_multitouch_logged = false;
static uint32_t s_multitouch_last_seen_ms = 0;

static void multitouch_poc_task(void *arg)
{
    (void)arg;

    /*
     * The CST9217 expects a write of the 2-byte register address followed by
     * a short settling delay before the host clocks out data.  The Waveshare
     * driver does exactly this (TX, vTaskDelay(2ms), RX).  Using a single
     * i2c_master_transmit_receive (repeated-start, no gap) failed most reads.
     */
    const uint8_t reg_addr[2] = {MT_POC_DATA_REG_HI, MT_POC_DATA_REG_LO};
    uint8_t data[MT_POC_READ_LEN];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(MT_POC_POLL_INTERVAL_MS));

        esp_err_t err = i2c_master_transmit(s_dev, reg_addr, sizeof(reg_addr),
                                            MT_POC_I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(MT_POC_REG_SETTLE_MS));

        memset(data, 0, sizeof(data));
        err = i2c_master_receive(s_dev, data, sizeof(data),
                                 MT_POC_I2C_TIMEOUT_MS);
        if (err != ESP_OK) {
            continue;
        }

        if (data[6] != MT_POC_ACK_VALUE) {
            continue;
        }

        uint8_t count = data[5] & 0x7F;
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

        if (count >= 2) {
            s_multitouch_last_seen_ms = now_ms;
            s_touch_count = count;
            if (!s_multitouch_logged) {
                s_multitouch_logged = true;
                ESP_LOGI(TAG, "multi-touch detected: touch_count=%u", (unsigned)count);
            }
        } else if ((now_ms - s_multitouch_last_seen_ms) < MT_POC_HOLD_HIGH_MS) {
            /* Hold the >=2 value during the decay window to suppress brief dips. */
        } else {
            s_touch_count = count;
            s_multitouch_logged = false;
        }
    }
}

esp_err_t multitouch_poc_init(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not available");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MT_POC_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to add I2C device: %s", esp_err_to_name(err));
        return err;
    }

    BaseType_t created = xTaskCreate(
        multitouch_poc_task, "mt_poc", MT_POC_TASK_STACK, NULL,
        MT_POC_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "failed to create polling task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "initialised (polling every %d ms)", MT_POC_POLL_INTERVAL_MS);
    return ESP_OK;
}

uint8_t multitouch_poc_get_touch_count(void)
{
    return s_touch_count;
}
