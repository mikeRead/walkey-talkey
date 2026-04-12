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

async function apiFetch<T = unknown>(
  deviceUrl: string,
  path: string,
  opts: RequestInit = {},
): Promise<T> {
  const url = `${deviceUrl}${path}`;
  const headers: Record<string, string> = { ...opts.headers as Record<string, string> };
  if (opts.body) {
    headers["Content-Type"] = "application/json";
  }
  const res = await fetch(url, { ...opts, headers });
  const text = await res.text();
  let json: T;
  try {
    json = JSON.parse(text);
  } catch {
    json = { raw: text } as T;
  }
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
