# sd_card

## Human Reference

SD card driver for the Waveshare ESP32-S3 Touch AMOLED 1.75 onboard microSD slot.

Delegates to the BSP's `bsp_sdcard_mount()` for SDMMC host init + FAT mount, then exposes raw block I/O for USB Mass Storage.

Pin mapping (SDMMC 1-bit mode, from BSP header):

| Signal | GPIO |
|--------|------|
| CLK    | 2    |
| CMD    | 1    |
| D0     | 3    |

Key design decisions:

- Uses the BSP's `bsp_sdcard_mount()` which calls `esp_vfs_fat_sdmmc_mount()` internally. This handles SDMMC peripheral init, card probe, and FAT filesystem mount in one call.
- The `bsp_sdcard` global (from the BSP) is the card handle used for raw sector reads/writes by the MSC callbacks in `usb_composite.c`.
- `sd_card_unmount()` marks FAT as disabled but does NOT deinit the SDMMC host, keeping the card handle alive for MSC.
- The SD card must be formatted as FAT32. exFAT is not supported by ESP-IDF's FatFS.

## AI Reference

- `sd_card_init()` must be called before any other function; it is safe to call multiple times.
- After init, standard C file I/O works under `/sdcard/` (via BSP mount).
- `sd_card_get_card()` returns the BSP's `bsp_sdcard` handle.
- `sd_card_read_blocks()` / `sd_card_write_blocks()` use `sdmmc_read_sectors()` / `sdmmc_write_sectors()` directly on the BSP handle.
- Block size is typically 512 bytes; block count comes from the card's CSD register.
- See `docs/SD_mount_issues.md` for the full investigation log of approaches tried.
