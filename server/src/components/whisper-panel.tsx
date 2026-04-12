"use client";

import { useState, useRef, useCallback, useEffect } from "react";
import { Loader2, Sparkles, Copy, Check } from "lucide-react";
import { useLogs } from "@/lib/log-store";

const MODELS = [
  { id: "onnx-community/whisper-tiny", label: "Whisper Tiny (fastest)" },
  { id: "onnx-community/whisper-base", label: "Whisper Base" },
  { id: "onnx-community/whisper-small", label: "Whisper Small (best)" },
] as const;

interface WhisperPanelProps {
  audioUrl: string | null;
  fileName: string | null;
}

type WorkerStatus =
  | "idle"
  | "loading"
  | "progress"
  | "transcribing"
  | "complete"
  | "error";

const RMS_SILENCE_THRESHOLD = 0.001;

export function WhisperPanel({ audioUrl, fileName }: WhisperPanelProps) {
  const { addWebLog } = useLogs();
  const [model, setModel] = useState<string>(MODELS[0].id);
  const [language, setLanguage] = useState("en");
  const [status, setStatus] = useState<WorkerStatus>("idle");
  const [progress, setProgress] = useState(0);
  const [progressMsg, setProgressMsg] = useState("");
  const [transcript, setTranscript] = useState("");
  const [copied, setCopied] = useState(false);
  const [audioReady, setAudioReady] = useState(false);
  const workerRef = useRef<Worker | null>(null);
  const cachedAudioRef = useRef<{ url: string; samples: Float32Array } | null>(null);

  useEffect(() => {
    return () => {
      workerRef.current?.terminate();
    };
  }, []);

  useEffect(() => {
    cachedAudioRef.current = null;
    setAudioReady(false);
    setTranscript("");
    setStatus("idle");
  }, [audioUrl]);

  const fetchAndCacheAudio = useCallback(async (): Promise<Float32Array | null> => {
    if (!audioUrl) return null;

    if (cachedAudioRef.current?.url === audioUrl) {
      return cachedAudioRef.current.samples;
    }

    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 30000);
    let response: Response;
    try {
      response = await fetch(audioUrl, { signal: controller.signal });
    } finally {
      clearTimeout(timeout);
    }
    const arrayBuffer = await response.arrayBuffer();
    addWebLog("WHISPER", `Fetched ${arrayBuffer.byteLength} bytes (${response.status}) from device`);

    if (arrayBuffer.byteLength < 100) {
      addWebLog("ERROR", `Audio fetch returned only ${arrayBuffer.byteLength} bytes -- likely truncated`);
      return null;
    }

    const audioCtx = new AudioContext({ sampleRate: 16000 });
    const decoded = await audioCtx.decodeAudioData(arrayBuffer);
    const mono = decoded.getChannelData(0);
    await audioCtx.close();

    const samples = new Float32Array(mono);
    cachedAudioRef.current = { url: audioUrl, samples };
    setAudioReady(true);
    addWebLog("WHISPER", `Decoded: ${samples.length} samples (${(samples.length / 16000).toFixed(1)}s)`);
    return samples;
  }, [audioUrl, addWebLog]);

  const computeRms = useCallback((samples: Float32Array): number => {
    let sum = 0;
    for (let i = 0; i < samples.length; i++) sum += samples[i] * samples[i];
    return Math.sqrt(sum / samples.length);
  }, []);

  const handleTranscribe = useCallback(async () => {
    if (!audioUrl) return;

    setStatus("loading");
    setTranscript("");
    setProgress(0);
    setProgressMsg("Fetching audio...");
    addWebLog("WHISPER", `Transcription started: ${fileName ?? "unknown"}`);

    try {
      const audio = await fetchAndCacheAudio();
      if (!audio) {
        setStatus("error");
        setTranscript("Error: Failed to fetch audio from device");
        return;
      }

      const rms = computeRms(audio);
      addWebLog("WHISPER", `Audio RMS: ${rms.toFixed(6)}, ${audio.length} samples`);
      if (rms < RMS_SILENCE_THRESHOLD) {
        setStatus("complete");
        setTranscript("(No speech detected -- audio is silent or too quiet)");
        addWebLog("WHISPER", `Skipped: RMS ${rms.toFixed(6)} below threshold ${RMS_SILENCE_THRESHOLD}`);
        return;
      }

      setProgressMsg("Starting Whisper worker...");

      workerRef.current?.terminate();
      const worker = new Worker(
        new URL("../lib/whisper-worker.ts", import.meta.url),
      );
      workerRef.current = worker;

      worker.onmessage = (e) => {
        const msg = e.data;
        if (msg.status === "loading") {
          setStatus("loading");
          setProgressMsg(msg.message || "Loading model...");
        } else if (msg.status === "progress") {
          setStatus("progress");
          if (msg.progress != null) setProgress(msg.progress);
          if (msg.file) setProgressMsg(`Loading ${msg.file}...`);
        } else if (msg.status === "transcribing") {
          setStatus("transcribing");
          setProgressMsg("Transcribing...");
        } else if (msg.status === "update") {
          setStatus("transcribing");
          if (msg.data?.[0]) {
            setTranscript(msg.data[0]);
          }
        } else if (msg.status === "complete") {
          setStatus("complete");
          if (msg.data?.text) {
            setTranscript(msg.data.text);
          } else if (typeof msg.data === "string") {
            setTranscript(msg.data);
          }
          addWebLog("WHISPER", `Transcription complete: ${fileName ?? "unknown"}`);
        } else if (msg.status === "error") {
          setStatus("error");
          setTranscript(`Error: ${msg.data}`);
          addWebLog("ERROR", `Whisper failed: ${msg.data}`);
        }
      };

      worker.postMessage({
        audio,
        model,
        language,
      });
    } catch (e) {
      setStatus("error");
      setTranscript(`Error: ${e}`);
      addWebLog("ERROR", `Whisper failed: ${e}`);
    }
  }, [audioUrl, model, language, fileName, fetchAndCacheAudio, computeRms, addWebLog]);

  const handleCopy = () => {
    navigator.clipboard.writeText(transcript);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  if (!audioUrl) return null;

  const isProcessing =
    status === "loading" || status === "progress" || status === "transcribing";

  return (
    <div className="card border-accent">
      <div className="mb-4 flex items-center gap-2">
        <Sparkles size={20} className="text-accent" />
        <h2 className="section-title text-accent">Whisper Transcription</h2>
      </div>

      {fileName && (
        <p className="mb-3 text-sm text-text-muted">
          File: <span className="font-bold text-text">{fileName}</span>
          {audioReady && cachedAudioRef.current && (
            <span className="ml-2 text-accent">
              ({(cachedAudioRef.current.samples.length / 16000).toFixed(1)}s cached locally)
            </span>
          )}
        </p>
      )}

      <div className="mb-4 flex flex-col gap-2 sm:flex-row">
        <select
          className="select sm:w-56"
          value={model}
          onChange={(e) => setModel(e.target.value)}
          disabled={isProcessing}
        >
          {MODELS.map((m) => (
            <option key={m.id} value={m.id}>
              {m.label}
            </option>
          ))}
        </select>

        <input
          className="input sm:w-28"
          value={language}
          onChange={(e) => setLanguage(e.target.value)}
          placeholder="en"
          disabled={isProcessing}
        />

        <button
          className="btn btn-accent"
          onClick={handleTranscribe}
          disabled={isProcessing}
        >
          {isProcessing ? (
            <Loader2 size={16} className="mr-1 animate-spin" />
          ) : (
            <Sparkles size={16} className="mr-1" />
          )}
          {isProcessing ? "Processing..." : "Transcribe"}
        </button>
      </div>

      {(status === "loading" || status === "progress") && (
        <div className="mb-4">
          <div className="mb-1 text-xs text-text-muted">{progressMsg}</div>
          <div className="h-2 w-full overflow-hidden rounded-full bg-surface-raised">
            <div
              className="h-full rounded-full bg-accent transition-all duration-300"
              style={{ width: `${Math.min(progress, 100)}%` }}
            />
          </div>
        </div>
      )}

      {status === "transcribing" && (
        <div className="mb-2 text-xs text-text-muted">
          <Loader2 size={12} className="mr-1 inline animate-spin" />
          Transcribing audio...
        </div>
      )}

      {transcript && (
        <div className="relative">
          <textarea
            className="input min-h-32 font-mono text-sm"
            value={transcript}
            readOnly
          />
          <button
            className="absolute right-2 top-2 btn btn-sm btn-ghost"
            onClick={handleCopy}
            title="Copy transcript"
          >
            {copied ? <Check size={14} className="text-success" /> : <Copy size={14} />}
          </button>
        </div>
      )}
    </div>
  );
}
