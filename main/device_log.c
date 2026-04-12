#include "device_log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sd_card.h"

static const char *TAG = "device_log";

#define LOG_FILE_PATH SD_CARD_MOUNT_POINT "/device.log"

typedef struct {
    char type[DEVICE_LOG_MAX_TYPE];
    char message[DEVICE_LOG_MAX_MESSAGE];
    uint32_t runtime_ms;
} device_log_entry_t;

static device_log_entry_t s_entries[DEVICE_LOG_MAX_ENTRIES];
static size_t s_head;
static size_t s_count;
static SemaphoreHandle_t s_mutex;

static void log_append_to_file(const device_log_entry_t *entry)
{
    if (!sd_card_is_present()) {
        return;
    }
    FILE *f = fopen(LOG_FILE_PATH, "a");
    if (f == NULL) {
        return;
    }
    fprintf(f, "[%lu] %s: %s\n",
            (unsigned long)entry->runtime_ms, entry->type, entry->message);
    fclose(f);
}

esp_err_t device_log_init(void)
{
    s_head = 0;
    s_count = 0;

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    if (sd_card_is_present()) {
        FILE *f = fopen(LOG_FILE_PATH, "w");
        if (f != NULL) {
            fclose(f);
        }
    }

    return ESP_OK;
}

void device_log(const char *type, const char *fmt, ...)
{
    if (s_mutex == NULL) {
        return;
    }

    device_log_entry_t entry;
    uint64_t us = esp_timer_get_time();
    entry.runtime_ms = (uint32_t)(us / 1000);

    if (type != NULL) {
        strncpy(entry.type, type, sizeof(entry.type) - 1);
        entry.type[sizeof(entry.type) - 1] = '\0';
    } else {
        entry.type[0] = '\0';
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, sizeof(entry.message), fmt, args);
    va_end(args);

    ESP_LOGI(TAG, "[%s] %s", entry.type, entry.message);

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    size_t idx = (s_head + s_count) % DEVICE_LOG_MAX_ENTRIES;
    if (s_count == DEVICE_LOG_MAX_ENTRIES) {
        s_head = (s_head + 1) % DEVICE_LOG_MAX_ENTRIES;
    } else {
        s_count++;
    }
    s_entries[idx] = entry;

    xSemaphoreGive(s_mutex);

    log_append_to_file(&entry);
}

static size_t json_escape(char *dst, size_t dst_size, const char *src)
{
    size_t w = 0;
    for (const char *p = src; *p != '\0' && w + 6 < dst_size; p++) {
        switch (*p) {
        case '"':  dst[w++] = '\\'; dst[w++] = '"';  break;
        case '\\': dst[w++] = '\\'; dst[w++] = '\\'; break;
        case '\n': dst[w++] = '\\'; dst[w++] = 'n';  break;
        case '\r': dst[w++] = '\\'; dst[w++] = 'r';  break;
        case '\t': dst[w++] = '\\'; dst[w++] = 't';  break;
        default:   dst[w++] = *p; break;
        }
    }
    dst[w] = '\0';
    return w;
}

int device_log_get_json(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size < 32 || s_mutex == NULL) {
        return -1;
    }

    device_log_entry_t *snapshot = heap_caps_malloc(
        DEVICE_LOG_MAX_ENTRIES * sizeof(device_log_entry_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (snapshot == NULL) {
        snapshot = malloc(DEVICE_LOG_MAX_ENTRIES * sizeof(device_log_entry_t));
    }
    if (snapshot == NULL) {
        return -1;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        free(snapshot);
        return -1;
    }

    size_t count = s_count;
    size_t head = s_head;
    for (size_t i = 0; i < count; i++) {
        snapshot[i] = s_entries[(head + i) % DEVICE_LOG_MAX_ENTRIES];
    }

    xSemaphoreGive(s_mutex);

    int offset = snprintf(buf, buf_size, "{\"logs\":[");
    if (offset < 0) {
        free(snapshot);
        return -1;
    }

    char esc_type[DEVICE_LOG_MAX_TYPE * 2];
    char esc_msg[DEVICE_LOG_MAX_MESSAGE * 2];

    for (size_t i = 0; i < count; i++) {
        json_escape(esc_type, sizeof(esc_type), snapshot[i].type);
        json_escape(esc_msg, sizeof(esc_msg), snapshot[i].message);

        int n = snprintf(buf + offset, buf_size - (size_t)offset,
                         "%s{\"type\":\"%s\",\"message\":\"%s\",\"runtime\":%lu}",
                         (i > 0) ? "," : "",
                         esc_type, esc_msg,
                         (unsigned long)snapshot[i].runtime_ms);
        if (n < 0 || (size_t)(offset + n) >= buf_size) {
            break;
        }
        offset += n;
    }

    free(snapshot);

    int n = snprintf(buf + offset, buf_size - (size_t)offset, "]}");
    if (n >= 0) {
        offset += n;
    }

    return offset;
}
