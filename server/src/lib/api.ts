import type {
  DeviceConfig,
  Mode,
  ModeSummary,
  Defaults,
  WifiConfig,
  RecordingSettings,
  RecordingFile,
  LogEntry,
} from "@/types/config";

type ApiLogCallback = (method: string, path: string, status: number, ok: boolean, ms: number) => void;

let _apiLogCallback: ApiLogCallback | null = null;

export function setApiLogCallback(cb: ApiLogCallback | null) {
  _apiLogCallback = cb;
}

async function apiFetch<T = unknown>(
  deviceUrl: string,
  path: string,
  opts: RequestInit = {},
): Promise<T> {
  const url = `${deviceUrl}${path}`;
  const method = (opts.method ?? "GET").toUpperCase();
  const headers: Record<string, string> = { ...opts.headers as Record<string, string> };
  if (opts.body) {
    headers["Content-Type"] = "application/json";
  }
  const t0 = performance.now();
  const res = await fetch(url, { ...opts, headers });
  const elapsed = Math.round(performance.now() - t0);
  const text = await res.text();
  let json: T;
  try {
    json = JSON.parse(text);
  } catch {
    json = { raw: text } as T;
  }
  _apiLogCallback?.(method, path, res.status, res.ok, elapsed);
  if (!res.ok) {
    throw new Error(`HTTP ${res.status}: ${JSON.stringify(json)}`);
  }
  return json;
}

function put<T = unknown>(deviceUrl: string, path: string, body: unknown): Promise<T> {
  return apiFetch<T>(deviceUrl, path, {
    method: "PUT",
    body: JSON.stringify(body),
  });
}

function post<T = unknown>(deviceUrl: string, path: string, body: unknown): Promise<T> {
  return apiFetch<T>(deviceUrl, path, {
    method: "POST",
    body: JSON.stringify(body),
  });
}

const DL_CHUNK = 32768; // 32 KB per Range request
const DL_RETRIES = 3;

/**
 * Reliably download a file from the ESP32 using byte-range requests.
 * The ESP32 httpd randomly truncates large responses, so we fetch
 * in small chunks and assemble them client-side.
 */
export async function downloadFile(
  url: string,
  expectedSize: number,
): Promise<ArrayBuffer> {
  let totalSize = expectedSize;

  if (totalSize <= 0) {
    try {
      const head = await fetch(url, { method: "HEAD" });
      const cl = head.headers.get("content-length");
      if (cl) totalSize = parseInt(cl, 10);
    } catch {
      // ignore
    }
  }

  if (totalSize <= 0) {
    const res = await fetch(url);
    return res.arrayBuffer();
  }

  const chunks: Uint8Array[] = [];
  let offset = 0;

  while (offset < totalSize) {
    const end = Math.min(offset + DL_CHUNK - 1, totalSize - 1);
    let chunk: ArrayBuffer | null = null;

    for (let attempt = 0; attempt < DL_RETRIES; attempt++) {
      try {
        const res = await fetch(url, {
          headers: { Range: `bytes=${offset}-${end}` },
        });
        chunk = await res.arrayBuffer();
        if (chunk.byteLength > 0) break;
      } catch {
        // network hiccup, retry
      }
      if (attempt < DL_RETRIES - 1)
        await new Promise((r) => setTimeout(r, 200));
    }

    if (!chunk || chunk.byteLength === 0) break;
    chunks.push(new Uint8Array(chunk));
    offset += chunk.byteLength;
  }

  const result = new Uint8Array(offset);
  let pos = 0;
  for (const c of chunks) {
    result.set(c, pos);
    pos += c.byteLength;
  }
  return result.buffer;
}

export const api = {
  ping: (url: string) => apiFetch(url, "/ping"),

  getConfig: (url: string) => apiFetch<DeviceConfig>(url, "/config"),
  getConfigCanonical: (url: string) => apiFetch<DeviceConfig>(url, "/config/canonical"),
  setConfig: (url: string, config: DeviceConfig) => put(url, "/config", config),
  validateConfig: (url: string, config: unknown) => post(url, "/config/validate", config),
  resetConfig: (url: string) => post(url, "/config/reset", {}),

  getModes: (url: string) => apiFetch<ModeSummary[]>(url, "/api/modes"),
  getMode: (url: string, id: string) =>
    apiFetch<Mode>(url, `/api/mode?id=${encodeURIComponent(id)}`),
  setMode: (url: string, id: string, mode: Mode) =>
    put(url, `/api/mode?id=${encodeURIComponent(id)}`, mode),
  createMode: (url: string, mode: Mode) => post(url, "/api/mode", mode),
  deleteMode: (url: string, id: string) =>
    apiFetch(url, `/api/mode?id=${encodeURIComponent(id)}`, { method: "DELETE" }),

  getDefaults: (url: string) => apiFetch<Defaults>(url, "/api/defaults"),
  setDefaults: (url: string, defaults: Partial<Defaults>) =>
    put(url, "/api/defaults", defaults),

  getWifi: (url: string) => apiFetch<WifiConfig>(url, "/api/wifi"),

  getActiveMode: (url: string) =>
    apiFetch<DeviceConfig>(url, "/config").then((c) => c.activeMode),
  setActiveMode: (url: string, modeId: string) =>
    put(url, "/api/active-mode", { activeMode: modeId }),

  getRecording: (url: string) => apiFetch<RecordingSettings>(url, "/api/recording"),
  setRecording: (url: string, settings: Partial<RecordingSettings>) =>
    put(url, "/api/recording", settings),

  getRecordings: (url: string) =>
    apiFetch<{ recordings: RecordingFile[] }>(url, "/api/recordings").then(
      (r) => r.recordings ?? [],
    ),
  recordingDownloadUrl: (deviceUrl: string, file: string) =>
    `${deviceUrl}/api/recordings/download?file=${file}`,
  deleteRecording: (url: string, file: string) =>
    apiFetch(url, `/api/recordings/delete?file=${file}`),

  getLogs: (url: string) =>
    apiFetch<{ logs: Omit<LogEntry, "source">[] }>(url, "/api/logs").then(
      (r) => r.logs ?? [],
    ),
};
