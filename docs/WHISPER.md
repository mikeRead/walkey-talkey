# Whisper Transcription

The WalKEY-TalKEY dashboard includes browser-based speech-to-text powered by
[Hugging Face Transformers.js](https://huggingface.co/docs/transformers.js) running
ONNX Whisper models entirely on the client — no cloud API required.

## How It Works

1. **Audio fetch** — the WAV file is downloaded from the ESP32 via
   `GET /api/recordings/download?file=…` using an HTTP `Range: bytes=0-` request.
2. **Resampling** — the browser's `AudioContext` decodes the WAV and resamples to
   **16 kHz mono Float32**, which is what Whisper expects.
3. **Caching** — the decoded samples are cached in memory so switching models
   doesn't re-fetch from the device.
4. **Silence gate** — an RMS energy check rejects audio below a threshold
   (`0.001`) before the model is loaded, saving time on truly silent clips.
5. **Trailing trim** — the last 1 600 samples (~0.1 s) are zeroed to suppress
   a known decoder hallucination trigger.
6. **Inference** — a Web Worker loads the selected ONNX model and runs
   `automatic-speech-recognition` with chunked decoding and timestamps.
7. **Hallucination filter** — the output text is compared against a list of
   known hallucination phrases (see below). Matches are replaced with
   *(No speech detected)*.

```
┌──────────┐   Range req    ┌──────────┐   decode/resample   ┌────────────┐
│  ESP32   │ ─────────────► │ Browser  │ ──────────────────► │ Web Worker │
│ SD Card  │   206 + data   │  fetch   │   Float32 @ 16kHz  │  Whisper   │
└──────────┘                └──────────┘                     └────────────┘
```

## Available Models

| Model | ID | Size | Speed | Quality |
|---|---|---|---|---|
| Tiny | `onnx-community/whisper-tiny` | ~75 MB | Fastest | Low — good for quick checks |
| Base | `onnx-community/whisper-base` | ~140 MB | Medium | Medium |
| Small | `onnx-community/whisper-small` | ~460 MB | Slowest | Best accuracy |

Models are downloaded on first use and cached by the browser.

## Known Issues & Mitigations

### 1. Hallucinated output on silent audio

**Problem:** Whisper was trained on YouTube videos with subtitles. When it
receives silence or very quiet audio, it hallucinates common phrases like
*"you"*, *"thank you"*, *"thank you for watching"*, or *"bye"*.

- **HuggingFace issue:** [transformers#24512](https://github.com/huggingface/transformers/issues/24512)

**Mitigations in place:**

| Layer | Where | What |
|---|---|---|
| RMS gate | `whisper-panel.tsx` | Skips transcription if RMS < 0.001 |
| Trailing mute | `whisper-worker.ts` | Zeros last 1 600 samples before inference |
| Phrase filter | `whisper-worker.ts` | Rejects exact matches against known hallucinations |

**If it still happens:** Lower the RMS threshold or add the new phrase to the
`HALLUCINATION_PHRASES` set in `whisper-worker.ts`.

### 2. Truncated audio (ESP32 chunked encoding)

**Problem:** The ESP32 HTTP server streams WAV files using chunked
`Transfer-Encoding` for non-Range requests. Over Wi-Fi, chunks are sometimes
lost, causing the browser to receive only a fraction of the file (e.g. 0.7 s
out of 9 s).

**Why it matters:** The `<audio>` element works fine because browsers
automatically use `Range` requests with proper buffering. But a plain
`fetch()` call gets the unreliable chunked path.

**Mitigation:** The Whisper fetch includes `Range: bytes=0-` in the request
headers, which forces the ESP32 to load the full file into PSRAM and send it
as a single response with `Content-Length`. This matches the reliable path the
`<audio>` element uses.

**If audio is still truncated:**

- Check the Logs page for the `Fetched … bytes (206)` log entry and compare
  against expected file size.
- Very large files (> 2 MB) may exceed PSRAM allocation. Consider shorter
  recordings or increasing PSRAM budget in `sdkconfig`.

### 3. Slow transcription

- **Tiny** takes ~5–15 s for a 10 s clip on a modern laptop.
- **Small** can take 30–90 s for the same clip.
- Models are downloaded on first use (~75–460 MB). Subsequent runs use the
  browser cache.
- Transcription runs in a Web Worker and does not block the UI.

### 4. Short or partial transcription

**Problem:** Whisper Tiny may only capture part of the speech, especially for
longer recordings or quiet voices.

**Fix:** Use **Whisper Base** or **Whisper Small** for better accuracy. The
audio is cached locally, so switching models is instant (no re-download from
the device).

## File Locations

| File | Purpose |
|---|---|
| `server/src/components/whisper-panel.tsx` | UI, audio fetch, RMS gate, caching |
| `server/src/lib/whisper-worker.ts` | Web Worker: model loading, inference, hallucination filter |
| `main/config_http_server.c` | ESP32 download handler with Range support |

## Adding a New Hallucination Phrase

Edit `HALLUCINATION_PHRASES` in `server/src/lib/whisper-worker.ts`:

```typescript
const HALLUCINATION_PHRASES = new Set([
  "you", "thank you", "thanks",
  // add new phrases here (lowercase, trimmed)
  "new phrase",
]);
```

## Adjusting the Silence Threshold

In `server/src/components/whisper-panel.tsx`:

```typescript
const RMS_SILENCE_THRESHOLD = 0.001; // lower = less aggressive filtering
```

Check the Logs page after transcription to see the actual RMS value for your
recordings and tune accordingly.
