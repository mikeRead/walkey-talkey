# audio_recorder

## Human Info

SD card audio recording module. When recording is active, PCM audio from the microphone is written to WAV files on the SD card. Recordings are accessed via HTTP endpoints, not USB MSC.

### Files

- `audio_recorder.h` -- public API
- `audio_recorder.c` -- implementation

### Behavior

- Call `audio_recorder_init()` once at startup to generate a per-boot session ID
- Call `audio_recorder_start(mode_name)` to begin recording; creates `/sdcard/recordings/<mode_name>/<sessionId>_<uptimeSec>.wav`
- Call `audio_recorder_feed(pcm, len)` from the USB audio task to push PCM frames into the ring buffer
- Call `audio_recorder_stop()` to flush, fix the WAV header, and close the file
- Recordings are served via `GET /api/recordings` (list), `GET /api/recordings/download` (stream), and `GET /api/recordings/delete` (remove)
- A web UI is available at `GET /recordings` for browsing, playing, downloading, and deleting recordings

### Audio Format

48000 Hz, 16-bit, mono PCM in a standard RIFF/WAV container.

### Architecture

A FreeRTOS StreamBuffer (16 KB, allocated in PSRAM) decouples the real-time USB audio task from SD card writes. A dedicated writer task (`rec_wr`, priority 4) drains the buffer and calls `fwrite()`.

---

## AI Info

### Key constants

- Ring buffer: 16 KB in PSRAM (~170 ms at 48kHz/16-bit/mono)
- Writer task: stack 4096, priority 4
- WAV header: 44 bytes, patched on stop with final data size
- Session ID: `esp_random()` called once in `init()`
- Filename: `snprintf("/sdcard/recordings/%s/%08lX_%05lu.wav", mode_name, session_id, uptime_sec)`

### Integration points

- `main.c` calls `audio_recorder_init()` after `sd_card_init()`, and `audio_recorder_start/stop()` from `app_set_mic_gate()`
- `usb_composite.c` calls `audio_recorder_feed()` in the audio task loop when `ptt_audio_active` is true
- `config_http_server.c` serves recordings via REST API (`/api/recordings`, `/api/recordings/download`, `/api/recordings/delete`) and the `/recordings` web UI

### Config

Controlled by `mode_config_t.recording.enabled` (global) and `mode_mic_gate_data_t.recording_override` (per-action). Resolution: action override wins if set, else global default. `mic_gate enabled=false` always stops recording.
