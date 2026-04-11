#include "sd_card.h"

#include "bsp/esp32_s3_touch_amoled_1_75.h"
#include "esp_log.h"
#include "sdmmc_cmd.h"

static const char *TAG = "sd_card";

static bool s_initialized = false;
static bool s_mounted = false;

esp_err_t sd_card_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Calling bsp_sdcard_mount ...");
    esp_err_t err = bsp_sdcard_mount();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BSP SD card mount failed: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    s_mounted = true;
    ESP_LOGI(TAG, "SD card ready via BSP: %s %lluMB",
             bsp_sdcard->cid.name,
             ((uint64_t)bsp_sdcard->csd.capacity) * bsp_sdcard->csd.sector_size / (1024 * 1024));
    return ESP_OK;
}

bool sd_card_is_present(void)
{
    return s_initialized && (bsp_sdcard != NULL);
}

sdmmc_card_t *sd_card_get_card(void)
{
    return bsp_sdcard;
}

esp_err_t sd_card_mount(void)
{
    if (!s_initialized || bsp_sdcard == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_mounted) {
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t sd_card_unmount(void)
{
    if (!s_mounted) {
        return ESP_OK;
    }

    s_mounted = false;
    ESP_LOGI(TAG, "FAT access disabled (SDMMC host + card kept alive for MSC)");
    return ESP_OK;
}

uint32_t sd_card_block_count(void)
{
    if (!sd_card_is_present()) {
        return 0;
    }
    return (uint32_t)bsp_sdcard->csd.capacity;
}

uint16_t sd_card_block_size(void)
{
    if (!sd_card_is_present()) {
        return 0;
    }
    return (uint16_t)bsp_sdcard->csd.sector_size;
}

int32_t sd_card_read_blocks(uint32_t lba, void *buffer, uint32_t block_count)
{
    if (!sd_card_is_present() || (buffer == NULL) || (block_count == 0)) {
        return -1;
    }

    esp_err_t err = sdmmc_read_sectors(bsp_sdcard, buffer, lba, block_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Block read failed lba=%lu count=%lu: %s",
                 (unsigned long)lba, (unsigned long)block_count, esp_err_to_name(err));
        return -1;
    }

    return (int32_t)(block_count * bsp_sdcard->csd.sector_size);
}

int32_t sd_card_write_blocks(uint32_t lba, const void *buffer, uint32_t block_count)
{
    if (!sd_card_is_present() || (buffer == NULL) || (block_count == 0)) {
        return -1;
    }

    esp_err_t err = sdmmc_write_sectors(bsp_sdcard, buffer, lba, block_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Block write failed lba=%lu count=%lu: %s",
                 (unsigned long)lba, (unsigned long)block_count, esp_err_to_name(err));
        return -1;
    }

    return (int32_t)(block_count * bsp_sdcard->csd.sector_size);
}
