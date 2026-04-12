"use client";

import { useState, useEffect } from "react";
import { useDevice } from "@/lib/device-store";
import { api } from "@/lib/api";
import type { Mode, ModeSummary, Binding } from "@/types/config";
import { BindingEditor } from "./binding-editor";
import {
  ChevronDown,
  ChevronRight,
  Save,
  Plus,
  Trash2,
  Loader2,
} from "lucide-react";
import { useLogs } from "@/lib/log-store";
import { cn } from "@/lib/utils";

interface ModeCardProps {
  summary: ModeSummary;
  onDeleted?: () => void;
}

export function ModeCard({ summary, onDeleted }: ModeCardProps) {
  const { deviceUrl } = useDevice();
  const { addWebLog } = useLogs();
  const [expanded, setExpanded] = useState(false);
  const [mode, setMode] = useState<Mode | null>(null);
  const [loading, setLoading] = useState(false);
  const [saving, setSaving] = useState(false);
  const [dirty, setDirty] = useState(false);
  const [toast, setToast] = useState<string | null>(null);

  useEffect(() => {
    if (!expanded || mode || summary.builtIn) return;
    setLoading(true);
    api
      .getMode(deviceUrl, summary.id)
      .then((m) => setMode(m))
      .catch((e) => console.error(e))
      .finally(() => setLoading(false));
  }, [expanded, mode, deviceUrl, summary.id, summary.builtIn]);

  const showToast = (msg: string) => {
    setToast(msg);
    setTimeout(() => setToast(null), 2000);
  };

  const handleSave = async () => {
    if (!mode) return;
    setSaving(true);
    try {
      await api.setMode(deviceUrl, mode.id, mode);
      setDirty(false);
      showToast("Saved!");
      addWebLog("CONFIG", `Mode saved: ${mode.id} (${mode.label})`);
    } catch (e) {
      showToast(`Error: ${e}`);
      addWebLog("ERROR", `Mode save failed: ${e}`);
    } finally {
      setSaving(false);
    }
  };

  const handleDelete = async () => {
    if (!confirm(`Delete mode "${summary.label}"?`)) return;
    try {
      await api.deleteMode(deviceUrl, summary.id);
      addWebLog("CONFIG", `Mode deleted: ${summary.id} (${summary.label})`);
      onDeleted?.();
    } catch (e) {
      showToast(`Error: ${e}`);
      addWebLog("ERROR", `Mode delete failed: ${e}`);
    }
  };

  const updateBinding = (index: number, binding: Binding) => {
    if (!mode) return;
    const bindings = [...mode.bindings];
    bindings[index] = binding;
    setMode({ ...mode, bindings });
    setDirty(true);
  };

  const removeBinding = (index: number) => {
    if (!mode) return;
    setMode({
      ...mode,
      bindings: mode.bindings.filter((_, i) => i !== index),
    });
    setDirty(true);
  };

  const addBinding = () => {
    if (!mode) return;
    setMode({
      ...mode,
      bindings: [
        ...mode.bindings,
        { input: "touch", trigger: "tap", actions: [{ type: "noop" }] },
      ],
    });
    setDirty(true);
  };

  const updateModeField = (field: string, value: unknown) => {
    if (!mode) return;
    setMode({ ...mode, [field]: value });
    setDirty(true);
  };

  return (
    <div className={cn("card", dirty && "border-highlight")}>
      <button
        type="button"
        className="flex w-full items-center gap-3 text-left"
        onClick={() => setExpanded(!expanded)}
      >
        {expanded ? (
          <ChevronDown size={20} className="text-primary" />
        ) : (
          <ChevronRight size={20} className="text-text-muted" />
        )}
        <div className="flex-1">
          <span className="text-lg font-extrabold">{summary.label}</span>
          {summary.builtIn && (
            <span className="badge badge-primary ml-2">Built-in</span>
          )}
        </div>
        <span className="badge badge-accent">
          {summary.bindingCount} bindings
        </span>
        <span className="text-xs text-text-muted">#{summary.cycleOrder}</span>
      </button>

      {toast && (
        <div className="mt-2 rounded-lg bg-highlight/20 px-3 py-1 text-sm font-bold text-highlight">
          {toast}
        </div>
      )}

      {expanded && (
        <div className="mt-4 space-y-4">
          {summary.builtIn ? (
            <p className="text-sm text-text-muted">
              Built-in mode. Configure via Defaults settings.
            </p>
          ) : loading ? (
            <div className="flex items-center justify-center py-8">
              <Loader2 className="animate-spin text-primary" size={24} />
            </div>
          ) : mode ? (
            <>
              <div className="flex flex-col gap-2 sm:flex-row">
                <div className="flex-1">
                  <label className="mb-1 block text-xs font-bold uppercase tracking-wider text-text-muted">
                    Label
                  </label>
                  <input
                    className="input"
                    value={mode.label}
                    onChange={(e) => updateModeField("label", e.target.value)}
                  />
                </div>
                <div className="w-28">
                  <label className="mb-1 block text-xs font-bold uppercase tracking-wider text-text-muted">
                    Cycle #
                  </label>
                  <input
                    className="input"
                    type="number"
                    min={0}
                    value={mode.cycleOrder}
                    onChange={(e) =>
                      updateModeField("cycleOrder", Number(e.target.value))
                    }
                  />
                </div>
              </div>

              <div className="space-y-3">
                <div className="text-xs font-bold uppercase tracking-wider text-text-muted">
                  Bindings
                </div>
                {mode.bindings.map((binding, i) => (
                  <BindingEditor
                    key={i}
                    binding={binding}
                    onChange={(b) => updateBinding(i, b)}
                    onRemove={() => removeBinding(i)}
                  />
                ))}
                <button
                  type="button"
                  className="btn btn-ghost w-full"
                  onClick={addBinding}
                >
                  <Plus size={16} className="mr-1" />
                  Add Binding
                </button>
              </div>

              <div className="flex gap-2 border-t border-border pt-4">
                <button
                  className="btn btn-primary flex-1"
                  onClick={handleSave}
                  disabled={saving || !dirty}
                >
                  {saving ? (
                    <Loader2 size={16} className="mr-1 animate-spin" />
                  ) : (
                    <Save size={16} className="mr-1" />
                  )}
                  Save Mode
                </button>
                <button
                  className="btn btn-danger"
                  onClick={handleDelete}
                  title="Delete mode"
                >
                  <Trash2 size={16} />
                </button>
              </div>
            </>
          ) : null}
        </div>
      )}
    </div>
  );
}
