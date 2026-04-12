# WalKEY-TalKEY Dashboard

A web dashboard for configuring the WalKEY-TalKEY ESP32-S3 macro controller. Built with Next.js, deployed as a static site to Cloudflare Pages.

## Features

- **Config Editor** — Structured UI for editing modes, bindings, actions, and defaults (no raw JSON required)
- **Recordings** — Browse, play, download, and transcribe SD card audio recordings
- **Whisper Transcription** — In-browser speech-to-text using Hugging Face Transformers.js (runs entirely client-side)
- **Documentation** — Rendered markdown docs with branded styling
- **Advanced Editor** — Monaco JSON editor for full raw config editing with validation

## Prerequisites

- The ESP32 device must be on the same network as your browser
- The device firmware must include CORS headers (`Access-Control-Allow-Origin: *`)

## Quick Start

```bash
cd server
npm install
npm run dev
```

Open [http://localhost:3000](http://localhost:3000) and enter your device URL (default: `http://walkey-talkey.local`).

## Build & Deploy

### Static Export

```bash
npm run build     # outputs to out/
```

### Cloudflare Pages

```bash
npx wrangler pages deploy out --project-name walkey-talkey-dashboard
```

Or connect your repo to Cloudflare Pages with build command `npm run build` and output directory `out`.

## Architecture

- **Fully static** — No server runtime; all API calls go directly from browser to ESP32
- **Device URL** stored in localStorage, configurable in the header
- **Whisper** runs in a Web Worker using `@huggingface/transformers`

## Tech Stack

| Library | Purpose |
|---|---|
| Next.js 16 | Framework (static export) |
| Tailwind CSS 4 | Styling |
| Monaco Editor | Raw JSON editing |
| @huggingface/transformers | In-browser Whisper ASR |
| react-markdown + remark-gfm | Documentation rendering |
| lucide-react | Icons |

## AI Context

- The dashboard talks to the ESP32 REST API documented in `docs/REST_API.md`
- All API helpers are in `src/lib/api.ts` — mirrors the MCP server endpoints in `mcp/index.js`
- Types in `src/types/config.ts` match the `config/mode-config.json` schema
- The Whisper worker in `src/lib/whisper-worker.ts` follows the same pattern as [xenova/whisper-web](https://github.com/xenova/whisper-web)
- Branding follows the 90s toy-box aesthetic from the WalKEY-TalKEY logo
