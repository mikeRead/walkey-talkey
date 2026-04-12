"use client";

import { useState, useEffect } from "react";
import { useDevice } from "@/lib/device-store";
import { api } from "@/lib/api";
import type { Defaults } from "@/types/config";
import { useLogs } from "@/lib/log-store";
import { Save, Loader2, Sliders } from "lucide-react";

function NumberField({
  label,
  value,
  onChange,
  min,
  max,
  step,
}: {
  label: string;
  value: number;
  onChange: (v: number) => void;
  min?: number;
  max?: number;
  step?: number;
}) {
  return (
    <div>
      <label className="mb-1 block text-xs font-bold uppercase tracking-wider text-text-muted">
        {label}
      </label>
      <div className="flex items-center gap-3">
        <input
          type="range"
          min={min}
          max={max}
          step={step}
          value={value}
          onChange={(e) => onChange(Number(e.target.value))}
          className="flex-1 accent-primary"
        />
        <input
          type="number"
          className="input w-24 text-center text-sm"
          value={value}
          min={min}
          max={max}
          step={step}
          onChange={(e) => onChange(Number(e.target.value))}
        />
      </div>
    </div>
  );
}

export function DefaultsEditor() {
  const { deviceUrl, connected } = useDevice();
  const { addWebLog } = useLogs();
  const [defaults, setDefaults] = useState<Defaults | null>(null);
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [dirty, setDirty] = useState(false);
  const [toast, setToast] = useState<string | null>(null);

  useEffect(() => {
    if (!connected) return;
    setLoading(true);
    api
      .getDefaults(deviceUrl)
      .then(setDefaults)
      .catch((e) => console.error(e))
      .finally(() => setLoading(false));
  }, [deviceUrl, connected]);

  if (!connected) return null;

  if (loading || !defaults) {
    return (
      <div className="card flex items-center justify-center py-8">
        <Loader2 className="animate-spin text-primary" size={24} />
      </div>
    );
  }

  const update = (path: string[], value: unknown) => {
    const next = JSON.parse(JSON.stringify(defaults));
    let obj = next as Record<string, unknown>;
    for (let i = 0; i < path.length - 1; i++) {
      obj = obj[path[i]] as Record<string, unknown>;
    }
    obj[path[path.length - 1]] = value;
    setDefaults(next);
    setDirty(true);
  };

  const handleSave = async () => {
    setSaving(true);
    try {
      await api.setDefaults(deviceUrl, defaults);
      setDirty(false);
      setToast("Saved!");
      addWebLog("CONFIG", "Defaults saved");
      setTimeout(() => setToast(null), 2000);
    } catch (e) {
      setToast(`Error: ${e}`);
      addWebLog("ERROR", `Defaults save failed: ${e}`);
      setTimeout(() => setToast(null), 3000);
    } finally {
      setSaving(false);
    }
  };

  return (
    <div className={`card ${dirty ? "border-highlight" : ""}`}>
      <div className="mb-4 flex items-center gap-2">
        <Sliders size={20} className="text-secondary" />
        <h2 className="section-title text-secondary">Defaults</h2>
      </div>

      {toast && (
        <div className="mb-3 rounded-lg bg-highlight/20 px-3 py-1 text-sm font-bold text-highlight">
          {toast}
        </div>
      )}

      {/* Touch Timing */}
      <div className="mb-6">
        <h3 className="mb-3 text-sm font-bold uppercase tracking-wider text-accent">
          Touch Timing
        </h3>
        <div className="space-y-3">
          <NumberField
            label="Hold (ms)"
            value={defaults.touch.holdMs}
            onChange={(v) => update(["touch", "holdMs"], v)}
            min={100}
            max={2000}
            step={50}
          />
          <NumberField
            label="Double Tap (ms)"
            value={defaults.touch.doubleTapMs}
            onChange={(v) => update(["touch", "doubleTapMs"], v)}
            min={100}
            max={1000}
            step={25}
          />
          <NumberField
            label="Swipe Min Distance"
            value={defaults.touch.swipeMinDistance}
            onChange={(v) => update(["touch", "swipeMinDistance"], v)}
            min={10}
            max={200}
            step={5}
          />
        </div>
      </div>

      {/* Mouse Type */}
      <div className="mb-6">
        <h3 className="mb-3 text-sm font-bold uppercase tracking-wider text-accent">
          Default Mouse
        </h3>
        <div className="flex gap-2">
          {(["airMouse", "touchMouse"] as const).map((type) => (
            <button
              key={type}
              className={`btn flex-1 ${
                defaults.defaultMouse === type ? "btn-primary" : "btn-ghost"
              }`}
              onClick={() => update(["defaultMouse"], type)}
            >
              {type === "airMouse" ? "Air Mouse" : "Touch Mouse"}
            </button>
          ))}
        </div>
      </div>

      {/* Air Mouse */}
      <div className="mb-6">
        <h3 className="mb-3 text-sm font-bold uppercase tracking-wider text-accent">
          Air Mouse
        </h3>
        <div className="space-y-3">
          <NumberField
            label="Sensitivity"
            value={defaults.airMouse.sensitivity}
            onChange={(v) => update(["airMouse", "sensitivity"], v)}
            min={0.01}
            max={5}
            step={0.01}
          />
          <NumberField
            label="Dead Zone (dps)"
            value={defaults.airMouse.deadZoneDps}
            onChange={(v) => update(["airMouse", "deadZoneDps"], v)}
            min={0}
            max={30}
            step={0.5}
          />
          <NumberField
            label="Easing Exponent"
            value={defaults.airMouse.easingExponent}
            onChange={(v) => update(["airMouse", "easingExponent"], v)}
            min={0.5}
            max={3}
            step={0.05}
          />
          <NumberField
            label="Max DPS"
            value={defaults.airMouse.maxDps}
            onChange={(v) => update(["airMouse", "maxDps"], v)}
            min={50}
            max={1000}
            step={10}
          />
          <NumberField
            label="EMA Alpha"
            value={defaults.airMouse.emaAlpha}
            onChange={(v) => update(["airMouse", "emaAlpha"], v)}
            min={0.01}
            max={1}
            step={0.01}
          />
          <NumberField
            label="Rewind Depth"
            value={defaults.airMouse.rewindDepth}
            onChange={(v) => update(["airMouse", "rewindDepth"], v)}
            min={0}
            max={16}
            step={1}
          />
          <NumberField
            label="Rewind Decay"
            value={defaults.airMouse.rewindDecay}
            onChange={(v) => update(["airMouse", "rewindDecay"], v)}
            min={0}
            max={1}
            step={0.05}
          />
          <NumberField
            label="Calibration Samples"
            value={defaults.airMouse.calibrationSamples}
            onChange={(v) => update(["airMouse", "calibrationSamples"], v)}
            min={16}
            max={512}
            step={16}
          />
        </div>
      </div>

      {/* Touch Mouse */}
      <div className="mb-6">
        <h3 className="mb-3 text-sm font-bold uppercase tracking-wider text-accent">
          Touch Mouse
        </h3>
        <div className="space-y-3">
          <NumberField
            label="Sensitivity"
            value={defaults.touchMouse.sensitivity}
            onChange={(v) => update(["touchMouse", "sensitivity"], v)}
            min={0.1}
            max={5}
            step={0.1}
          />
          <NumberField
            label="Move Threshold (px)"
            value={defaults.touchMouse.moveThresholdPx}
            onChange={(v) => update(["touchMouse", "moveThresholdPx"], v)}
            min={1}
            max={50}
            step={1}
          />
          <NumberField
            label="Tap Drag Window (ms)"
            value={defaults.touchMouse.tapDragWindowMs}
            onChange={(v) => update(["touchMouse", "tapDragWindowMs"], v)}
            min={50}
            max={500}
            step={10}
          />
        </div>
      </div>

      <button
        className="btn btn-primary w-full"
        onClick={handleSave}
        disabled={saving || !dirty}
      >
        {saving ? (
          <Loader2 size={16} className="mr-1 animate-spin" />
        ) : (
          <Save size={16} className="mr-1" />
        )}
        Save Defaults
      </button>
    </div>
  );
}
