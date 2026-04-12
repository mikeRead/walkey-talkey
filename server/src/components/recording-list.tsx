"use client";

import { useState, useEffect } from "react";
import { useDevice } from "@/lib/device-store";
import { api } from "@/lib/api";
import type { RecordingFile, RecordingSettings } from "@/types/config";
import { useLogs } from "@/lib/log-store";
import { formatBytes } from "@/lib/utils";
import {
  Mic,
  MicOff,
  Play,
  Download,
  Trash2,
  Loader2,
  FileAudio,
  Sparkles,
} from "lucide-react";

interface RecordingListProps {
  onTranscribe: (fileUrl: string, fileName: string) => void;
}

export function RecordingList({ onTranscribe }: RecordingListProps) {
  const { deviceUrl, connected } = useDevice();
  const { addWebLog } = useLogs();
  const [recordings, setRecordings] = useState<RecordingFile[]>([]);
  const [settings, setSettings] = useState<RecordingSettings | null>(null);
  const [loading, setLoading] = useState(false);
  const [playingUrl, setPlayingUrl] = useState<string | null>(null);

  const fetchData = async () => {
    if (!connected) return;
    setLoading(true);
    try {
      const [recs, sett] = await Promise.all([
        api.getRecordings(deviceUrl),
        api.getRecording(deviceUrl),
      ]);
      setRecordings(recs);
      setSettings(sett);
    } catch (e) {
      console.error(e);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [deviceUrl, connected]);

  const toggleRecording = async () => {
    if (!settings) return;
    try {
      const next = { enabled: !settings.enabled };
      await api.setRecording(deviceUrl, next);
      setSettings({ ...settings, ...next });
      addWebLog("CONFIG", `Recording ${next.enabled ? "enabled" : "disabled"}`);
    } catch (e) {
      console.error(e);
      addWebLog("ERROR", `Recording toggle failed: ${e}`);
    }
  };

  const handleDelete = async (file: string) => {
    if (!confirm(`Delete ${file}?`)) return;
    try {
      await api.deleteRecording(deviceUrl, file);
      setRecordings(recordings.filter((r) => r.path !== file));
      addWebLog("ACTION", `Recording deleted: ${file}`);
    } catch (e) {
      console.error(e);
      addWebLog("ERROR", `Recording delete failed: ${e}`);
    }
  };

  if (!connected) {
    return (
      <div className="memphis-bg flex flex-col items-center justify-center rounded-xl border-2 border-dashed border-border px-6 py-16 text-center">
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
          className={`btn ${settings?.enabled ? "btn-accent" : "btn-ghost"}`}
          onClick={toggleRecording}
        >
          {settings?.enabled ? (
            <Mic size={18} className="mr-2" />
          ) : (
            <MicOff size={18} className="mr-2" />
          )}
          {settings?.enabled ? "Recording Enabled" : "Recording Disabled"}
        </button>
        <p className="text-sm text-text-muted">
          {settings?.enabled
            ? "Mic activations write WAV files to SD card"
            : "Enable to save recordings to SD card"}
        </p>
        <button
          className="btn btn-sm btn-ghost ml-auto"
          onClick={fetchData}
          disabled={loading}
        >
          {loading ? <Loader2 size={14} className="animate-spin" /> : "Refresh"}
        </button>
      </div>

      {/* Recording List */}
      {loading ? (
        <div className="flex items-center justify-center py-8">
          <Loader2 className="animate-spin text-primary" size={24} />
        </div>
      ) : recordings.length === 0 ? (
        <div className="card py-8 text-center text-text-muted">
          No recordings found on SD card.
        </div>
      ) : (
        <div className="space-y-2">
          {recordings.map((rec) => {
            const downloadUrl = api.recordingDownloadUrl(deviceUrl, rec.path);
            const isPlaying = playingUrl === downloadUrl;

            return (
              <div
                key={rec.path}
                className="card flex flex-col gap-3 sm:flex-row sm:items-center"
              >
                <div className="flex-1 min-w-0">
                  <div className="truncate font-bold text-sm">{rec.path}</div>
                  <div className="text-xs text-text-muted">
                    {formatBytes(rec.size)}
                  </div>
                </div>

                {isPlaying && (
                  <audio
                    src={downloadUrl}
                    controls
                    autoPlay
                    className="w-full sm:w-64"
                    onEnded={() => setPlayingUrl(null)}
                  />
                )}

                <div className="flex gap-2">
                  <button
                    className="btn btn-sm btn-ghost"
                    onClick={() =>
                      setPlayingUrl(isPlaying ? null : downloadUrl)
                    }
                    title="Play"
                  >
                    <Play size={14} />
                  </button>
                  <a
                    href={downloadUrl}
                    className="btn btn-sm btn-ghost"
                    download
                    title="Download"
                  >
                    <Download size={14} />
                  </a>
                  <button
                    className="btn btn-sm btn-primary"
                    onClick={() => onTranscribe(downloadUrl, rec.path)}
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
            );
          })}
        </div>
      )}
    </div>
  );
}
