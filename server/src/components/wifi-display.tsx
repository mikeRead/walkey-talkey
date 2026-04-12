"use client";

import { useState, useEffect } from "react";
import { useDevice } from "@/lib/device-store";
import { api } from "@/lib/api";
import type { WifiConfig } from "@/types/config";
import { Wifi, Loader2 } from "lucide-react";

export function WifiDisplay() {
  const { deviceUrl, connected } = useDevice();
  const [wifi, setWifi] = useState<WifiConfig | null>(null);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    if (!connected) return;
    setLoading(true);
    api
      .getWifi(deviceUrl)
      .then(setWifi)
      .catch((e) => console.error(e))
      .finally(() => setLoading(false));
  }, [deviceUrl, connected]);

  if (!connected) return null;

  if (loading || !wifi) {
    return (
      <div className="card flex items-center justify-center py-8">
        <Loader2 className="animate-spin text-primary" size={24} />
      </div>
    );
  }

  return (
    <div className="card">
      <div className="mb-4 flex items-center gap-2">
        <Wifi size={20} className="text-primary" />
        <h2 className="section-title text-primary">Wi-Fi</h2>
        <span className="badge badge-primary ml-auto">Read-only</span>
      </div>

      <div className="grid gap-4 sm:grid-cols-2">
        <div className="rounded-lg border border-border p-3">
          <div className="mb-2 text-xs font-bold uppercase tracking-wider text-accent">
            Station (STA)
          </div>
          <div className="space-y-1 text-sm">
            <div>
              <span className="text-text-muted">SSID: </span>
              <span className="font-bold">{wifi.sta.ssid}</span>
            </div>
            <div>
              <span className="text-text-muted">Password: </span>
              <span className="font-mono text-text-muted">••••••••</span>
            </div>
          </div>
        </div>

        <div className="rounded-lg border border-border p-3">
          <div className="mb-2 text-xs font-bold uppercase tracking-wider text-secondary">
            Access Point (AP)
          </div>
          <div className="space-y-1 text-sm">
            <div>
              <span className="text-text-muted">SSID: </span>
              <span className="font-bold">{wifi.ap.ssid}</span>
            </div>
            <div>
              <span className="text-text-muted">Password: </span>
              <span className="font-mono text-text-muted">••••••••</span>
            </div>
          </div>
        </div>
      </div>

      <div className="mt-3 space-y-1 text-sm">
        <div>
          <span className="text-text-muted">Hostname: </span>
          <span className="font-bold text-highlight">{wifi.hostname}</span>
        </div>
        <div>
          <span className="text-text-muted">URL: </span>
          <span className="font-bold text-primary">{wifi.localUrl}</span>
        </div>
      </div>
    </div>
  );
}
