"use client";

import { useState } from "react";
import { useDevice } from "@/lib/device-store";
import { Wifi, WifiOff, Loader2, Settings } from "lucide-react";
import { cn } from "@/lib/utils";

export function DeviceConnector() {
  const { deviceUrl, setDeviceUrl, connected, connecting, testConnection } =
    useDevice();
  const [open, setOpen] = useState(false);
  const [draft, setDraft] = useState(deviceUrl);

  const handleSave = () => {
    setDeviceUrl(draft);
    setOpen(false);
  };

  return (
    <div className="relative">
      <button
        onClick={() => {
          setDraft(deviceUrl);
          setOpen(!open);
        }}
        className={cn(
          "flex items-center gap-2 rounded-lg px-3 py-2 text-sm font-bold transition-all",
          connected
            ? "bg-success/20 text-success"
            : "bg-danger/20 text-danger",
        )}
      >
        {connecting ? (
          <Loader2 size={16} className="animate-spin" />
        ) : connected ? (
          <Wifi size={16} />
        ) : (
          <WifiOff size={16} />
        )}
        <span className="hidden sm:inline">
          {connected ? "Connected" : "Disconnected"}
        </span>
        <Settings size={14} className="text-text-muted" />
      </button>

      {open && (
        <div className="absolute right-0 top-full z-50 mt-2 w-80 rounded-xl border-2 border-border bg-surface p-4 shadow-2xl">
          <label className="mb-1 block text-xs font-bold uppercase tracking-wider text-text-muted">
            Device URL
          </label>
          <input
            className="input mb-3"
            value={draft}
            onChange={(e) => setDraft(e.target.value)}
            placeholder="http://walkey-talkey.local"
            onKeyDown={(e) => e.key === "Enter" && handleSave()}
          />
          <div className="flex gap-2">
            <button className="btn btn-primary flex-1" onClick={handleSave}>
              Save
            </button>
            <button
              className="btn btn-ghost flex-1"
              onClick={() => testConnection()}
              disabled={connecting}
            >
              {connecting ? "Testing..." : "Test"}
            </button>
          </div>
          {!connected && !connecting && (
            <p className="mt-2 text-xs text-danger">
              Cannot reach device. Check the URL and ensure you&apos;re on the same
              network.
            </p>
          )}
        </div>
      )}
    </div>
  );
}
