# REST API Reference

WalKEY-TalKEY exposes a local HTTP REST API over Wi-Fi. All endpoints are served from the device itself -- no internet or cloud service required.

**Base URL:** `http://walkey-talkey.local` (or `http://192.168.4.1` when connected to the device's fallback AP)

All request and response bodies are JSON (`application/json`) unless otherwise noted. The API uses chunked transfer encoding for large responses.

---

## Quick Reference

| Method | Endpoint | Description |
|---|---|---|
| GET | `/ping` | Health check |
| GET | `/` | Web UI (HTML) |
| GET | `/portal` | Web UI (HTML, alias) |
| GET | `/config` | Read full active config |
| GET | `/config/canonical` | Read full config in canonical format |
| PUT | `/config` | Replace full config |
| POST | `/config/validate` | Validate JSON without saving |
| POST | `/config/reset` | Reset to built-in firmware defaults |
| GET | `/api/modes` | List all modes (summary) |
| GET | `/api/mode?id=X` | Read a single mode |
| POST | `/api/mode` | Create a new mode |
| PUT | `/api/mode?id=X` | Replace a mode |
| DELETE | `/api/mode?id=X` | Delete a mode |
| GET | `/api/wifi` | Read Wi-Fi config |
| PUT | `/api/wifi` | Update Wi-Fi config (merge) |
| GET | `/api/defaults` | Read defaults (touch, mouse) |
| PUT | `/api/defaults` | Update defaults (touch, mouse; merge) |
| GET | `/api/recording` | Read recording config |
| PUT | `/api/recording` | Update recording config (merge) |
| GET | `/api/recordings` | List all recording WAV files |
| GET | `/api/recordings/download?file=...` | Download a recording WAV file |
| GET | `/api/recordings/delete?file=...` | Delete a recording WAV file |
| GET | `/recordings` | Recordings management web UI (HTML) |
| PUT | `/api/active-mode` | Set the active mode |
| GET | `/api/boot-mode` | Read boot-mode definition |
| GET | `/api/global-bindings` | Read global bindings |
| GET | `/downloads/mode-config.schema.json` | Download JSON schema |
| GET | `/downloads/AI_GUIDE.md` | Download AI guide |
| GET | `/downloads/USER_GUIDE.md` | Download user guide |

---

## General Notes

- The device starts the HTTP server about **8 seconds after boot** to let the display and touch stack settle first.
- Responses from config-mutating endpoints (`PUT /config`, `POST /config/reset`, `PUT /api/*`, `POST /api/mode`, `DELETE /api/mode`) automatically save to flash, reload the runtime, and reapply Wi-Fi settings. No manual reboot is needed.
- The maximum request body size is **32 KB**.
- All `PUT` and `POST` endpoints expect a `Content-Type: application/json` body.

---

## Health & UI

### `GET /ping`

Health check. Returns `ok` as plain text.

```
HTTP/1.1 200 OK
Content-Type: text/plain

ok
```

### `GET /` and `GET /portal`

Serve the built-in web UI as HTML. Both paths return identical content.

---

## Full Config Endpoints

These endpoints operate on the entire config file at once.

### `GET /config`

Read the full active config. The response wraps the config JSON inside a metadata envelope.

**Response:**

```json
{
  "ok": true,
  "source": "external",
  "config": {
    "version": 1,
    "activeMode": "cursor",
    "defaults": { ... },
    "wifi": { ... },
    "globalBindings": [ ... ],
    "bootMode": { ... },
    "modes": [ ... ]
  }
}
```

The `source` field indicates where the config was loaded from: `"external"` (SPIFFS file), `"builtin"` (compiled into firmware), or `"failsafe"` (hardcoded last resort).

### `GET /config/canonical`

Same as `GET /config` but returns the config in canonical (normalized) JSON format. Includes an additional `"format": "canonical"` field.

**Response:**

```json
{
  "ok": true,
  "source": "external",
  "format": "canonical",
  "config": { ... }
}
```

### `PUT /config`

Replace the entire config. Validates, saves to SPIFFS, reloads the runtime, and reapplies Wi-Fi.

**Request body:** Full config JSON object (same shape as what `GET /config` returns in its `config` field).

**Success response:**

```json
{
  "ok": true,
  "saved": true,
  "source": "external",
  "config": { ... }
}
```

**Error response (validation failure):**

```json
{
  "ok": false,
  "error": "VALIDATION_FAILED",
  "message": "Description of what went wrong"
}
```

**Error response (storage failure):**

```json
{
  "ok": false,
  "error": "STORAGE_FAILED",
  "stage": "write",
  "formatAttempted": false,
  "path": "/spiffs/mode-config.json",
  "partition": "storage",
  "espError": "...",
  "errnoValue": 28,
  "errnoMessage": "No space left on device",
  "suggestions": ["Reflash the firmware and partition table"]
}
```

### `POST /config/validate`

Validate a JSON config without saving. Useful for checking edits before committing.

**Request body:** Full config JSON object.

**Success response:**

```json
{
  "ok": true,
  "valid": true,
  "config": { ... }
}
```

The returned `config` is the normalized version of what you sent.

**Error response:**

```json
{
  "ok": false,
  "error": "VALIDATION_FAILED",
  "message": "..."
}
```

### `POST /config/reset`

Reset the config to the built-in firmware defaults. Writes the default JSON back to SPIFFS, reloads the runtime, and reapplies Wi-Fi.

**No request body required.**

**Success response:**

```json
{
  "ok": true,
  "source": "external",
  "config": { ... }
}
```

---

## Mode Endpoints

Granular endpoints for working with individual modes.

### `GET /api/modes`

List all modes with summary information.

**Response:**

```json
[
  {
    "id": "cursor",
    "label": "Cursor",
    "cycleOrder": 0,
    "bindingCount": 8
  },
  {
    "id": "media",
    "label": "Media",
    "cycleOrder": 1,
    "bindingCount": 5
  }
]
```

### `GET /api/mode?id=<mode_id>`

Read the full definition of a single mode.

**Query parameter:** `id` (required) -- the mode id string.

**Response:**

```json
{
  "id": "cursor",
  "cycleOrder": 0,
  "label": "Cursor",
  "bindings": [
    {
      "input": "touch",
      "trigger": "tap",
      "actions": [
        { "type": "hid_key_tap", "key": "F14" },
        { "type": "ui_hint", "text": "Cursor mode" }
      ]
    }
  ]
}
```

**Error:** `404` if the mode id does not exist.

### `POST /api/mode`

Create a new mode. The request body is a complete mode JSON object.

**Request body:**

```json
{
  "id": "gaming",
  "cycleOrder": 4,
  "label": "Gaming",
  "bindings": [
    {
      "input": "touch",
      "trigger": "tap",
      "actions": [
        { "type": "hid_key_tap", "key": "SPACE" }
      ]
    }
  ]
}
```

**Success response:**

```json
{ "ok": true, "created": true }
```

### `PUT /api/mode?id=<mode_id>`

Replace an existing mode entirely. The request body is the full updated mode object.

**Query parameter:** `id` (required) -- the mode id to replace.

**Request body:** Same shape as `POST /api/mode`.

**Success response:**

```json
{ "ok": true, "updated": true }
```

**Error:** `404` if the mode id does not exist.

### `DELETE /api/mode?id=<mode_id>`

Delete a mode.

**Query parameter:** `id` (required) -- the mode id to delete.

**Success response:**

```json
{ "ok": true, "deleted": true }
```

**Errors:**
- `400` if trying to delete the currently active mode.
- `404` if the mode id does not exist.

---

## Wi-Fi Endpoints

### `GET /api/wifi`

Read the current Wi-Fi configuration.

**Response:**

```json
{
  "sta": {
    "ssid": "YourNetworkName",
    "password": "YourPassword"
  },
  "ap": {
    "ssid": "walkey-talkey",
    "password": "secretKEY"
  },
  "hostname": "walkey-talkey",
  "localUrl": "walkey-talkey.local"
}
```

### `PUT /api/wifi`

Update Wi-Fi settings. Uses **merge** semantics -- only the fields you send are overwritten; everything else is preserved.

**Request body (example -- update just the STA credentials):**

```json
{
  "sta": {
    "ssid": "NewNetwork",
    "password": "NewPassword"
  }
}
```

**Success response:**

```json
{ "ok": true, "updated": true }
```

Wi-Fi is reapplied immediately after save. If you change the STA credentials, the device will attempt to join the new network.

---

## Defaults Endpoints

### `GET /api/defaults`

Read the current defaults for touch timing, mouse mode selection, and per-backend mouse configuration.

**Response:**

```json
{
  "touch": {
    "holdMs": 400,
    "doubleTapMs": 350,
    "swipeMinDistance": 40
  },
  "defaultMouse": "airMouse",
  "airMouse": {
    "sensitivity": 1.0,
    "deadZoneDps": 6.0,
    "easingExponent": 1.25,
    "maxDps": 300.0,
    "emaAlpha": 0.35,
    "rewindDepth": 12,
    "rewindDecay": 0.7,
    "calibrationSamples": 128
  },
  "touchMouse": {
    "sensitivity": 1.0,
    "moveThresholdPx": 5,
    "tapDragWindowMs": 180
  }
}
```

### `PUT /api/defaults`

Update defaults. Uses **merge** semantics -- only the fields you send are overwritten; everything else is preserved. Works for `touch`, `defaultMouse`, `airMouse`, and `touchMouse` fields.

**Request body (example -- adjust hold threshold):**

```json
{
  "touch": {
    "holdMs": 500
  }
}
```

**Request body (example -- switch to touch mouse and tweak air sensitivity):**

```json
{
  "defaultMouse": "touchMouse",
  "airMouse": {
    "sensitivity": 1.5
  }
}
```

**Success response:**

```json
{ "ok": true, "updated": true }
```

---

## Recording Endpoints

### `GET /api/recording`

Read the current SD card recording settings.

**Response:**

```json
{
  "enabled": false,
  "format": "wav"
}
```

When `enabled` is `true`, every `mic_gate` activation also writes a WAV file (48kHz, 16-bit, mono) to the SD card at `/sdcard/recordings/<modeId>/<sessionId>_<uptimeSec>.wav`. Recordings are accessed over HTTP, not via USB drive browsing.

### `PUT /api/recording`

Update recording settings. Uses **merge** semantics -- only the fields you send are overwritten.

**Request body (enable recording):**

```json
{
  "enabled": true
}
```

**Request body (disable recording):**

```json
{
  "enabled": false
}
```

**Success response:**

```json
{ "ok": true, "updated": true }
```

**curl example:**

```bash
curl -X PUT http://walkey-talkey.local/api/recording \
  -H "Content-Type: application/json" \
  -d '{"enabled": true}'
```

---

## Recording File Endpoints

Recordings are served over Wi-Fi via these endpoints. They are **not** accessed via USB Mass Storage.

### `GET /api/recordings`

List all recording WAV files on the SD card.

**Response:**

```json
{
  "recordings": [
    { "path": "whisper/ABC123_00042.wav", "size": 344300 },
    { "path": "whisper/ABC123_00058.wav", "size": 750380 }
  ]
}
```

### `GET /api/recordings/download?file=<path>`

Stream a recording WAV file. The `file` parameter is the relative path from the listing.

**Example:**

```bash
curl -o recording.wav "http://walkey-talkey.local/api/recordings/download?file=whisper/ABC123_00042.wav"
```

**Response:** Binary WAV data with `Content-Type: audio/wav`, streamed in chunks.

### `GET /api/recordings/delete?file=<path>`

Delete a recording from the SD card.

**Example:**

```bash
curl "http://walkey-talkey.local/api/recordings/delete?file=whisper/ABC123_00042.wav"
```

**Response:**

```json
{ "ok": true }
```

### `GET /recordings`

Web UI page for browsing, playing, downloading, and deleting recordings. Returns an HTML page.

---

## Active Mode Endpoint

### `PUT /api/active-mode`

Set which mode the device starts in.

**Request body:**

```json
{
  "activeMode": "media"
}
```

**Success response:**

```json
{ "ok": true, "updated": true }
```

---

## Boot Mode & Global Bindings (Read-Only)

### `GET /api/boot-mode`

Read the boot-mode definition (the temporary control layer active while the BOOT button is held).

**Response:**

```json
{
  "label": "Mode Control",
  "ui": {
    "title": "Swipe to switch mode",
    "subtitle": "Hold BOOT and swipe to change modes",
    "showModeList": true,
    "showGestureHints": true,
    "showCurrentModeCard": true
  },
  "bindings": [ ... ]
}
```

### `GET /api/global-bindings`

Read the global bindings (always-active bindings regardless of current mode).

**Response:**

```json
[
  {
    "input": "boot_button",
    "trigger": "press",
    "actions": [
      { "type": "enter_boot_mode" }
    ]
  },
  {
    "input": "boot_button",
    "trigger": "release",
    "actions": [
      { "type": "exit_boot_mode" }
    ]
  }
]
```

---

## Download Endpoints

These serve documentation and schema files embedded in the firmware.

| Endpoint | Content-Type | Description |
|---|---|---|
| `GET /downloads/mode-config.schema.json` | `application/json` | JSON schema for config validation |
| `GET /downloads/AI_GUIDE.md` | `text/markdown` | AI assistant guide for generating configs |
| `GET /downloads/USER_GUIDE.md` | `text/markdown` | End-user setup and authoring guide |

---

## Error Format

All error responses follow this shape:

```json
{
  "ok": false,
  "error": "ERROR_CODE",
  "message": "Human-readable description"
}
```

Storage-specific failures include additional diagnostic fields (`stage`, `formatAttempted`, `path`, `partition`, `espError`, `errnoValue`, `errnoMessage`, `suggestions`).

---

## Example: Create a Mode with curl

```bash
# Create a new "spotify" mode
curl -X POST http://walkey-talkey.local/api/mode \
  -H "Content-Type: application/json" \
  -d '{
    "id": "spotify",
    "cycleOrder": 4,
    "label": "Spotify",
    "bindings": [
      {
        "input": "touch",
        "trigger": "tap",
        "actions": [
          { "type": "hid_usage_tap", "usage": "PLAY_PAUSE" },
          { "type": "ui_hint", "text": "Play/Pause" }
        ]
      },
      {
        "input": "touch",
        "trigger": "swipe_right",
        "actions": [
          { "type": "hid_usage_tap", "usage": "MEDIA_NEXT_TRACK" },
          { "type": "ui_hint", "text": "Next" }
        ]
      },
      {
        "input": "touch",
        "trigger": "swipe_left",
        "actions": [
          { "type": "hid_usage_tap", "usage": "MEDIA_PREV_TRACK" },
          { "type": "ui_hint", "text": "Previous" }
        ]
      },
      {
        "input": "touch",
        "trigger": "swipe_up",
        "actions": [
          { "type": "hid_usage_tap", "usage": "VOLUME_UP" },
          { "type": "ui_hint", "text": "Volume up" }
        ]
      },
      {
        "input": "touch",
        "trigger": "swipe_down",
        "actions": [
          { "type": "hid_usage_tap", "usage": "VOLUME_DOWN" },
          { "type": "ui_hint", "text": "Volume down" }
        ]
      }
    ]
  }'
```

```bash
# Switch to the new mode
curl -X PUT http://walkey-talkey.local/api/active-mode \
  -H "Content-Type: application/json" \
  -d '{"activeMode": "spotify"}'
```

```bash
# Verify it's there
curl http://walkey-talkey.local/api/modes
```
