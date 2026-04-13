"use client";

import Image from "next/image";
import { RecordingList } from "@/components/recording-list";

export default function RecordingsPage() {
  return (
    <div>
      <div className="memphis-bg mb-6 flex items-center gap-5 rounded-xl border-2 border-dashed border-border p-6 backdrop-blur-[6px]">
        <div className="h-40 w-40 shrink-0 overflow-hidden rounded-lg">
          <Image
            src="/rec.png"
            alt="Recordings"
            width={200}
            height={200}
            className="h-full w-full object-cover"
            style={{ transform: "scale(1.2)", objectPosition: "center" }}
          />
        </div>
        <div>
          <h1 className="text-2xl font-extrabold sm:text-3xl">
            <span className="text-accent">Recordings</span>
          </h1>
          <p className="mt-1 text-sm text-text-muted">
            Play, download, and transcribe audio recordings from the SD card
          </p>
        </div>
      </div>

      <div className="space-y-6">
        <RecordingList />
      </div>
    </div>
  );
}
