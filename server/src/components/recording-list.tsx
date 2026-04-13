"use client";

import { useState } from "react";
import { useDevice } from "@/lib/device-store";
import { useTranscription } from "@/lib/transcription-store";
import { api } from "@/lib/api";
import { formatBytes } from "@/lib/utils";
import { PlayButton } from "./play-button";
import {
  Mic,
  MicOff,
  Download,
  Trash2,
  Loader2,
  FileAudio,
  Sparkles,
  Copy,
  Check,
} from "lucide-react";

export function RecordingList() {
  const { deviceUrl, connected } = useDevice();
  const {
    recordings,
    recordingSettings,
    fetchRecordings,
    getTranscription,
    transcribe,
    deleteRecording,
    setRecordingEnabled,
  } = useTranscription();
  const [refreshing, setRefreshing] = useState(false);
  const [copiedPath, setCopiedPath] = useState<string | null>(null);

  const handleRefresh = async () => {
    setRefreshing(true);
    await fetchRecordings();
    setRefreshing(false);
  };

  const handleToggleRecording = async () => {
    if (!recordingSettings) return;
    try {
      await setRecordingEnabled(!recordingSettings.enabled);
    } catch (e) {
      console.error(e);
    }
  };

  const handleDelete = async (file: string) => {
    if (!confirm(`Delete ${file}?`)) return;
    try {
      await deleteRecording(file);
    } catch (e) {
      console.error(e);
    }
  };

  const handleCopy = (path: string, text: string) => {
    navigator.clipboard.writeText(text);
    setCopiedPath(path);
    setTimeout(() => setCopiedPath(null), 2000);
  };

  if (!connected) {
    return (
      <div className="memphis-bg flex flex-col items-center justify-center rounded-xl border-2 border-dashed border-border px-6 py-16 text-center backdrop-blur-[6px]">
        <FileAudio size={48} className="mb-4 text-text-muted" />
        <h2 className="mb-2 text-xl font-extrabold">No Device Connected</h2>
        <p className="text-sm text-text-muted">
          Connect to your device to manage recordings.
        </p>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      {/* Recording Toggle */}
      <div className="card flex items-center gap-4">
        <button
          className={`btn ${recordingSettings?.enabled ? "btn-accent" : "btn-ghost"}`}
          onClick={handleToggleRecording}
        >
          {recordingSettings?.enabled ? (
            <Mic size={18} className="mr-2" />
          ) : (
            <MicOff size={18} className="mr-2" />
          )}
          {recordingSettings?.enabled ? "Recording Enabled" : "Recording Disabled"}
        </button>
        <p className="text-sm text-text-muted">
          {recordingSettings?.enabled
            ? "Mic activations write WAV files by default"
            : "Enable to save recordings to SD card"}
        </p>
        <button
          className="btn btn-sm btn-ghost ml-auto"
          onClick={handleRefresh}
          disabled={refreshing}
        >
          {refreshing ? <Loader2 size={14} className="animate-spin" /> : "Refresh"}
        </button>
      </div>

      {/* Recording List */}
      {recordings.length === 0 ? (
        <div className="card py-8 text-center text-text-muted">
          No recordings found on SD card.
        </div>
      ) : (
        <div className="space-y-2">
          {recordings.map((rec) => {
            const downloadUrl = api.recordingDownloadUrl(deviceUrl, rec.path);
            const attachmentUrl = api.recordingAttachmentUrl(deviceUrl, rec.path);
            const ts = getTranscription(rec.path);

            return (
              <div
                key={rec.path}
                className="card flex flex-col gap-3"
              >
                {/* Top row: file info + actions */}
                <div className="flex flex-col gap-3 sm:flex-row sm:items-center">
                  <PlayButton src={downloadUrl} expectedSize={rec.size} />

                  <div className="flex-1 min-w-0">
                    <div className="truncate font-bold text-sm">{rec.path}</div>
                    <div className="text-xs text-text-muted">
                      {formatBytes(rec.size)}
                    </div>
                  </div>

                  <div className="flex gap-2">
                    <a
                      href={attachmentUrl}
                      className="btn btn-sm btn-ghost"
                      download
                      title="Download"
                    >
                      <Download size={14} />
                    </a>
                    <button
                      className="btn btn-sm btn-primary"
                      onClick={() => transcribe(rec.path)}
                      title="Transcribe with Whisper"
                    >
                      <Sparkles size={14} className="mr-1" />
                      <span className="hidden sm:inline">Transcribe</span>
                    </button>
                    <button
                      className="btn btn-sm btn-danger"
                      onClick={() => handleDelete(rec.path)}
                      title="Delete"
                    >
                      <Trash2 size={14} />
                    </button>
                  </div>
                </div>

                {/* Inline transcription status */}
                {ts && (
                  <TranscriptionRow
                    state={ts}
                    path={rec.path}
                    copiedPath={copiedPath}
                    onCopy={handleCopy}
                  />
                )}
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}

function TranscriptionRow({
  state,
  path,
  copiedPath,
  onCopy,
}: {
  state: NonNullable<ReturnType<ReturnType<typeof useTranscription>["getTranscription"]>>;
  path: string;
  copiedPath: string | null;
  onCopy: (path: string, text: string) => void;
}) {
  const { status, progress, progressMsg, transcript, error } = state;

  if (status === "queued") {
    return (
      <div className="text-xs text-text-muted">
        Queued for transcription...
      </div>
    );
  }

  if (status === "fetching" || status === "loading") {
    return (
      <div>
        <div className="mb-1 text-xs text-text-muted">
          <Loader2 size={10} className="mr-1 inline animate-spin" />
          {progressMsg}
        </div>
        {status === "loading" && (
          <div className="h-1.5 w-full overflow-hidden rounded-full bg-surface-raised">
            <div
              className="h-full rounded-full bg-accent transition-all duration-300"
              style={{ width: `${Math.min(progress, 100)}%` }}
            />
          </div>
        )}
      </div>
    );
  }

  if (status === "transcribing") {
    return (
      <div>
        <div className="mb-1 text-xs text-text-muted">
          <Loader2 size={10} className="mr-1 inline animate-spin" />
          Transcribing...
        </div>
        {transcript && (
          <p className="text-xs text-text-muted italic">{transcript}</p>
        )}
      </div>
    );
  }

  if (status === "error") {
    return (
      <div className="text-xs text-danger">{error}</div>
    );
  }

  if (status === "complete" && transcript) {
    return (
      <div className="relative rounded-lg bg-surface-raised p-2.5">
        <p className="pr-8 text-xs text-text whitespace-pre-wrap">{transcript}</p>
        <button
          className="absolute right-1.5 top-1.5 rounded p-1 text-text-muted hover:text-text transition-colors cursor-pointer"
          onClick={() => onCopy(path, transcript)}
          title="Copy transcript"
        >
          {copiedPath === path ? (
            <Check size={12} className="text-success" />
          ) : (
            <Copy size={12} />
          )}
        </button>
      </div>
    );
  }

  return null;
}
