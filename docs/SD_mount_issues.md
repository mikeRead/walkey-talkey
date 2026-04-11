# SD Card Mount Issues — Investigation Log

This document records every approach attempted to get the SD card recognized
by a Windows host as a USB Mass Storage device on the Waveshare ESP32-S3 Touch
AMOLED 1.75 board, using ESP-IDF v5.5 and TinyUSB.

---

## 1. Hardware & Board Context

| Detail | Value |
|---|---|
| Board | Waveshare ESP32-S3-Touch-AMOLED-1.75 |
| MCU | ESP32-S3 |
| SD Card Slot | microSD, wired to SDMMC peripheral (1-bit mode) |
| SD Card Pins | CLK = GPIO 2, CMD = GPIO 1, D0 = GPIO 3 |
| Display Bus | QSPI on `SPI2_HOST` (GPIO 4/5/6/7/12/38) |
| USB | Single USB-C port shared between TinyUSB device and serial/JTAG |
| BSP Component | `waveshare__esp32_s3_touch_amoled_1_75` (managed component) |

**Key constraint:** The board has a single USB-C port. Once TinyUSB takes
control, the serial/JTAG debug console is no longer available, making it very
difficult to capture crash logs or ESP_LOG output from a running application.

---

## 2. Goal

Add USB Mass Storage Class (MSC) to the existing USB composite device
(Audio microphone + HID keyboard) so that:

1. The SD card appears as a removable drive in Windows.
2. Audio and HID continue to work alongside MSC.
3. The device boots reliably without crashes.

---

## 3. Existing USB Configuration (Known Good Baseline)

Before any SD card work, the device shipped with:

- **Audio**: 1-channel 48 kHz microphone via TinyUSB Audio class.
- **HID**: Keyboard + Consumer + System + Mouse reports.
- **No MSC** — no SD card exposure over USB.
- **CMakeLists.txt**: Compile definitions for `CFG_TUD_AUDIO=1` and related
  audio buffer sizes.
- **sdkconfig.defaults**: `CONFIG_TINYUSB_HID_COUNT=1`,
  `CONFIG_SR_MN_EN_MULTINET7_QUANT=y`.

This baseline booted reliably and all USB functions worked.

---

## 4. Attempt 1 — `esp_tinyusb` MSC Component via Kconfig

### What was done

Added MSC configuration to `sdkconfig.defaults`:

```
CONFIG_TINYUSB_MSC_ENABLED=y
CONFIG_TINYUSB_MSC_BUFSIZE=512
CONFIG_TINYUSB_DESC_MSC_STRING="WalKEY-TalKEY Storage"
CONFIG_TINYUSB_MSC_MOUNT_PATH="/sdcard"
```

Attempted to use the `esp_tinyusb` component's high-level MSC API:
- `tinyusb_msc_new_storage_sdmmc()` — to register the SD card.
- `tinyusb_msc_install_driver()` — to install the MSC driver.

### Result

**Boot loop.** The device would flash the UI for approximately one second,
then restart in an infinite loop. The `esp_tinyusb` MSC component has internal
dependencies that conflict with the manual Audio+HID descriptor layout already
in `usb_composite.c`. Specifically, its `tinyusb_msc.c` tries to own the MSC
descriptor and callbacks, colliding with the custom composite device setup.

### Resolution

Removed `tinyusb_msc_new_storage_sdmmc()` and `tinyusb_msc_install_driver()`
calls. Stripped all MSC-related entries from `sdkconfig.defaults`. Device
booted normally again, but without MSC.

---

## 5. Attempt 2 — Manual MSC via CMake Compile Definitions

### What was done

Instead of using the `esp_tinyusb` MSC component, enabled MSC at the TinyUSB
core level by adding compile definitions in the root `CMakeLists.txt`:

```cmake
add_compile_definitions(
    # ... existing audio defs ...
    CONFIG_TINYUSB_MSC_ENABLED=1
    CONFIG_TINYUSB_MSC_BUFSIZE=512
    CONFIG_TINYUSB_DESC_MSC_STRING="WalKEY-TalKEY Storage"
)
```

This causes `tusb_config.h` (from `esp_tinyusb`) to set `CFG_TUD_MSC=1`
without compiling the problematic `tinyusb_msc.c` file.

In `usb_composite.c`:
- Added `ITF_NUM_MSC` to the interface enum.
- Added `STRID_MSC` to the string enum.
- Added `TUD_MSC_DESCRIPTOR(...)` to the configuration descriptor array.
- Updated `TUSB_DESC_TOTAL_LEN` to include `TUD_MSC_DESC_LEN`.
- Updated endpoint addresses to avoid conflicts.
- Added empty stub functions `msc_storage_mount_to_usb()` and
  `msc_storage_mount_to_app()` to satisfy conditional calls inside
  `esp_tinyusb`'s `tinyusb.c`.
- Implemented all seven required MSC callbacks manually:
  - `tud_msc_inquiry_cb` — returns vendor/product/revision strings.
  - `tud_msc_test_unit_ready_cb` — checks `sd_card_is_present()`.
  - `tud_msc_capacity_cb` — returns block count and block size.
  - `tud_msc_start_stop_cb` — no-op (always ready).
  - `tud_msc_read10_cb` — calls `sd_card_read_blocks()`.
  - `tud_msc_write10_cb` — calls `sd_card_write_blocks()`.
  - `tud_msc_scsi_cb` — handles standard SCSI commands.

### SD Card Init: SPI Mode (First `sd_card.c` version)

The initial `sd_card.c` used the SPI bus to talk to the SD card:

```c
#define SD_SPI_HOST  SPI2_HOST
#define SD_PIN_MISO  GPIO_NUM_3   // BSP_SD_D0
#define SD_PIN_MOSI  GPIO_NUM_1   // BSP_SD_CMD
#define SD_PIN_CLK   GPIO_NUM_2   // BSP_SD_CLK
#define SD_PIN_CS    GPIO_NUM_NC
```

Used `sdspi_host_init()`, `sdspi_host_init_device()`, and
`sdmmc_card_init()`.

### Result

**Boot loop resolved** — the USB composite device now enumerated in Windows
with Audio, HID, and MSC interfaces. However, the SD card showed as **"No
Media"** in Windows Disk Management.

**Root cause:** `SPI2_HOST` is already used by the display's QSPI bus. The
BSP initializes `SPI2_HOST` during `bsp_display_start()`, so when
`sd_card_init()` tried to initialize the same SPI host, there was a conflict.
The SD card could not communicate because the SPI bus was owned by the
display driver.

---

## 6. Attempt 3 — SPI Mode on `SPI3_HOST`

### What was done

Changed the SPI host to `SPI3_HOST` to avoid the display bus conflict:

```c
#define SD_SPI_HOST  SPI3_HOST
```

### Result

**Instant boot loop.** The device would not even briefly show the UI —
it rebooted immediately in a tight loop. `SPI3_HOST` initialization itself
caused the crash. On ESP32-S3, `SPI3_HOST` (FSPI) may not be available or
may have pin conflicts depending on the board's GPIO usage.

### Resolution

Abandoned SPI approach entirely.

---

## 7. Attempt 4 — Native SDMMC 1-Bit Mode (Manual Init)

### What was done

Rewrote `sd_card.c` to use the native SDMMC peripheral in 1-bit mode,
matching the BSP's pin assignments:

```c
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
host.max_freq_khz = SDMMC_FREQ_DEFAULT;
host.flags &= ~SDMMC_HOST_FLAG_4BIT;  // Force 1-bit mode

sdmmc_slot_config_t slot_config = {
    .clk = BSP_SD_CLK,   // GPIO 2
    .cmd = BSP_SD_CMD,    // GPIO 1
    .d0  = BSP_SD_D0,     // GPIO 3
    .d1 through .d7 = GPIO_NUM_NC,
    .cd  = SDMMC_SLOT_NO_CD,
    .wp  = SDMMC_SLOT_NO_WP,
    .width = 1,
    .flags = 0,
};
```

Manually called `sdmmc_host_init()`, `sdmmc_host_init_slot()`,
allocated `sdmmc_card_t`, assigned `s_card->host = host`, and
called `sdmmc_card_init()`.

### Result

**Instant boot loop.** Same symptom as Attempt 3 — no UI visible,
immediate restart cycle.

### Analysis

The manual SDMMC initialization code was functionally identical to
what the BSP does internally, but something about the initialization
sequence or timing was causing a crash before the display could render.
Without serial output (due to the shared USB port), the exact crash
reason could not be determined.

---

## 8. Attempt 5 — BSP `bsp_sdcard_mount()` Only

### What was done

Simplified `sd_card.c` to delegate entirely to the BSP's tested
implementation:

```c
esp_err_t sd_card_init(void)
{
    ESP_LOGI(TAG, "Calling bsp_sdcard_mount ...");
    esp_err_t err = bsp_sdcard_mount();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BSP SD card mount failed: %s", esp_err_to_name(err));
        return err;
    }
    s_initialized = true;
    s_mounted = true;
    // Uses bsp_sdcard global for card info
    return ESP_OK;
}
```

All block read/write functions used the `bsp_sdcard` global handle from
the BSP.

### Result (First Flash)

**No boot loop.** The device booted normally. However, Windows showed the
SD card as **"No Media"** in Disk Management. The MSC interface was
enumerated but reported no usable storage.

On-screen diagnostics showed: `SD: ESP_FAIL (0xffffffff)`

This means `bsp_sdcard_mount()` returned `ESP_FAIL`. The BSP's
`bsp_sdcard_mount()` calls `esp_vfs_fat_sdmmc_mount()`, which
initializes the SDMMC host, probes the card, AND mounts the FAT
filesystem in one call. If FAT mount fails, the entire call fails.

**Likely cause:** The SD card was formatted as **exFAT**, which is not
supported by ESP-IDF's FatFS library. Modern SD cards (32 GB+) are
commonly pre-formatted as exFAT. ESP-IDF's `esp_vfs_fat_sdmmc_mount()`
requires FAT12/FAT16/FAT32.

### Result (Second Flash — Same Code)

**Boot loop.** Flashing the exact same binary again produced a boot loop
with no UI visible. This was unexpected and non-deterministic — the same
code had just worked. This points to possible:
- Flash state corruption or stale `sdkconfig` cache.
- Timing-sensitive initialization race conditions.
- Memory layout differences between builds.

---

## 9. Attempt 6 — BSP Mount with Manual SDMMC Fallback

### What was done

Added a fallback strategy: try `bsp_sdcard_mount()` first, and if it
fails (as expected with exFAT), fall back to manual SDMMC init without
FAT, keeping the raw card handle alive for MSC block access:

```c
esp_err_t sd_card_init(void)
{
    // Try BSP mount first (needs FAT)
    esp_err_t err = bsp_sdcard_mount();
    if (err == ESP_OK) {
        s_initialized = true;
        s_mounted = true;
        return ESP_OK;
    }

    // BSP failed — try raw SDMMC init
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags &= ~SDMMC_HOST_FLAG_4BIT;
    // ... manual slot config with BSP pins ...
    // ... sdmmc_host_init + slot init + card init ...
}
```

### Result

**Instant boot loop.** The fallback path triggered the same crash as
Attempt 4, suggesting that `sdmmc_host_init()` itself was problematic
when called after a failed `bsp_sdcard_mount()` (which may have left
the SDMMC peripheral in a partially-initialized state).

---

## 10. Attempt 7 — On-Screen Diagnostics

### What was done

Since serial monitoring was not available (TinyUSB takes over the USB
port), added on-screen diagnostic display to capture the SD card init
error code:

```c
// In main.c
EXT_RAM_BSS_ATTR static char s_sd_diag_text[48] = {0};

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_display_start() ? ESP_OK : ESP_FAIL);
    esp_err_t sd_err = sd_card_init();
    snprintf(s_sd_diag_text, sizeof(s_sd_diag_text),
             "SD: %s (0x%x)",
             (sd_err == ESP_OK) ? "OK" : esp_err_to_name(sd_err),
             (unsigned)sd_err);
    // ... display sd_diag_text in top status bar ...
}
```

### Result

When the device did boot, the display showed `SD: ESP_FAIL (0xffffffff)`,
confirming that `bsp_sdcard_mount()` was returning `ESP_FAIL`. When the
device boot-looped, the UI was never visible, so the diagnostic was useless.

---

## 11. Attempt 8 — Serial Monitor via Delayed USB Init

### What was done

Added a 10-second delay before USB initialization in `app_main()` to
keep the serial/JTAG port available long enough to capture boot logs:

```c
ESP_LOGI(TAG, "Delaying 10s for serial debug...");
vTaskDelay(pdMS_TO_TICKS(10000));
ESP_ERROR_CHECK(usb_composite_init());
```

### Result

`idf.py monitor` connected but saw the device in **DOWNLOAD** mode
rather than running the application. The hard reset after flashing was
putting the device into download mode. Manually pressing the RST button
would start the app, but by then the serial port was already taken over
by TinyUSB.

This approach was abandoned in favor of on-screen diagnostics.

---

## 12. Summary of All Approaches

| # | Approach | SD Init Method | Result | Root Cause |
|---|---|---|---|---|
| 1 | `esp_tinyusb` MSC API via Kconfig | `tinyusb_msc_new_storage_sdmmc()` | Boot loop | Component conflicts with custom composite device |
| 2 | Manual MSC + SPI on `SPI2_HOST` | `sdspi_host` on SPI2 | "No Media" | SPI bus conflict with display |
| 3 | Manual MSC + SPI on `SPI3_HOST` | `sdspi_host` on SPI3 | Instant boot loop | SPI3 init crash (pin conflict or unavailable) |
| 4 | Manual MSC + SDMMC 1-bit | `sdmmc_host_init()` manual | Instant boot loop | Unknown crash during SDMMC init |
| 5 | Manual MSC + `bsp_sdcard_mount()` | BSP function | No crash but "No Media" / ESP_FAIL | exFAT not supported by FatFS; non-deterministic boot loop on reflash |
| 6 | BSP mount → SDMMC fallback | BSP then manual fallback | Instant boot loop | Partial SDMMC state from failed BSP call + re-init crash |
| 7 | On-screen diagnostics | (diagnostic only) | `SD: ESP_FAIL (0xffffffff)` | Confirmed mount failure |
| 8 | Delayed USB init for serial | (debug only) | Could not capture logs | Single USB port, JTAG unavailable during app |

---

## 13. Suspected Root Causes

### 13a. SD Card Filesystem Format (MOST LIKELY ROOT CAUSE)

**The SD card used was 100+ GB (SDXC class).** This is almost certainly
the primary cause of the `ESP_FAIL` errors:

- **SDXC cards (64 GB+) are factory-formatted as exFAT** per the SD
  specification. This is mandatory — no SDXC card ships as FAT32.
- **ESP-IDF's FatFS does not support exFAT.** It only supports FAT12,
  FAT16, and FAT32.
- `bsp_sdcard_mount()` calls `esp_vfs_fat_sdmmc_mount()`, which combines
  SDMMC host init + card probe + FAT mount into one call. If FAT mount
  fails (as it will with exFAT), the entire call returns `ESP_FAIL` and
  deinitializes the SDMMC host, leaving no card handle for MSC to use.
- This perfectly explains the `SD: ESP_FAIL (0xffffffff)` seen on-screen.

**Recommended fix:** Format the SD card as **FAT32** on a computer before
inserting it into the board. Windows' built-in format tool does not offer
FAT32 for volumes over 32 GB, so use a third-party tool like **Rufus**
or **guiformat** ("FAT32 Format" by Ridgecrop).

### 13b. SDMMC Host Init Instability

Multiple attempts to manually initialize the SDMMC host caused instant
boot loops. This could be due to:

- The SDMMC peripheral being partially initialized by a failed
  `bsp_sdcard_mount()` call, leaving it in a bad state.
- GPIO conflicts with other peripherals.
- Stack overflow or memory corruption during SDMMC DMA setup.
- The `SDMMC_HOST_DEFAULT()` macro defaulting to 4-bit mode flags that
  need explicit clearing for 1-bit operation.

### 13c. `esp_vfs_fat_sdmmc_mount()` All-or-Nothing Design

The BSP's `bsp_sdcard_mount()` uses `esp_vfs_fat_sdmmc_mount()`, which
does not expose intermediate states. If the card is present but the
filesystem is unreadable (exFAT, corrupted, unformatted), the function
fails and tears down the entire SDMMC stack. There is no way to get
just a raw card handle for MSC block access through this API.

For MSC to work with arbitrary filesystem formats, the initialization
needs to be split:
1. Initialize SDMMC host and probe card → get `sdmmc_card_t*`
2. Optionally mount FAT for local file access
3. Expose the raw card handle to MSC regardless of FAT mount result

### 13d. Single USB Port Debugging Limitation

The ESP32-S3's built-in USB serial/JTAG controller shares the same USB
PHY as TinyUSB. Once `tinyusb_driver_install()` is called, the serial
console is no longer available. This makes it nearly impossible to
capture crash backtraces for boot loops, which would immediately reveal
the exact line causing the crash.

---

## 14. Files Created During Investigation

All files remain in the repo as untracked (not part of the build since
`main/CMakeLists.txt` was reverted):

| File | Purpose |
|---|---|
| `main/sd_card.h` | SD card abstraction header |
| `main/sd_card.c` | SD card init/mount/block-access implementation (last version: manual SDMMC) |
| `main/sd_card_config.h` | Header for config-file population on SD card |
| `main/sd_card_config.c` | Writes default config JSON, schema, and docs to SD card |
| `main/sd_card.Readme.md` | AI/human notes for `sd_card.c` |
| `main/sd_card_config.Readme.md` | AI/human notes for `sd_card_config.c` |

---

## 15. Resolution (2026-04-10)

**Root cause confirmed: the 128 GB SDXC card was factory-formatted as
exFAT**, which ESP-IDF's FatFS does not support.

### Fix

1. Formatted the SD card as **FAT32** using **Rufus** (Large FAT32, MBR,
   32 KB cluster size).
2. Used the BSP-only approach (Attempt 5): `sd_card_init()` calls
   `bsp_sdcard_mount()`, which succeeded with `ESP_OK`.
3. MSC callbacks in `usb_composite.c` use `bsp_sdcard` for raw block
   read/write via `sdmmc_read_sectors()` / `sdmmc_write_sectors()`.

### Result

- Device boots normally — no boot loop.
- On-screen status shows `SD: <card_name> <size>MB`.
- Windows recognizes the 128 GB drive (D:) as a removable FAT32 volume.
- `sd_card_config_populate()` successfully writes `AI_GUIDE.md`,
  `USER_GUIDE.md`, `mode-config.json`, and `mode-config.schema.json`
  to the card.
- Audio microphone and HID keyboard continue to work alongside MSC.

### Key Takeaway

Always format SD cards as **FAT32** for use with ESP-IDF. Cards 64 GB
and larger (SDXC) ship as exFAT and must be reformatted. Windows' built-in
format tool does not offer FAT32 for volumes over 32 GB — use Rufus or
a similar tool.

---

## 17. BSP Reference Code

For reference, this is the BSP's SD card implementation
(`waveshare__esp32_s3_touch_amoled_1_75/esp32_s3_touch_amoled_1_75.c`):

```c
sdmmc_card_t *bsp_sdcard = NULL;

esp_err_t bsp_sdcard_mount(void)
{
    const esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    const sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    const sdmmc_slot_config_t slot_config = {
        .clk = BSP_SD_CLK,    // GPIO 2
        .cmd = BSP_SD_CMD,    // GPIO 1
        .d0 = BSP_SD_D0,      // GPIO 3
        .d1 = GPIO_NUM_NC,
        // ... d2-d7 = GPIO_NUM_NC ...
        .cd = SDMMC_SLOT_NO_CD,
        .wp = SDMMC_SLOT_NO_WP,
        .width = 1,
        .flags = 0,
    };

    return esp_vfs_fat_sdmmc_mount(
        BSP_SD_MOUNT_POINT, &host, &slot_config,
        &mount_config, &bsp_sdcard);
}
```

Note that `SDMMC_HOST_DEFAULT()` defaults to 4-bit width in its `.flags`,
but the slot's `.width = 1` overrides this at the slot level. The BSP
does **not** clear `SDMMC_HOST_FLAG_4BIT` from `host.flags`.
