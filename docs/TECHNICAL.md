# Technical Details

Developer and contributor reference for WalKEY-TalKEY. For the product overview, see the [main README](../README.md). For end-user setup and JSON config authoring, see the [User Guide](USER_GUIDE.md).

## Hardware

| Feature | Spec |
|---|---|
| MCU | ESP32-S3 @ 240 MHz |
| Display | 1.75" AMOLED (CO5300 controller) |
| Touch | CST9217 capacitive |
| Flash | 16 MB QIO |
| PSRAM | Octal SPI, 80 MHz |

Board: [Waveshare ESP32-S3 Touch AMOLED 1.75"](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75)

## SD Card Notes

- The board hardware can use large microSD cards, including `128 GB` cards, as long as the card can be mounted by the ESP-IDF FAT filesystem stack.
- For best compatibility, format the card as `FAT32`.
- Avoid `exFAT` unless you have separately added and tested support for it in the firmware.
- The BSP SD card path is wired for `SDMMC` in `1-bit` mode.

## Controls And UI

- The main card shows the active mode as a large centered heading
- There is no separate `Touch Controller` title row
- Status/hint text lives inside the main card and is blank by default until a mode/action sets it
- Gesture debug text is shown as a small gray label inside the main card and stays visible until a newer gesture replaces it
- A large circular BOOT-position marker is drawn near the physical BOOT button for alignment/debugging
- The BOOT marker uses the same green accent as the heading by default and turns red while `BOOT` is held
- Holding `BOOT` opens the simplified mode selector with top instruction text, a bottom confirm hint, and the centered active mode label still visible
- Pressing the touchscreen briefly shifts the touch-feedback palette to a dark red pressed state
- Normal touch gestures show text labels such as `PRESS`, `TAP`, `DOUBLE TAP`, `LONG PRESS`, `HOLD END`, and swipe directions instead of arrow glyphs
- Cursor-mode touch hold keeps the dictation workflow: 400+ ms hold enables mic gate and sends `F13`, release disables mic gate and releases `F13`
- Cursor-mode tap sends `F14`
- Cursor-mode double tap sends `Enter`
- Cursor-mode swipe up sends `Ctrl+A` then `Backspace` to clear the field
- Cursor-mode swipe down sends `Ctrl+.` to toggle Cursor text mode
- Cursor-mode swipe left sends `Ctrl+N`
- Cursor-mode swipe right sends `Enter`
- BOOT long press resets the selection to `Cursor`
- Swipe actions are edge-triggered and should fire once per gesture, not repeat while the finger is still moving

## Prerequisites

- **ESP-IDF v5.5** -- [installation guide](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/get-started/)
- Target: `esp32s3`

## USB Ports

This board has a single USB-C connector that is shared between two USB peripherals:

| Port | Controller | Purpose | When available |
|---|---|---|---|
| **COM4** (typical) | USB-Serial-JTAG | Flashing and early boot console | Download mode only (BOOT + RESET) |
| **COM6** (typical) | USB-OTG via TinyUSB CDC | Runtime log output | After firmware boots |

The COM numbers are assigned dynamically by Windows and may differ on your machine -- check Device Manager under `Ports (COM & LPT)`.

At runtime, TinyUSB owns the USB-OTG peripheral and the USB-Serial-JTAG port disappears. To flash new firmware, you must enter download mode first (hold **BOOT**, press **RESET**, release **BOOT**).

## Build & Flash

### Quick (using flash.ps1)

```powershell
.\flash.ps1                # build + flash (COM4 default)
.\flash.ps1 -Port COM5     # different port
.\flash.ps1 -BuildOnly     # build without flashing
.\flash.ps1 -FlashOnly     # flash without rebuilding
.\flash.ps1 -Clean          # full clean build + flash
```

### Manual

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COM4 flash          # replace COM4 with your flash port
```

Dependencies (`waveshare/esp32_s3_touch_amoled_1_75`, `lvgl/lvgl 9.4.*`, `espressif/esp_tinyusb 2.1.1`) are fetched automatically by the IDF Component Manager on first build.

## Expected USB Behavior

- Windows should enumerate the board as a USB microphone input device, a USB HID keyboard, a USB mass-storage drive, and a virtual COM port (CDC ACM serial)
- The refreshed Windows-facing identity is `VID_303A` / `PID_4214`
- The recording endpoint should appear as `Microphone` / `PTT Smart Mic Microphone` in Windows
- The CDC serial port appears under `Ports (COM & LPT)` in Device Manager as `WalKEY-TalKEY Serial`
- The USB microphone remains present even while idle
- BOOT gates microphone audio content instead of connect/disconnect behavior

## Monitoring Logs

Firmware logs are redirected to the CDC ACM virtual serial port (see the **USB Ports** table above).

1. Plug in the USB-C cable and wait for boot to complete
2. Find the CDC COM port in Device Manager under `Ports (COM & LPT)` -- it shows as `USB Serial Device (COMx)`
3. Connect with any serial terminal at **115200 baud**:

```powershell
$port = New-Object System.IO.Ports.SerialPort COM6,115200
$port.DtrEnable = $true; $port.Open()
while($true) { if($port.BytesToRead) { Write-Host $port.ReadExisting() -NoNewline }; Start-Sleep -Milliseconds 100 }
```

Or use PuTTY, Tera Term, or the VS Code Serial Monitor extension.

4. All `ESP_LOGx` output appears when device activity generates log messages (touch, button press, voice, etc.)

Early boot logs (before TinyUSB initializes) are not captured.

## Expected HID Behavior

- BOOT press enters temporary mode-selection state and should not send `F13`
- BOOT release confirms the current mode and exits mode-selection state
- In `Cursor` mode, a 400+ ms touchscreen hold sends `F13` down and release sends `F13` up
- In `Cursor` mode, tap sends `F14`
- In `Cursor` mode, double tap sends `Enter`
- In `Cursor` mode, swipe up sends `Ctrl+A` then `Backspace`, swipe down sends `Ctrl+.`, swipe left sends `Ctrl+N`, and swipe right sends `Enter`
- Swipe gestures should execute their mapped action once per gesture
- In swipe-driven modes, left/right swipes map to mode-specific keyboard-safe actions from `main/mode_config.c`
- If USB is not mounted or not ready, the UI still updates and the serial log explains why HID was skipped

## Expected Microphone Behavior

- The USB microphone enumerates continuously as a normal Windows input device
- Microphone transport follows a TinyUSB-style 48 kHz / 16-bit / mono full-speed profile
- When BOOT is held, live mic frames are sent over USB Audio Class
- When BOOT is released, the firmware still services the audio stream but sends silence
- Serial logs should show USB attach/detach and microphone streaming start/stop events

## Partition Table

Custom layout in `partitions.csv` -- 8 MB factory app, 5 MB model SPIFFS, 2 MB config/docs SPIFFS:

| Name | Type | Size |
|---|---|---|
| nvs | data | 24 KB |
| phy_init | data | 4 KB |
| factory | app | 8 MB |
| model | data (spiffs) | 5 MB |
| storage | data (spiffs) | 2 MB |

The runtime mode JSON file lives at `/spiffs/mode-config.json`. A repo copy is provided at `config/mode-config.json`.

### Wi-Fi Config Portal

The firmware exposes a local config portal over Wi-Fi:

- It first tries the router credentials stored in the JSON config and advertises `http://walkey-talkey.local/`
- If router join succeeds, browse to `walkey-talkey.local` or the IP shown on the BOOT overlay
- If router join fails, it falls back to a device-hosted access point
- Fallback SSID: `walkey-talkey`
- Fallback password: `secretKEY`
- Fallback URL: `http://192.168.4.1/`
- The portal serves a small web UI and REST endpoints for `GET /config`, `POST /config/validate`, `PUT /config`, and `POST /config/reset`
- The portal also offers direct documentation downloads for `mode-config.schema.json`, `AI_GUIDE.md`, and `USER_GUIDE.md`
- The portal intentionally comes up after a short startup delay of about 8 seconds
- The BOOT overlay shows `Connecting...` immediately during that startup delay, then switches to the active hostname, IP, or AP label when Wi-Fi is ready
- `Save` and `Reset` both reapply the Wi-Fi config immediately, so a manual reboot is no longer required after changing network settings
- Reset writes the built-in firmware JSON back to the external config file, then reloads the runtime from that restored config
- The hardcoded failsafe config remains an internal safety net if both external and built-in JSON loading fail
- If `Save` or `Reset` fails, the portal returns a detailed `STORAGE_FAILED` payload that explains whether the failure happened while mounting SPIFFS or writing `/spiffs/mode-config.json`, including `stage`, `formatAttempted`, `path`, `partition`, `espError`, `errnoValue`, `errnoMessage`, and suggested recovery steps

### Portal/SR Coexistence Notes

- The main limiter is internal runtime RAM and `largest_internal`, not the 16 MB flash size
- Large portal responses such as `GET /config` and `GET /portal` were restored by keeping chunked/streamed sends, preferring PSRAM for temporary buffers, and enabling `CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY`
- Avoid reverting the PSRAM/BSS settings without re-testing `GET /config` and `GET /portal`, especially if SR or USB audio is active

## JSON Macro Model

The JSON macro model is intentionally declarative:

- Each binding is `input` + `trigger` + ordered `actions`
- The `actions` array is the macro
- Tap actions already include the firmware's built-in tap gap
- Use `sleep_ms` only for extra delay between macro steps
- Prefer `hid_shortcut_tap` with `modifiers` plus `key` for keyboard chords and `hid_usage_*` for media/system HID

For the full JSON authoring reference, see the [User Guide](USER_GUIDE.md).

## Project Structure

```
├── CMakeLists.txt          # Top-level project CMake
├── flash.ps1               # Build & flash helper script (PowerShell)
├── partitions.csv          # Custom partition table
├── sdkconfig.defaults      # Default Kconfig (PSRAM, LVGL core settings)
└── main/
    ├── CMakeLists.txt      # Component CMake
    ├── README.md           # Notes for the main app modules
    ├── action_engine.c     # Executes declarative mode actions
    ├── action_engine.h
    ├── audio_input.c       # ES7210 + I2S microphone capture wrapper
    ├── audio_input.h
    ├── boot_button.c       # GPIO0 polling and debounce
    ├── boot_button.h
    ├── input_router.c      # Normalizes raw BOOT/touch events into triggers
    ├── input_router.h
    ├── mode_config.c       # Hybrid JSON/fallback mode config entry point
    ├── mode_config.h
    ├── mode_json_loader.c  # JSON-to-runtime config compiler
    ├── mode_json_loader.h
    ├── mode_controller.c   # Active mode and temporary boot-mode control
    ├── mode_controller.h
    ├── mode_system.Readme.md
    ├── mode_types.h
    ├── ptt_state.c         # Small host-testable PTT transition state machine
    ├── ptt_state.h
    ├── usb_cdc_log.c       # CDC ACM virtual serial port log redirect
    ├── usb_cdc_log.h
    ├── usb_composite.c     # Composite USB HID + microphone + MSC + CDC transport
    ├── usb_composite.h
    ├── idf_component.yml   # IDF Component Manager dependencies
    ├── component.mk        # Legacy Make support
    ├── main.c              # App orchestration and queued mode/input/event handling
    ├── ui_status.c         # Current mode UI, BOOT overlay, and touch/swipe feedback
    └── ui_status.h
```

## Key Configuration (sdkconfig.defaults)

- Octal PSRAM with XIP enabled
- 32 KB instruction cache / 64 KB data cache (64-byte lines)
- LVGL refresh period: 15 ms
- 2 SW draw units for parallel rendering
- IRAM-placed fast-mem attributes for LVGL
- FreeRTOS tick rate: 1000 Hz
- LVGL demo features disabled for this custom UI app
- TinyUSB HID interface count set to 1
- TinyUSB Audio Class is enabled through project-wide compile definitions so the project does not rely on edited `managed_components`
- TinyUSB CDC ACM is enabled (`CFG_TUD_CDC=1`) for a virtual serial port that carries ESP-IDF log output
- USB audio sizing matches a Windows-friendlier TinyUSB microphone profile
- Console output is set to `none` (`CONFIG_ESP_CONSOLE_NONE=y`) because USB-Serial-JTAG is unavailable while TinyUSB owns the USB peripheral

## Manual Validation

### Firmware-Side

- Build with `idf.py build` or `.\flash.ps1 -BuildOnly`
- Flash and confirm the default screen shows the centered active mode heading with no fallback `Cursor mode` placeholder text
- Confirm the in-card hint/status area is blank until populated by mode activity
- Verify there is no separate `Touch Controller` title row
- Press and hold the touchscreen briefly without swiping and confirm the touch-down palette shifts to dark red while pressed
- Perform touch gestures and confirm the in-card debug label shows text like `PRESS`, `TAP`, `DOUBLE TAP`, `LONG PRESS`, `HOLD END`, and swipe directions
- Confirm the BOOT-position marker is visible near the physical BOOT button, uses the green accent at idle, and turns red while `BOOT` is held
- Press and hold `BOOT` to confirm the BOOT selector appears with `Swipe to switch mode`, the active network address on the next line once Wi-Fi is ready, `Release BOOT = Confirm`, and the centered active mode label still visible
- While holding `BOOT`, swipe left or right and confirm the selected mode changes
- Release `BOOT` and confirm the newly selected mode remains active
- Watch serial logs for USB init, BOOT press/release, touch events, HID send messages, and microphone streaming start/stop

### Host-Side

- Connect the board to the USB-OTG-capable port used for device mode
- Confirm Device Manager shows a USB keyboard / HID entry and Windows Sound settings show `PTT Smart Mic Microphone`
- Verify BOOT mode changes do not emit an `F13` key event by themselves
- Verify in `Cursor` mode that a 400+ ms stationary press sends `F13` down and release sends `F13` up
- Verify in `Cursor` mode that tap sends `F14`
- Verify in `Cursor` mode that double tap sends `Enter`
- Verify in `Cursor` mode that swipe up sends `Ctrl+A` then `Backspace`
- Verify in `Cursor` mode that swipe down sends `Ctrl+.`
- Verify in `Cursor` mode that swipe left sends `Ctrl+N`
- Verify in `Cursor` mode that swipe right sends `Enter`
- Verify a short tap only performs the active mode's mapped tap behavior
- Open Windows `Sound settings` or `mmsys.cpl` recording devices and confirm the mic meter stays quiet when idle
- Hold touch in `Cursor` mode and speak into the board to confirm the recording meter reacts only while dictation is active

## AI Context

- Board BSP provided by `waveshare/esp32_s3_touch_amoled_1_75` component (display init, touch, backlight).
- The onboard microphone path uses the BSP audio layer plus `esp_codec_dev`.
- The app uses the BSP default display/touch orientation.
- `sdkconfig` is git-ignored; `sdkconfig.defaults` is the source of truth for configuration.
- Mode-system behavior is split across `main/main.c`, `main/mode_config.c`, `main/mode_json_loader.c`, `main/mode_controller.c`, `main/input_router.c`, `main/action_engine.c`, and `main/ui_status.c`.
- Dictation-specific behavior is still supported through `Cursor` mode plus `main/ptt_state.c`, `main/audio_input.c`, and `main/usb_composite.c`.
- `ptt_state.c` keeps PTT transitions deterministic and host-testable without BSP, LVGL, or TinyUSB dependencies.
- `usb_composite.c` owns the composite TinyUSB descriptors and callbacks for keyboard HID, USB microphone streaming, MSC storage, and CDC ACM serial, including key report state for `F13` and any future extra keys.
- `usb_cdc_log.c` redirects `ESP_LOGx` output to the CDC ACM virtual serial port via `esp_log_set_vprintf()`.
- `ui_status.c` owns touch gesture detection, the BOOT overlay, the in-card gesture debug label, the BOOT-position marker, and reports high-level touch events back to `main.c`.
