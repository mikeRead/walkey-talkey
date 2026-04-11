#include "sd_card_config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "mode_config.h"
#include "sd_card.h"

static const char *TAG = "sd_card_config";

#define SD_CONFIG_PATH SD_CARD_MOUNT_POINT "/mode-config.json"
#define SD_DOCS_DIR SD_CARD_MOUNT_POINT "/docs"
#define SD_SCHEMA_PATH SD_DOCS_DIR "/mode-config.schema.json"
#define SD_AI_GUIDE_PATH SD_DOCS_DIR "/AI_GUIDE.md"
#define SD_USER_GUIDE_PATH SD_DOCS_DIR "/USER_GUIDE.md"
#define SD_REST_API_PATH SD_DOCS_DIR "/REST_API.md"
#define SD_README_PATH SD_CARD_MOUNT_POINT "/README.md"
#define SD_AUTORUN_INF_PATH SD_CARD_MOUNT_POINT "/autorun.inf"
#define SD_ICON_PATH SD_CARD_MOUNT_POINT "/ico.ico"

extern const uint8_t schema_start[] asm("_binary_mode_config_schema_json_start");
extern const uint8_t schema_end[] asm("_binary_mode_config_schema_json_end");
extern const uint8_t ai_guide_start[] asm("_binary_AI_GUIDE_md_start");
extern const uint8_t ai_guide_end[] asm("_binary_AI_GUIDE_md_end");
extern const uint8_t user_guide_start[] asm("_binary_USER_GUIDE_md_start");
extern const uint8_t user_guide_end[] asm("_binary_USER_GUIDE_md_end");
extern const uint8_t rest_api_start[] asm("_binary_REST_API_md_start");
extern const uint8_t rest_api_end[] asm("_binary_REST_API_md_end");
extern const uint8_t readme_start[] asm("_binary_README_md_start");
extern const uint8_t readme_end[] asm("_binary_README_md_end");
extern const uint8_t autorun_inf_start[] asm("_binary_autorun_inf_start");
extern const uint8_t autorun_inf_end[] asm("_binary_autorun_inf_end");
extern const uint8_t ico_ico_start[] asm("_binary_ico_ico_start");
extern const uint8_t ico_ico_end[] asm("_binary_ico_ico_end");

static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static bool write_file(const char *path, const void *data, size_t length)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        ESP_LOGW(TAG, "Cannot open %s for writing: %s", path, strerror(errno));
        return false;
    }
    size_t written = fwrite(data, 1, length, f);
    fclose(f);
    if (written != length) {
        ESP_LOGW(TAG, "Short write %s: %u of %u", path, (unsigned)written, (unsigned)length);
        return false;
    }
    ESP_LOGI(TAG, "Wrote %s (%u bytes)", path, (unsigned)length);
    return true;
}

static void ensure_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        mkdir(path, 0755);
    }
}

esp_err_t sd_card_config_populate(void)
{
    ensure_directory(SD_DOCS_DIR);

    if (!file_exists(SD_CONFIG_PATH)) {
        const char *builtin = mode_config_builtin_json();
        if (builtin != NULL) {
            write_file(SD_CONFIG_PATH, builtin, strlen(builtin));
        }
    }

    if (!file_exists(SD_SCHEMA_PATH)) {
        size_t len = (size_t)(schema_end - schema_start);
        write_file(SD_SCHEMA_PATH, schema_start, len);
    }

    if (!file_exists(SD_AI_GUIDE_PATH)) {
        size_t len = (size_t)(ai_guide_end - ai_guide_start);
        write_file(SD_AI_GUIDE_PATH, ai_guide_start, len);
    }

    if (!file_exists(SD_USER_GUIDE_PATH)) {
        size_t len = (size_t)(user_guide_end - user_guide_start);
        write_file(SD_USER_GUIDE_PATH, user_guide_start, len);
    }

    if (!file_exists(SD_REST_API_PATH)) {
        size_t len = (size_t)(rest_api_end - rest_api_start);
        write_file(SD_REST_API_PATH, rest_api_start, len);
    }

    if (!file_exists(SD_README_PATH)) {
        size_t len = (size_t)(readme_end - readme_start);
        write_file(SD_README_PATH, readme_start, len);
    }

    if (!file_exists(SD_AUTORUN_INF_PATH)) {
        size_t len = (size_t)(autorun_inf_end - autorun_inf_start);
        write_file(SD_AUTORUN_INF_PATH, autorun_inf_start, len);
    }

    if (!file_exists(SD_ICON_PATH)) {
        size_t len = (size_t)(ico_ico_end - ico_ico_start);
        write_file(SD_ICON_PATH, ico_ico_start, len);
    }

    return ESP_OK;
}

bool sd_card_config_read(char **out_json)
{
    if (out_json == NULL) {
        return false;
    }
    *out_json = NULL;

    if (!file_exists(SD_CONFIG_PATH)) {
        return false;
    }

    FILE *f = fopen(SD_CONFIG_PATH, "rb");
    if (f == NULL) {
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    long length = ftell(f);
    if (length <= 0) {
        fclose(f);
        return false;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    char *buffer = (char *)calloc((size_t)length + 1, 1);
    if (buffer == NULL) {
        fclose(f);
        return false;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)length, f);
    fclose(f);

    if (bytes_read != (size_t)length) {
        free(buffer);
        return false;
    }

    buffer[length] = '\0';
    *out_json = buffer;
    ESP_LOGI(TAG, "Read %s (%ld bytes)", SD_CONFIG_PATH, length);
    return true;
}
