# sd_card_config

## Human Reference

Boot-time helper that populates the SD card with default files and reads config JSON from it.

Called once during `app_main` while the SD card FAT filesystem is mounted. The FAT filesystem remains mounted for both config access and audio recording. USB MSC shares the SD card concurrently.

Files written on first boot (only if missing):

| File on SD card            | Source                            |
|----------------------------|-----------------------------------|
| `mode-config.json`         | Built-in JSON string from firmware |
| `mode-config.schema.json`  | Embedded binary (`EMBED_TXTFILES`) |
| `AI_GUIDE.md`              | Embedded binary (`EMBED_TXTFILES`) |
| `USER_GUIDE.md`            | Embedded binary (`EMBED_TXTFILES`) |

## AI Reference

- `sd_card_config_populate()` is idempotent; it only writes files that do not already exist.
- `sd_card_config_read()` returns a heap-allocated string that the caller must free.
- Both functions require the SD card FAT filesystem to be mounted at `/sdcard/`.
- The embedded binary symbols (`_binary_*_start` / `_binary_*_end`) match the `EMBED_TXTFILES` entries in `main/CMakeLists.txt`.
- Config source priority: SD card > SPIFFS external > builtin > failsafe (see `mode_config.c`).
