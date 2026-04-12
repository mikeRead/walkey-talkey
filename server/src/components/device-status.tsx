"use client";

import { useState, useEffect } from "react";
import { useDevice } from "@/lib/device-store";
import { api } from "@/lib/api";
import type { ModeSummary } from "@/types/config";
import { Zap } from "lucide-react";

export function DeviceStatus() {
  const { deviceUrl, connected } = useDevice();
  const [activeMode, setActiveMode] = useState<string | null>(null);
  const [modes, setModes] = useState<ModeSummary[]>([]);

  useEffect(() => {
    if (!connected) return;
    api.getActiveMode(deviceUrl).then(setActiveMode).catch(() => {});
    api.getModes(deviceUrl).then(setModes).catch(() => {});
  }, [deviceUrl, connected]);

  const handleModeChange = async (modeId: string) => {
    try {
      await api.setActiveMode(deviceUrl, modeId);
      setActiveMode(modeId);
    } catch (e) {
      console.error("Failed to set active mode", e);
    }
  };

  if (!connected) return null;

  return (
    <div className="flex items-center gap-3">
      <Zap size={16} className="text-highlight" />
      <select
        className="select max-w-48 text-sm"
        value={activeMode ?? ""}
        onChange={(e) => handleModeChange(e.target.value)}
      >
        <option value="" disabled>
          Active Mode
        </option>
        {modes.map((m) => (
          <option key={m.id} value={m.id}>
            {m.label}
          </option>
        ))}
      </select>
    </div>
  );
}
