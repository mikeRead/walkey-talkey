"use client";

import { useState } from "react";
import { RecordingList } from "@/components/recording-list";
import { WhisperPanel } from "@/components/whisper-panel";

export default function RecordingsPage() {
  const [transcribeUrl, setTranscribeUrl] = useState<string | null>(null);
  const [transcribeFile, setTranscribeFile] = useState<string | null>(null);

  return (
    <div>
      <div className="mb-6 rounded-xl border border-dashed border-border p-6 backdrop-blur-[6px]">
        <h1 className="text-2xl font-extrabold sm:text-3xl">
          <span className="text-accent">Recordings</span>
        </h1>
        <p className="mt-1 text-sm text-text-muted">
          Play, download, and transcribe audio recordings from the SD card
        </p>
      </div>

      <div className="space-y-6">
        <RecordingList
          onTranscribe={(url, name) => {
            setTranscribeUrl(url);
            setTranscribeFile(name);
          }}
        />
        <WhisperPanel audioUrl={transcribeUrl} fileName={transcribeFile} />
      </div>
    </div>
  );
}
