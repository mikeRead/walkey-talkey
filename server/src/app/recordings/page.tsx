"use client";

import { RecordingList } from "@/components/recording-list";

export default function RecordingsPage() {
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
        <RecordingList />
      </div>
    </div>
  );
}
