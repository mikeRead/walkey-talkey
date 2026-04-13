# Deploying to Cloudflare Pages

The WalKEY-TalKEY dashboard is a fully static Next.js site -- no server runtime, no API routes, no database. Every API call goes directly from the browser to your ESP32 device on the local network. This makes Cloudflare Pages a perfect (and free) host.

## Prerequisites

- A [Cloudflare account](https://dash.cloudflare.com/sign-up) (free tier works)
- Node.js 18+ installed locally
- The dashboard source code in the `server/` directory

## Option 1: Deploy via CLI (Wrangler)

The fastest way to get a deploy up.

### 1. Build the static export

```bash
cd server
npm install
npm run build
```

This produces an `out/` directory containing the full static site.

### 2. Deploy with Wrangler

```bash
npx wrangler pages deploy out --project-name walkey-talkey-dashboard
```

On first run, Wrangler will prompt you to log in to your Cloudflare account. After that, your dashboard will be live at:

```
https://walkey-talkey-dashboard.pages.dev
```

To deploy updates, just rebuild and run the same command again.

## Option 2: Connect Your Git Repo (Cloudflare Builds)

For automatic deploys on every push.

### 1. Go to Workers & Pages

Open [Cloudflare Dashboard > Workers & Pages](https://dash.cloudflare.com/?to=/:account/pages) and click **Create** > **Import a repository**.

### 2. Select your repository

Authorize Cloudflare to access your GitHub account and select the `walkey-talkey` repository.

### 3. Configure the build

| Setting | Value |
|---|---|
| **Project name** | `walkey-talkey` |
| **Build command** | `cd server && npm install && npm run build` |
| **Deploy command** | `npx wrangler pages deploy server/out` |
| **Path** | `/` |

If the build fails due to a Node version mismatch, add a variable at the bottom of the form: **Variable name** = `NODE_VERSION`, **Variable value** = `18` (or `20`).

Leave **API token** as "Create new token" -- Cloudflare generates one automatically.

### 4. Deploy

Click **Deploy**. Cloudflare will build and publish the site. Every subsequent push to `main` triggers a new deploy automatically. Non-production branches also get preview deployments if the "Builds for non-production branches" checkbox is enabled.

Your dashboard will be available at:

```
https://walkey-talkey.pages.dev
```

## Custom Domain (Optional)

1. In the Cloudflare Pages project settings, go to **Custom domains**
2. Click **Set up a custom domain**
3. Enter your domain (e.g., `dashboard.walkey-talkey.com`)
4. Cloudflare handles DNS and SSL automatically if the domain is already on Cloudflare

## How It Works

The dashboard is exported as a static site via Next.js `output: "export"` in `next.config.ts`. There is no server-side rendering or API layer on Cloudflare -- the browser talks directly to the ESP32 over your local network.

```
Browser  ──HTTP──>  ESP32 (http://walkey-talkey.local)
   │
   └── served from Cloudflare Pages (static HTML/JS/CSS)
```

This means:

- **Your device must be on the same network as your browser** for config, recordings, and logs to work
- **Whisper transcription runs entirely in-browser** using a Web Worker -- no audio ever leaves your machine
- **The device URL** is stored in your browser's localStorage and configurable in the header bar

## Troubleshooting

### Build fails with Node version error

Add the environment variable `NODE_VERSION` = `18` (or `20`) in your Cloudflare Pages build settings.

### CORS errors when connecting to the device

The ESP32 firmware includes `Access-Control-Allow-Origin: *` headers on all API responses. If you see CORS errors, make sure you're running the latest firmware. Mixed content (HTTPS page calling HTTP device) can also cause issues -- see below.

### Mixed content blocked (HTTPS -> HTTP)

Cloudflare Pages serves over HTTPS, but your ESP32 device is on `http://`. Most browsers block mixed content by default. Workarounds:

1. **Use the local dev server instead** (`npm run dev` at `http://localhost:3000`) -- no mixed content issue
2. **Allow insecure content** for the Cloudflare Pages domain in your browser settings
3. **Use the device's built-in web portal** directly at `http://walkey-talkey.local` -- the firmware serves its own config UI that doesn't have this limitation

### Page loads but can't reach the device

The dashboard is a static site -- it has no backend proxy. Your browser must be able to reach `http://walkey-talkey.local` (or the device's IP) directly. This won't work from a phone on cellular data or a different network.

## AI Context

- The build output is a plain static site with no server-side dependencies
- `next.config.ts` sets `output: "export"` and `images: { unoptimized: true }`
- The `out/` directory contains everything needed -- just upload it to any static host
- No environment variables or secrets are required at build time
- The site works identically whether hosted on Cloudflare Pages, GitHub Pages, Vercel (static), or served from a local `npx serve out`
