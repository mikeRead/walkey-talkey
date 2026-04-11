# Flashing firmware to the ESP32-S3 device

## Build and flash script

Use `flash.ps1` in the project root. Do NOT call `idf.py` directly — the script handles ESP-IDF environment setup automatically.

```powershell
# Build and flash (default port COM4)
.\flash.ps1

# Flash only (skip build)
.\flash.ps1 -FlashOnly

# Build only (no flash)
.\flash.ps1 -BuildOnly

# Clean build + flash
.\flash.ps1 -Clean

# Use a different COM port
.\flash.ps1 -Port COM5
```

## Manual steps required by the user

This device does NOT support automatic boot-mode entry or automatic reset. The user must physically interact with the device for both steps:

1. **Before flashing**: the user must put the device into flash/boot mode manually (hold BOOT, press RESET, release BOOT).
2. **After flashing**: the user must manually reset the device (press RESET) or power-cycle it to exit boot mode and run the new firmware.

Always remind the user of both steps when flashing. Do not assume the device will reboot on its own.

## Glob pattern

glob: flash.ps1
