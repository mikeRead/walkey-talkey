"use client";

import { useState, useEffect, useCallback } from "react";
import { useDevice } from "@/lib/device-store";
import { api } from "@/lib/api";
import type { ModeSummary, DeviceConfig } from "@/types/config";
import { ModeCard } from "./mode-card";
import { DefaultsEditor } from "./defaults-editor";
import { WifiDisplay } from "./wifi-display";
import { useLogs } from "@/lib/log-store";
import {
  Plus,
  Loader2,
  RotateCcw,
  Code2,
  Layers,
} from "lucide-react";
import dynamic from "next/dynamic";

const MonacoEditor = dynamic(() => import("@monaco-editor/react").then((m) => m.default), {
  ssr: false,
  loading: () => (
    <div className="flex items-center justify-center py-12">
      <Loader2 className="animate-spin text-primary" size={24} />
    </div>
  ),
});

function handleEditorWillMount(monaco: Parameters<NonNullable<React.ComponentProps<typeof MonacoEditor>["beforeMount"]>>[0]) {
  monaco.editor.defineTheme("walkey-dark", {
    base: "vs-dark",
    inherit: false,
    rules: [
      { token: "", foreground: "f5f5f5", background: "0a0a0a" },
      { token: "string.key.json", foreground: "00BFFF" },
      { token: "string.value.json", foreground: "4ade80" },
      { token: "number", foreground: "FFD700" },
      { token: "number.json", foreground: "FFD700" },
      { token: "keyword", foreground: "FFD700" },
      { token: "keyword.json", foreground: "FFD700" },
      { token: "delimiter", foreground: "a0a0a0" },
      { token: "delimiter.bracket", foreground: "a0a0a0" },
      { token: "delimiter.array", foreground: "a0a0a0" },
      { token: "delimiter.comma", foreground: "a0a0a0" },
      { token: "delimiter.colon", foreground: "a0a0a0" },
      { token: "string", foreground: "4ade80" },
      { token: "comment", foreground: "888888", fontStyle: "italic" },
      { token: "type", foreground: "FFD700" },
      { token: "variable", foreground: "66d9ff" },
      { token: "constant", foreground: "FFD700" },
    ],
    colors: {
      "editor.background": "#0a0a0a",
      "editor.foreground": "#f5f5f5",
      "editor.lineHighlightBackground": "#141414",
      "editor.selectionBackground": "#7B2FBE44",
      "editorCursor.foreground": "#00BFFF",
      "editorLineNumber.foreground": "#444444",
      "editorLineNumber.activeForeground": "#888888",
      "editor.selectionHighlightBackground": "#00BFFF22",
      "editorBracketMatch.background": "#7B2FBE33",
      "editorBracketMatch.border": "#7B2FBE",
      "editorGutter.background": "#0a0a0a",
      "scrollbarSlider.background": "#2a2a2a88",
      "scrollbarSlider.hoverBackground": "#2a2a2acc",
    },
  });
}

export function ConfigEditor() {
  const { deviceUrl, connected } = useDevice();
  const { addWebLog } = useLogs();
  const [modes, setModes] = useState<ModeSummary[]>([]);
  const [loading, setLoading] = useState(false);
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [rawConfig, setRawConfig] = useState("");
  const [rawSaving, setRawSaving] = useState(false);
  const [rawValidation, setRawValidation] = useState<string | null>(null);
  const [newModeOpen, setNewModeOpen] = useState(false);
  const [newModeId, setNewModeId] = useState("");
  const [newModeLabel, setNewModeLabel] = useState("");

  const fetchModes = useCallback(async () => {
    if (!connected) return;
    setLoading(true);
    try {
      const data = await api.getModes(deviceUrl);
      const hasMouseMode = data.some((m) => m.id === "mouse");
      if (!hasMouseMode) {
        data.push({
          id: "mouse",
          label: "Mouse",
          cycleOrder: 65534,
          bindingCount: 0,
          builtIn: true,
        });
      }
      setModes(data);
    } catch (e) {
      console.error(e);
    } finally {
      setLoading(false);
    }
  }, [deviceUrl, connected]);

  useEffect(() => {
    fetchModes();
  }, [fetchModes]);

  const handleOpenAdvanced = async () => {
    if (!showAdvanced) {
      try {
        const config = await api.getConfig(deviceUrl);
        setRawConfig(JSON.stringify(config, null, 2));
        setRawValidation(null);
      } catch (e) {
        console.error(e);
      }
    }
    setShowAdvanced(!showAdvanced);
  };

  const handleValidate = async () => {
    try {
      const parsed = JSON.parse(rawConfig);
      const result = await api.validateConfig(deviceUrl, parsed);
      setRawValidation(JSON.stringify(result, null, 2));
    } catch (e) {
      setRawValidation(`Error: ${e}`);
    }
  };

  const handleRawSave = async () => {
    setRawSaving(true);
    try {
      const parsed = JSON.parse(rawConfig) as DeviceConfig;
      await api.setConfig(deviceUrl, parsed);
      setRawValidation("Saved successfully!");
      addWebLog("CONFIG", "Full config saved");
      fetchModes();
    } catch (e) {
      setRawValidation(`Error: ${e}`);
      addWebLog("ERROR", `Config save failed: ${e}`);
    } finally {
      setRawSaving(false);
    }
  };

  const handleReset = async () => {
    if (!confirm("Reset ALL settings to factory defaults? This cannot be undone.")) return;
    try {
      await api.resetConfig(deviceUrl);
      addWebLog("CONFIG", "Factory reset performed");
      fetchModes();
    } catch (e) {
      console.error(e);
      addWebLog("ERROR", `Factory reset failed: ${e}`);
    }
  };

  const handleCreateMode = async () => {
    if (!newModeId || !newModeLabel) return;
    try {
      await api.createMode(deviceUrl, {
        id: newModeId,
        label: newModeLabel,
        cycleOrder: modes.length,
        bindings: [],
      });
      addWebLog("CONFIG", `Mode created: ${newModeId} (${newModeLabel})`);
      setNewModeOpen(false);
      setNewModeId("");
      setNewModeLabel("");
      fetchModes();
    } catch (e) {
      console.error(e);
      addWebLog("ERROR", `Mode creation failed: ${e}`);
    }
  };

  if (!connected) {
    return (
      <div className="memphis-bg flex flex-col items-center justify-center rounded-xl border-2 border-dashed border-border px-6 py-16 text-center backdrop-blur-[6px]">
        <Layers size={48} className="mb-4 text-text-muted" />
        <h2 className="mb-2 text-xl font-extrabold">No Device Connected</h2>
        <p className="text-sm text-text-muted">
          Enter your WalKEY-TalKEY device URL in the header to get started.
        </p>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Modes Section */}
      <div>
        <div className="mb-4 flex items-center gap-3">
          <Layers size={20} className="text-accent" />
          <h2 className="section-title text-accent">Modes</h2>
          <div className="ml-auto flex gap-2">
            <button
              className="btn btn-sm btn-ghost"
              onClick={() => setNewModeOpen(!newModeOpen)}
            >
              <Plus size={14} className="mr-1" />
              New Mode
            </button>
            <button
              className="btn btn-sm btn-danger"
              onClick={handleReset}
              title="Factory reset"
            >
              <RotateCcw size={14} />
            </button>
          </div>
        </div>

        {newModeOpen && (
          <div className="card mb-4 border-highlight">
            <h3 className="mb-3 text-sm font-bold uppercase tracking-wider text-highlight">
              Create New Mode
            </h3>
            <div className="flex flex-col gap-2 sm:flex-row">
              <input
                className="input flex-1"
                placeholder="Mode ID (e.g. gaming)"
                value={newModeId}
                onChange={(e) => setNewModeId(e.target.value)}
              />
              <input
                className="input flex-1"
                placeholder="Label (e.g. Gaming)"
                value={newModeLabel}
                onChange={(e) => setNewModeLabel(e.target.value)}
              />
              <button
                className="btn btn-highlight"
                onClick={handleCreateMode}
                disabled={!newModeId || !newModeLabel}
              >
                Create
              </button>
            </div>
          </div>
        )}

        {loading ? (
          <div className="flex items-center justify-center py-8">
            <Loader2 className="animate-spin text-primary" size={24} />
          </div>
        ) : (
          <div className="space-y-3">
            {modes
              .sort((a, b) => a.cycleOrder - b.cycleOrder)
              .map((m) => (
                <ModeCard
                  key={m.id}
                  summary={m}
                  onDeleted={fetchModes}
                />
              ))}
          </div>
        )}
      </div>

      {/* Defaults */}
      <DefaultsEditor />

      {/* Wi-Fi */}
      <WifiDisplay />

      {/* Advanced JSON Editor */}
      <div className="card">
        <button
          type="button"
          className="flex w-full items-center gap-2 text-left"
          onClick={handleOpenAdvanced}
        >
          <Code2 size={20} className="text-highlight" />
          <h2 className="section-title text-highlight">Advanced JSON</h2>
          <span className="ml-auto text-xs text-text-muted">
            {showAdvanced ? "Close" : "Open raw editor"}
          </span>
        </button>

        {showAdvanced && (
          <div className="mt-4 space-y-3">
            <div className="overflow-hidden rounded-lg border-2 border-border">
              <MonacoEditor
                height="500px"
                language="json"
                theme="walkey-dark"
                value={rawConfig}
                onChange={(v) => setRawConfig(v ?? "")}
                beforeMount={handleEditorWillMount}
                options={{
                  minimap: { enabled: false },
                  fontSize: 13,
                  wordWrap: "on",
                  scrollBeyondLastLine: false,
                  fontFamily: '"Fira Code", "Fira Mono", Menlo, Consolas, monospace',
                }}
              />
            </div>
            {rawValidation && (
              <pre className="max-h-32 overflow-auto rounded-lg bg-surface-raised p-3 text-xs">
                {rawValidation}
              </pre>
            )}
            <div className="flex gap-2">
              <button className="btn btn-ghost flex-1" onClick={handleValidate}>
                Validate
              </button>
              <button
                className="btn btn-highlight flex-1"
                onClick={handleRawSave}
                disabled={rawSaving}
              >
                {rawSaving ? "Saving..." : "Save Full Config"}
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
