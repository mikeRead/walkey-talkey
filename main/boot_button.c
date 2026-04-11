#include "boot_button.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define BOOT_BUTTON_POLL_MS 10
#define BOOT_BUTTON_DEBOUNCE_MS 30
#define BOOT_BUTTON_STABLE_SAMPLES (BOOT_BUTTON_DEBOUNCE_MS / BOOT_BUTTON_POLL_MS)

static const char *TAG = "boot_button";

static boot_button_event_cb_t s_event_cb = NULL;
static void *s_event_user_data = NULL;
static TaskHandle_t s_task_handle = NULL;
static bool s_stable_pressed = false;

static bool boot_button_read_raw(void)
{
    return gpio_get_level(BOOT_BUTTON_GPIO) == 0;
}

static void boot_button_task(void *arg)
{
    bool last_raw_pressed = boot_button_read_raw();
    uint32_t stable_samples = BOOT_BUTTON_STABLE_SAMPLES;

    s_stable_pressed = last_raw_pressed;

    while (1) {
        bool raw_pressed = boot_button_read_raw();

        if (raw_pressed == last_raw_pressed) {
            if (stable_samples < BOOT_BUTTON_STABLE_SAMPLES) {
                stable_samples++;
            }
        } else {
            last_raw_pressed = raw_pressed;
            stable_samples = 1;
        }

        if ((stable_samples >= BOOT_BUTTON_STABLE_SAMPLES) && (raw_pressed != s_stable_pressed)) {
            s_stable_pressed = raw_pressed;
            ESP_LOGI(TAG, "BOOT %s detected", s_stable_pressed ? "press" : "release");

            if (s_event_cb != NULL) {
                s_event_cb(s_stable_pressed, s_event_user_data);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BOOT_BUTTON_POLL_MS));
    }
}

esp_err_t boot_button_init(boot_button_event_cb_t event_cb, void *user_data)
{
    if (s_task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Failed to configure BOOT GPIO");

    s_event_cb = event_cb;
    s_event_user_data = user_data;
    s_stable_pressed = boot_button_read_raw();

    BaseType_t task_created = xTaskCreate(
        boot_button_task,
        "boot_button",
        3072,
        NULL,
        5,
        &s_task_handle
    );

    if (task_created != pdPASS) {
        s_task_handle = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

bool boot_button_is_pressed(void)
{
    return s_stable_pressed;
}
