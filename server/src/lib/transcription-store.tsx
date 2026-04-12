"use client";

import {
  createContext,
  useContext,
  useState,
  useCallback,
  useEffect,
  useRef,
  useMemo,
  type ReactNode,
} from "react";
import { useDevice } from "./device-store";
import { useLogs } from "./log-store";
import { api, downloadFile } from "./api";
import type { RecordingFile, RecordingSettings } from "@/types/config";

const POLL_INTERVAL = 10_000;
const RMS_SILENCE_THRESHOLD = 0.001;
const STORAGE_PREFIX = "walkey-transcript:";
const DEFAULT_MODEL = "onnx-community/whisper-tiny";
const DEFAULT_LANGUAGE = "en";

export interface TranscriptionState {
  status:
    | "queued"
    | "fetching"
    | "loading"
    | "transcribing"
    | "complete"
    | "error";
  progress: number;
  progressMsg: string;
  transcript: string;
  error: string | null;
}

interface TranscriptionContext {
  recordings: RecordingFile[];
  recordingSettings: RecordingSettings | null;
  fetchRecordings: () => Promise<void>;
  getTranscription: (path: string) => TranscriptionState | undefined;
  transcribe: (path: string) => void;
  deleteTranscription: (path: string) => void;
  deleteRecording: (path: string) => Promise<void>;
  pendingCount: number;
  setRecordingEnabled: (enabled: boolean) => Promise<void>;
  transcriptionVersion: number;
}

const TranscriptionCtx = createContext<TranscriptionContext | null>(null);

function loadSavedTranscripts(): Map<string, TranscriptionState> {
  const map = new Map<string, TranscriptionState>();
  try {
    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i);
      if (!key?.startsWith(STORAGE_PREFIX)) continue;
      const path = key.slice(STORAGE_PREFIX.length);
      const data = JSON.parse(localStorage.getItem(key)!);
      if (data?.transcript) {
        map.set(path, {
          status: "complete",
          progress: 100,
          progressMsg: "",
          transcript: data.transcript,
          error: null,
        });
      }
    }
  } catch {
    // localStorage unavailable or corrupt
  }
  return map;
}

function saveTranscript(path: string, transcript: string) {
  try {
    localStorage.setItem(
      STORAGE_PREFIX + path,
      JSON.stringify({ transcript, savedAt: Date.now() }),
    );
  } catch {
    // quota exceeded or unavailable
  }
}

function removeSavedTranscript(path: string) {
  try {
    localStorage.removeItem(STORAGE_PREFIX + path);
  } catch {
    // ignore
  }
}

function computeRms(samples: Float32Array): number {
  let sum = 0;
  for (let i = 0; i < samples.length; i++) sum += samples[i] * samples[i];
  return Math.sqrt(sum / samples.length);
}

async function fetchAudioSamples(
  url: string,
  expectedSize: number,
): Promise<Float32Array | null> {
  const buf = await downloadFile(url, expectedSize);
  if (buf.byteLength < 100) return null;
  const ctx = new AudioContext({ sampleRate: 16000 });
  const decoded = await ctx.decodeAudioData(buf);
  const mono = decoded.getChannelData(0);
  await ctx.close();
  return new Float32Array(mono);
}

export function TranscriptionProvider({ children }: { children: ReactNode }) {
  const { deviceUrl, connected } = useDevice();
  const { addWebLog } = useLogs();

  const [recordings, setRecordings] = useState<RecordingFile[]>([]);
  const [settings, setSettings] = useState<RecordingSettings | null>(null);
  const [transcriptionVersion, setTranscriptionVersion] = useState(0);

  // Transcriptions stored in a ref so updates don't re-render the entire tree.
  // Components that need reactivity use `transcriptionVersion` to trigger re-renders.
  const transcriptionsRef = useRef<Map<string, TranscriptionState>>(loadSavedTranscripts());
  const knownSizesRef = useRef<Map<string, number>>(new Map());
  const queueRef = useRef<string[]>([]);
  const processingRef = useRef(false);
  const workerRef = useRef<Worker | null>(null);
  const mountedRef = useRef(true);

  // Keep latest values in refs so polling/processing callbacks stay stable
  const deviceUrlRef = useRef(deviceUrl);
  deviceUrlRef.current = deviceUrl;
  const addWebLogRef = useRef(addWebLog);
  addWebLogRef.current = addWebLog;

  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
      workerRef.current?.terminate();
      workerRef.current = null;
    };
  }, []);

  const bumpVersion = useCallback(() => {
    if (mountedRef.current) setTranscriptionVersion((v) => v + 1);
  }, []);

  const updateState = useCallback(
    (path: string, partial: Partial<TranscriptionState>) => {
      const map = transcriptionsRef.current;
      const existing = map.get(path) ?? {
        status: "queued" as const,
        progress: 0,
        progressMsg: "",
        transcript: "",
        error: null,
      };
      map.set(path, { ...existing, ...partial });
      bumpVersion();
    },
    [bumpVersion],
  );

  const processQueue = useCallback(async () => {
    if (processingRef.current) return;
    const path = queueRef.current.shift();
    if (!path) return;

    processingRef.current = true;
    const downloadUrl = api.recordingDownloadUrl(deviceUrlRef.current, path);
    const fileSize = knownSizesRef.current.get(path) ?? 0;

    updateState(path, { status: "fetching", progressMsg: "Fetching audio..." });
    addWebLogRef.current("WHISPER", `Auto-transcription started: ${path}`);

    try {
      const audio = await fetchAudioSamples(downloadUrl, fileSize);
      if (!audio) {
        updateState(path, {
          status: "error",
          error: "Failed to fetch audio from device",
        });
        processingRef.current = false;
        processQueue();
        return;
      }

      const rms = computeRms(audio);
      addWebLogRef.current("WHISPER", `Audio RMS: ${rms.toFixed(6)}, ${audio.length} samples`);
      if (rms < RMS_SILENCE_THRESHOLD) {
        const transcript = "(No speech detected -- audio is silent or too quiet)";
        updateState(path, { status: "complete", transcript, progress: 100 });
        saveTranscript(path, transcript);
        addWebLogRef.current("WHISPER", `Skipped: RMS below threshold`);
        processingRef.current = false;
        processQueue();
        return;
      }

      if (!workerRef.current) {
        workerRef.current = new Worker(
          new URL("./whisper-worker.ts", import.meta.url),
        );
      }
      const worker = workerRef.current;

      updateState(path, { status: "loading", progressMsg: "Loading model..." });

      worker.onmessage = (e) => {
        const msg = e.data;
        if (msg.status === "loading") {
          updateState(path, {
            status: "loading",
            progressMsg: msg.message || "Loading model...",
          });
        } else if (msg.status === "progress") {
          updateState(path, {
            status: "loading",
            progress: msg.progress ?? 0,
            progressMsg: msg.file ? `Loading ${msg.file}...` : "Loading model...",
          });
        } else if (msg.status === "transcribing") {
          updateState(path, {
            status: "transcribing",
            progressMsg: "Transcribing...",
          });
        } else if (msg.status === "update") {
          if (msg.data?.[0]) {
            updateState(path, { status: "transcribing", transcript: msg.data[0] });
          }
        } else if (msg.status === "complete") {
          const text =
            msg.data?.text ?? (typeof msg.data === "string" ? msg.data : "");
          updateState(path, { status: "complete", transcript: text, progress: 100 });
          saveTranscript(path, text);
          addWebLogRef.current("WHISPER", `Transcription complete: ${path}`);
          processingRef.current = false;
          processQueue();
        } else if (msg.status === "error") {
          updateState(path, {
            status: "error",
            error: String(msg.data),
          });
          addWebLogRef.current("ERROR", `Whisper failed: ${msg.data}`);
          processingRef.current = false;
          processQueue();
        }
      };

      worker.postMessage({
        audio,
        model: DEFAULT_MODEL,
        language: DEFAULT_LANGUAGE,
      });
    } catch (err) {
      updateState(path, { status: "error", error: String(err) });
      addWebLogRef.current("ERROR", `Whisper failed: ${err}`);
      processingRef.current = false;
      processQueue();
    }
  }, [updateState]);

  const enqueue = useCallback(
    (path: string) => {
      if (queueRef.current.includes(path)) return;
      queueRef.current.push(path);
      updateState(path, {
        status: "queued",
        progress: 0,
        progressMsg: "Queued for transcription...",
        transcript: "",
        error: null,
      });
      processQueue();
    },
    [updateState, processQueue],
  );

  // Stable ref so polling can call enqueue without dep churn
  const enqueueRef = useRef(enqueue);
  enqueueRef.current = enqueue;

  const transcribe = useCallback((path: string) => {
    removeSavedTranscript(path);
    queueRef.current = queueRef.current.filter((p) => p !== path);
    enqueueRef.current(path);
  }, []);

  const deleteTranscription = useCallback((path: string) => {
    removeSavedTranscript(path);
    transcriptionsRef.current.delete(path);
    bumpVersion();
  }, [bumpVersion]);

  const deleteRecording = useCallback(async (path: string) => {
    await api.deleteRecording(deviceUrlRef.current, path);
    setRecordings((prev) => prev.filter((r) => r.path !== path));
    removeSavedTranscript(path);
    transcriptionsRef.current.delete(path);
    knownSizesRef.current.delete(path);
    bumpVersion();
    addWebLogRef.current("ACTION", `Recording deleted: ${path}`);
  }, [bumpVersion]);

  const setRecordingEnabled = useCallback(async (enabled: boolean) => {
    await api.setRecording(deviceUrlRef.current, { enabled });
    setSettings((prev) => (prev ? { ...prev, enabled } : prev));
    addWebLogRef.current("CONFIG", `Recording ${enabled ? "enabled" : "disabled"}`);
  }, []);

  // Stable polling function -- reads everything from refs, never changes identity
  const pollNow = useCallback(async () => {
    if (!mountedRef.current) return;
    const url = deviceUrlRef.current;
    try {
      const recs = await api.getRecordings(url);
      if (!mountedRef.current) return;
      setRecordings(recs);

      const sizes = knownSizesRef.current;
      const txMap = transcriptionsRef.current;

      for (const rec of recs) {
        const prevSize = sizes.get(rec.path);
        if (prevSize === undefined) {
          // First time seeing this file -- record size, wait for next poll
          sizes.set(rec.path, rec.size);
        } else if (prevSize !== rec.size) {
          // Size changed -- still being written, update and wait
          sizes.set(rec.path, rec.size);
        } else {
          // Size stable across two polls -- ready for transcription
          if (!txMap.has(rec.path)) {
            enqueueRef.current(rec.path);
          }
        }
      }

      // Remove sizes for deleted files
      const currentPaths = new Set(recs.map((r) => r.path));
      for (const p of sizes.keys()) {
        if (!currentPaths.has(p)) sizes.delete(p);
      }
    } catch (e) {
      console.error("Failed to fetch recordings:", e);
    }
  }, []);

  const fetchSettingsOnce = useCallback(async () => {
    if (!mountedRef.current) return;
    try {
      const sett = await api.getRecording(deviceUrlRef.current);
      if (mountedRef.current) setSettings(sett);
    } catch {
      // device unreachable
    }
  }, []);

  // Single stable effect -- only depends on `connected`
  useEffect(() => {
    if (!connected) return;
    fetchSettingsOnce();
    pollNow();
    const id = setInterval(pollNow, POLL_INTERVAL);
    return () => clearInterval(id);
  }, [connected, fetchSettingsOnce, pollNow]);

  // Stable getTranscription reads from ref -- doesn't change identity
  const getTranscription = useCallback(
    (path: string) => transcriptionsRef.current.get(path),
    [],
  );

  // Memoize pendingCount so it only recalculates when version bumps
  const pendingCount = useMemo(() => {
    // version used for reactivity
    void transcriptionVersion;
    let count = 0;
    for (const t of transcriptionsRef.current.values()) {
      if (
        t.status === "queued" ||
        t.status === "fetching" ||
        t.status === "loading" ||
        t.status === "transcribing"
      ) {
        count++;
      }
    }
    return count;
  }, [transcriptionVersion]);

  // Expose fetchRecordings as just a trigger for manual refresh
  const fetchRecordings = useCallback(() => {
    pollNow();
  }, [pollNow]);

  const ctxValue = useMemo(
    () => ({
      recordings,
      recordingSettings: settings,
      fetchRecordings,
      getTranscription,
      transcribe,
      deleteTranscription,
      deleteRecording,
      pendingCount,
      setRecordingEnabled,
      transcriptionVersion,
    }),
    [
      recordings,
      settings,
      fetchRecordings,
      getTranscription,
      transcribe,
      deleteTranscription,
      deleteRecording,
      pendingCount,
      setRecordingEnabled,
      transcriptionVersion,
    ],
  );

  return (
    <TranscriptionCtx.Provider value={ctxValue}>
      {children}
    </TranscriptionCtx.Provider>
  );
}

export function useTranscription() {
  const ctx = useContext(TranscriptionCtx);
  if (!ctx)
    throw new Error(
      "useTranscription must be used within TranscriptionProvider",
    );
  return ctx;
}
