#include "audio_input.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "audio_input";

static esp_codec_dev_handle_t s_record_dev = NULL;
static bool s_ready = false;
static SemaphoreHandle_t s_read_lock = NULL;

static const i2s_std_config_t s_i2s_mic_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_INPUT_SAMPLE_RATE_HZ),
    .slot_cfg = I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
        .mclk = BSP_I2S_MCLK,
        .bclk = BSP_I2S_SCLK,
        .ws = BSP_I2S_LCLK,
        .dout = BSP_I2S_DOUT,
        .din = BSP_I2S_DSIN,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = false,
        },
    },
};

esp_err_t audio_input_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_audio_init(&s_i2s_mic_cfg), TAG, "Failed to initialize audio BSP");

    s_record_dev = bsp_audio_codec_microphone_init();
    ESP_RETURN_ON_FALSE(s_record_dev != NULL, ESP_FAIL, TAG, "Failed to initialize microphone codec");

    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = AUDIO_INPUT_BITS_PER_SAMPLE,
        .channel = AUDIO_INPUT_CHANNEL_COUNT,
        .channel_mask = 0,
        .sample_rate = AUDIO_INPUT_SAMPLE_RATE_HZ,
        .mclk_multiple = 0,
    };

    ESP_RETURN_ON_FALSE(esp_codec_dev_open(s_record_dev, &sample_info) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG, "Failed to open microphone stream");
    ESP_RETURN_ON_FALSE(esp_codec_dev_set_in_gain(s_record_dev, 24.0f) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG, "Failed to set microphone gain");
    if (s_read_lock == NULL) {
        s_read_lock = xSemaphoreCreateMutex();
        ESP_RETURN_ON_FALSE(s_read_lock != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create mic read mutex");
    }

    s_ready = true;
    ESP_LOGI(TAG, "Microphone initialized: %d Hz, %d-bit, %d ch", AUDIO_INPUT_SAMPLE_RATE_HZ, AUDIO_INPUT_BITS_PER_SAMPLE, AUDIO_INPUT_CHANNEL_COUNT);
    return ESP_OK;
}

bool audio_input_ready(void)
{
    return s_ready;
}

esp_err_t audio_input_read_raw(void *buffer, size_t len)
{
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "Buffer is NULL");
    ESP_RETURN_ON_FALSE(len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid frame size");

    if (!s_ready || s_record_dev == NULL) {
        memset(buffer, 0, len);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_FALSE(s_read_lock != NULL, ESP_ERR_INVALID_STATE, TAG, "Mic read mutex not ready");
    if (xSemaphoreTake(s_read_lock, portMAX_DELAY) != pdTRUE) {
        memset(buffer, 0, len);
        return ESP_ERR_TIMEOUT;
    }

    int ret = esp_codec_dev_read(s_record_dev, buffer, (int)len);
    xSemaphoreGive(s_read_lock);
    if (ret != ESP_CODEC_DEV_OK) {
        memset(buffer, 0, len);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t audio_input_read_frame(void *buffer, size_t len, bool ptt_active)
{
    esp_err_t err = audio_input_read_raw(buffer, len);
    if (err != ESP_OK) {
        return err;
    }

    if (!ptt_active) {
        memset(buffer, 0, len);
    }

    return ESP_OK;
}
