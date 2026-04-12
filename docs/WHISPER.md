# Whisper Transcription

The WalKEY-TalKEY dashboard includes browser-based speech-to-text powered by
[Hugging Face Transformers.js](https://huggingface.co/docs/transformers.js) running
ONNX Whisper models entirely on the client — no cloud API required.

## How It Works

1. **Auto-transcription** — new recordings are detected by polling
   `GET /api/recordings` every 10 s. A file is considered ready once its size
   is stable across two consecutive polls (avoids transcribing incomplete WAVs
   that are still being written).
2. **Chunked download** — the WAV file is downloaded from the ESP32 using
   sequential **32 KB byte-range requests** (`Range: bytes=N-M`). Each chunk
   retries up to 3 times on failure. This works around the ESP32 httpd randomly
   truncating large responses.
3. **Resampling** — the browser's `AudioContext` decodes the WAV and resamples to
   **16 kHz mono Float32**, which is what Whisper expects.
4. **Silence gate** — an RMS energy check rejects audio below a threshold
   (`0.001`) before the model is loaded, saving time on truly silent clips.
5. **Inference** — a Web Worker loads the selected ONNX model and runs
   `automatic-speech-recognition` with chunked decoding and timestamps.
6. **Hallucination filter** — the output text is compared against a list of
   known hallucination phrases (see below). Matches are replaced with
   *(No speech detected)*.
7. **Persistence** — completed transcripts are saved to `localStorage` keyed
   by file name, so they survive page reloads. Auto-transcription skips files
   that already have a saved transcript; clicking "Transcribe" always
   re-transcribes.

```
┌──────────┐  32 KB Range   ┌──────────┐  decode/resample   ┌────────────┐
│  ESP32   │  requests ×N   │ Browser  │ ──────────────────► │ Web Worker │
│ SD Card  │ ◄────────────► │  fetch   │   Float32 @ 16kHz  │  Whisper   │
└──────────┘                └──────────┘                     └────────────┘
```

## Audio Playback

The play button also uses the chunked download to get the full WAV file. On
first click, the entire file is fetched via 32 KB range requests, assembled
into a Blob URL, and played through an `<audio>` element. The blob is cached
so subsequent plays are instant.

A circular SVG progress ring fills counter-clockwise in green while playing,
and a short time label (e.g. `9s`, `1M`) counts down the remaining duration.

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
| RMS gate | `transcription-store.tsx` | Skips transcription if RMS < 0.001 |
| Phrase filter | `whisper-worker.ts` | Rejects exact matches against known hallucinations |

**If it still happens:** Lower the RMS threshold or add the new phrase to the
`HALLUCINATION_PHRASES` set in `whisper-worker.ts`.

### 2. Truncated audio (ESP32 httpd limitation)

**Problem:** The ESP32 HTTP server randomly truncates large WAV file responses.
A plain `fetch()` call may receive only a fraction of the file (e.g. 3 s out
of 10 s), and the amount varies between requests with no pattern.

**Mitigation:** Both playback and transcription use `downloadFile()` which
fetches the file in **32 KB byte-range chunks**, each with up to 3 retries.
The ESP32 handles small range requests reliably, so assembling the full file
from many small requests is consistently successful.

**If audio is still truncated:**

- Check that the expected file size from `GET /api/recordings` matches what
  was downloaded (logged to console).
- Very large files (> 2 MB) may need more time. The download is sequential,
  so ~32 requests per MB at ~100 ms round-trip each.
- Reduce concurrent load on the ESP32 (e.g. close other browser tabs hitting
  the device).

### 3. Incomplete WAV files during recording

**Problem:** Recordings appear in the file listing immediately when recording
starts, while the WAV is still being written. Transcribing an incomplete file
produces errors or partial results.

**Mitigation:** Auto-transcription tracks each file's size across polls. A file
is only queued for transcription once its size is unchanged between two
consecutive 10 s polls, indicating the recording has finished writing.

### 4. Slow transcription

- **Tiny** takes ~5–15 s for a 10 s clip on a modern laptop.
- **Small** can take 30–90 s for the same clip.
- Models are downloaded on first use (~75–460 MB). Subsequent runs use the
  browser cache.
- Transcription runs in a Web Worker and does not block the UI.

### 5. Short or partial transcription

**Problem:** Whisper Tiny may only capture part of the speech, especially for
longer recordings or quiet voices.

**Fix:** Use **Whisper Base** or **Whisper Small** for better accuracy. The
audio is cached locally, so switching models is instant (no re-download from
the device).

## File Locations

| File | Purpose |
|---|---|
| `server/src/lib/transcription-store.tsx` | TranscriptionProvider: auto-transcription, queue, polling, localStorage persistence |
| `server/src/lib/api.ts` | `downloadFile()`: chunked Range-request downloader |
| `server/src/lib/whisper-worker.ts` | Web Worker: model loading, inference, hallucination filter |
| `server/src/components/play-button.tsx` | Play button with SVG progress ring and chunked audio download |
| `server/src/components/recording-list.tsx` | Recording list UI with inline transcription status |
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

In `server/src/lib/transcription-store.tsx`:

```typescript
const RMS_SILENCE_THRESHOLD = 0.001; // lower = less aggressive filtering
```

Check the Logs page after transcription to see the actual RMS value for your
recordings and tune accordingly.
