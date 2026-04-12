#include "audio_recorder.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

#include "audio_input.h"
#include "device_log.h"
#include "sd_card.h"

static const char *TAG = "audio_recorder";

#define RING_BUFFER_SIZE 16384
#define WRITER_TASK_STACK 4096
#define WRITER_TASK_PRIORITY 4
#define WRITER_DRAIN_BYTES 1024
#define WAV_HEADER_SIZE 44
#define WRITER_STOP_TIMEOUT_MS 3000
#define RECORDINGS_ROOT SD_CARD_MOUNT_POINT "/recordings"

typedef struct {
    uint8_t riff[4];
    uint32_t file_size;
    uint8_t wave[4];
    uint8_t fmt_id[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint8_t data_id[4];
    uint32_t data_size;
} __attribute__((packed)) wav_header_t;

static bool s_initialized = false;
static uint32_t s_session_id = 0;
static volatile bool s_recording = false;
static FILE *s_file = NULL;
static uint32_t s_data_bytes_written = 0;
static StreamBufferHandle_t s_ring_buffer = NULL;
static uint8_t *s_ring_storage = NULL;
static StaticStreamBuffer_t s_ring_struct;
static TaskHandle_t s_writer_task_handle = NULL;
static volatile bool s_writer_running = false;
static SemaphoreHandle_t s_writer_done_sem = NULL;
static uint32_t s_feed_bytes_total = 0;
static uint32_t s_feed_call_count = 0;

static bool audio_recorder_ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            ESP_LOGE(TAG, "'%s' exists but is not a directory", path);
            return false;
        }
        return true;
    }
    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(TAG, "mkdir '%s' failed: %s", path, strerror(errno));
        return false;
    }
    return true;
}

static void audio_recorder_write_wav_header(FILE *f, uint32_t data_size)
{
    wav_header_t hdr = {
        .riff = {'R', 'I', 'F', 'F'},
        .file_size = WAV_HEADER_SIZE - 8 + data_size,
        .wave = {'W', 'A', 'V', 'E'},
        .fmt_id = {'f', 'm', 't', ' '},
        .fmt_size = 16,
        .audio_format = 1,
        .num_channels = AUDIO_INPUT_CHANNEL_COUNT,
        .sample_rate = AUDIO_INPUT_SAMPLE_RATE_HZ,
        .byte_rate = AUDIO_INPUT_SAMPLE_RATE_HZ * AUDIO_INPUT_CHANNEL_COUNT * (AUDIO_INPUT_BITS_PER_SAMPLE / 8),
        .block_align = (uint16_t)(AUDIO_INPUT_CHANNEL_COUNT * (AUDIO_INPUT_BITS_PER_SAMPLE / 8)),
        .bits_per_sample = AUDIO_INPUT_BITS_PER_SAMPLE,
        .data_id = {'d', 'a', 't', 'a'},
        .data_size = data_size,
    };
    size_t written = fwrite(&hdr, 1, sizeof(hdr), f);
    if (written != sizeof(hdr)) {
        ESP_LOGE(TAG, "WAV header write failed: %u of %u bytes",
                 (unsigned)written, (unsigned)sizeof(hdr));
    }
}

static void audio_recorder_writer_task(void *arg)
{
    (void)arg;
    uint8_t drain_buf[WRITER_DRAIN_BYTES];

    while (s_writer_running) {
        size_t received = xStreamBufferReceive(s_ring_buffer, drain_buf, sizeof(drain_buf), pdMS_TO_TICKS(50));
        if (received > 0 && s_file != NULL) {
            size_t written = fwrite(drain_buf, 1, received, s_file);
            if (written == received) {
                s_data_bytes_written += (uint32_t)written;
            } else {
                ESP_LOGW(TAG, "Short write: %u of %u bytes", (unsigned)written, (unsigned)received);
            }
        }
    }

    /* Drain remaining data after stop signal. */
    for (;;) {
        size_t received = xStreamBufferReceive(s_ring_buffer, drain_buf, sizeof(drain_buf), 0);
        if (received == 0) {
            break;
        }
        if (s_file != NULL) {
            size_t written = fwrite(drain_buf, 1, received, s_file);
            if (written == received) {
                s_data_bytes_written += (uint32_t)written;
            }
        }
    }

    xSemaphoreGive(s_writer_done_sem);
    vTaskDelete(NULL);
}

esp_err_t audio_recorder_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_writer_done_sem = xSemaphoreCreateBinary();
    if (s_writer_done_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create writer semaphore");
        return ESP_ERR_NO_MEM;
    }

    s_session_id = esp_random();
    s_initialized = true;
    ESP_LOGI(TAG, "Initialized, session %08lX", (unsigned long)s_session_id);
    return ESP_OK;
}

esp_err_t audio_recorder_start(const char *mode_name)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_recording) {
        return ESP_OK;
    }

    if (!sd_card_is_present()) {
        ESP_LOGW(TAG, "No SD card present, skipping recording");
        return ESP_ERR_NOT_FOUND;
    }

    if (s_ring_buffer == NULL) {
        s_ring_storage = heap_caps_malloc(RING_BUFFER_SIZE + 1, MALLOC_CAP_SPIRAM);
        if (s_ring_storage == NULL) {
            ESP_LOGE(TAG, "Failed to allocate ring buffer in PSRAM");
            return ESP_ERR_NO_MEM;
        }
        s_ring_buffer = xStreamBufferCreateStatic(RING_BUFFER_SIZE, 1,
                                                   s_ring_storage, &s_ring_struct);
        if (s_ring_buffer == NULL) {
            heap_caps_free(s_ring_storage);
            s_ring_storage = NULL;
            ESP_LOGE(TAG, "Failed to create ring buffer");
            return ESP_ERR_NO_MEM;
        }
    }

    if (!audio_recorder_ensure_dir(RECORDINGS_ROOT)) {
        return ESP_FAIL;
    }

    char mode_dir[80];
    snprintf(mode_dir, sizeof(mode_dir), RECORDINGS_ROOT "/%s",
             (mode_name != NULL) ? mode_name : "unknown");
    if (!audio_recorder_ensure_dir(mode_dir)) {
        return ESP_FAIL;
    }

    uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
    char path[128];
    snprintf(path, sizeof(path), "%s/%08lX_%05lu.wav",
             mode_dir, (unsigned long)s_session_id, (unsigned long)uptime_sec);

    errno = 0;
    s_file = fopen(path, "wb");
    if (s_file == NULL) {
        ESP_LOGE(TAG, "Cannot open %s: %s", path, strerror(errno));
        return ESP_FAIL;
    }

    s_data_bytes_written = 0;
    audio_recorder_write_wav_header(s_file, 0);

    xStreamBufferReset(s_ring_buffer);
    xSemaphoreTake(s_writer_done_sem, 0);

    s_writer_running = true;
    BaseType_t ret = xTaskCreate(audio_recorder_writer_task, "rec_wr",
                                 WRITER_TASK_STACK, NULL,
                                 WRITER_TASK_PRIORITY, &s_writer_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create writer task");
        fclose(s_file);
        s_file = NULL;
        s_writer_running = false;
        return ESP_ERR_NO_MEM;
    }

    s_feed_bytes_total = 0;
    s_feed_call_count = 0;
    s_recording = true;
    ESP_LOGI(TAG, "Recording started: %s", path);
    device_log("ACTION", "Recording started: %s", path);
    return ESP_OK;
}

esp_err_t audio_recorder_feed(const void *pcm, size_t len)
{
    if (!s_recording || s_ring_buffer == NULL) {
        return ESP_OK;
    }

    size_t sent = xStreamBufferSend(s_ring_buffer, pcm, len, 0);
    s_feed_bytes_total += (uint32_t)sent;
    s_feed_call_count++;

    return ESP_OK;
}

esp_err_t audio_recorder_stop(void)
{
    if (!s_recording) {
        return ESP_OK;
    }

    s_recording = false;
    s_writer_running = false;

    if (xSemaphoreTake(s_writer_done_sem, pdMS_TO_TICKS(WRITER_STOP_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Writer task did not finish within %d ms", WRITER_STOP_TIMEOUT_MS);
    }
    s_writer_task_handle = NULL;

    if (s_file != NULL) {
        fflush(s_file);
        fseek(s_file, 0, SEEK_SET);
        audio_recorder_write_wav_header(s_file, s_data_bytes_written);
        fflush(s_file);
        fclose(s_file);
        s_file = NULL;
        ESP_LOGI(TAG, "Recording stopped: fed=%lu written=%lu",
                 (unsigned long)s_feed_bytes_total,
                 (unsigned long)s_data_bytes_written);
        device_log("ACTION", "Recording stopped: %lu bytes", (unsigned long)s_data_bytes_written);
    }

    s_ring_buffer = NULL;
    if (s_ring_storage != NULL) {
        heap_caps_free(s_ring_storage);
        s_ring_storage = NULL;
    }

    return ESP_OK;
}

bool audio_recorder_is_recording(void)
{
    return s_recording;
}
