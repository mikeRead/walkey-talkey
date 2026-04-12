# device_log — Structured Device Logging

## Human Reference

Lightweight logging module that stores the last 25 log entries in a RAM circular buffer and optionally persists them to `/sdcard/device.log`.

### Usage

```c
#include "device_log.h"

device_log_init();                       // Call once at boot (clears SD file)
device_log("BOOT", "System started");    // Add a log entry
device_log("INFO", "IP: %s", ip_str);   // printf-style formatting

char buf[4096];
device_log_get_json(buf, sizeof(buf));   // Serialize to JSON for HTTP
```

### Entry Fields

| Field     | Type     | Description                          |
|-----------|----------|--------------------------------------|
| type      | string   | Category: BOOT, INFO, WARN, ERROR, CONFIG, ACTION |
| message   | string   | Up to 128 chars, printf-formatted    |
| runtime   | uint32   | Milliseconds since boot              |

### Behavior

- `device_log_init()` creates the mutex and truncates the SD log file (fresh start each boot).
- `device_log()` is thread-safe (FreeRTOS mutex). It writes to both the RAM buffer and the SD file.
- `device_log_get_json()` snapshots the buffer under the mutex and serializes to `{"logs":[...]}`.
- If the SD card is not present, file writes are silently skipped; RAM buffer still works.

---

## AI Reference

- **Files**: `main/device_log.h`, `main/device_log.c`
- **Circular buffer**: Fixed array of 25 `device_log_entry_t`, managed by `s_head`/`s_count`.
- **Thread safety**: `s_mutex` (FreeRTOS mutex) guards all buffer access with 100-200ms timeouts.
- **SD file**: `/sdcard/device.log`, opened with `fopen("a")` per entry. Truncated at init.
- **JSON output**: Hand-built (no cJSON dependency) with `json_escape()` for safe string encoding.
- **Timestamp**: `esp_timer_get_time() / 1000` gives ms since boot.
- **Registered as HTTP endpoint**: `GET /api/logs` in `config_http_server.c`.
- **Build**: Listed in `main/CMakeLists.txt` SRCS.
