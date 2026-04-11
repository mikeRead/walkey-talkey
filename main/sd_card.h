#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "sdmmc_cmd.h"

#define SD_CARD_MOUNT_POINT "/sdcard"

esp_err_t sd_card_init(void);
bool sd_card_is_present(void);
esp_err_t sd_card_mount(void);
esp_err_t sd_card_unmount(void);
sdmmc_card_t *sd_card_get_card(void);

uint32_t sd_card_block_count(void);
uint16_t sd_card_block_size(void);
int32_t sd_card_read_blocks(uint32_t lba, void *buffer, uint32_t block_count);
int32_t sd_card_write_blocks(uint32_t lba, const void *buffer, uint32_t block_count);
