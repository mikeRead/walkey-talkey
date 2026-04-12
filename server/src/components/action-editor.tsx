"use client";

import type { Action } from "@/types/config";
import {
  ACTION_TYPES,
  COMMON_KEYS,
  MODIFIERS,
  MEDIA_USAGES,
} from "@/types/config";
import { Trash2 } from "lucide-react";

interface ActionEditorProps {
  action: Action;
  onChange: (action: Action) => void;
  onRemove: () => void;
}

export function ActionEditor({ action, onChange, onRemove }: ActionEditorProps) {
  const meta = ACTION_TYPES.find((a) => a.value === action.type);

  const update = (field: string, value: unknown) => {
    onChange({ ...action, [field]: value });
  };

  const setType = (type: string) => {
    const base: Action = { type };
    const m = ACTION_TYPES.find((a) => a.value === type);
    if (m) {
      for (const f of m.fields) {
        if (f === "key") base.key = "";
        else if (f === "modifier") base.modifier = "";
        else if (f === "modifiers") base.modifiers = [];
        else if (f === "enabled") base.enabled = true;
        else if (f === "text") base.text = "";
        else if (f === "duration_ms") base.duration_ms = 20;
        else if (f === "mode") base.mode = "";
        else if (f === "mouseType") base.mouseType = "airMouse";
        else if (f === "tracking") base.tracking = true;
        else if (f === "usage") base.usage = "";
      }
    }
    onChange(base);
  };

  return (
    <div className="flex flex-col gap-2 rounded-lg border border-border/50 bg-bg p-3 sm:flex-row sm:items-start">
      <select
        className="select text-sm sm:w-40"
        value={action.type}
        onChange={(e) => setType(e.target.value)}
      >
        <option value="">Select type...</option>
        {ACTION_TYPES.map((a) => (
          <option key={a.value} value={a.value}>
            {a.label}
          </option>
        ))}
      </select>

      <div className="flex flex-1 flex-wrap gap-2">
        {meta?.fields.includes("key") && (
          <select
            className="select text-sm sm:w-36"
            value={(action.key as string) ?? ""}
            onChange={(e) => update("key", e.target.value)}
          >
            <option value="">Key...</option>
            {COMMON_KEYS.map((k) => (
              <option key={k} value={k}>
                {k}
              </option>
            ))}
          </select>
        )}

        {meta?.fields.includes("modifiers") && (
          <div className="flex gap-1">
            {MODIFIERS.map((mod) => {
              const mods = (action.modifiers as string[]) ?? [];
              const active = mods.includes(mod);
              return (
                <button
                  key={mod}
                  type="button"
                  className={`btn btn-sm ${active ? "btn-accent" : "btn-ghost"}`}
                  onClick={() =>
                    update(
                      "modifiers",
                      active ? mods.filter((m) => m !== mod) : [...mods, mod],
                    )
                  }
                >
                  {mod}
                </button>
              );
            })}
          </div>
        )}

        {meta?.fields.includes("modifier") && (
          <select
            className="select text-sm sm:w-32"
            value={(action.modifier as string) ?? ""}
            onChange={(e) => update("modifier", e.target.value)}
          >
            <option value="">Modifier...</option>
            {MODIFIERS.map((m) => (
              <option key={m} value={m}>
                {m}
              </option>
            ))}
          </select>
        )}

        {meta?.fields.includes("usage") && (
          <select
            className="select text-sm sm:w-48"
            value={typeof action.usage === "string" ? action.usage : ""}
            onChange={(e) => update("usage", e.target.value)}
          >
            <option value="">Usage...</option>
            {MEDIA_USAGES.map((u) => (
              <option key={u} value={u}>
                {u}
              </option>
            ))}
          </select>
        )}

        {meta?.fields.includes("enabled") && (
          <label className="flex items-center gap-2 text-sm">
            <input
              type="checkbox"
              checked={!!action.enabled}
              onChange={(e) => update("enabled", e.target.checked)}
              className="h-5 w-5 accent-primary"
            />
            Enabled
          </label>
        )}

        {meta?.fields.includes("text") && (
          <input
            className="input text-sm sm:w-48"
            value={(action.text as string) ?? ""}
            onChange={(e) => update("text", e.target.value)}
            placeholder="Hint text..."
          />
        )}

        {meta?.fields.includes("duration_ms") && (
          <input
            className="input text-sm sm:w-28"
            type="number"
            min={0}
            value={(action.duration_ms as number) ?? 20}
            onChange={(e) => update("duration_ms", Number(e.target.value))}
            placeholder="ms"
          />
        )}

        {meta?.fields.includes("mode") && (
          <input
            className="input text-sm sm:w-36"
            value={(action.mode as string) ?? ""}
            onChange={(e) => update("mode", e.target.value)}
            placeholder="Mode ID..."
          />
        )}

        {meta?.fields.includes("mouseType") && (
          <select
            className="select text-sm sm:w-36"
            value={(action.mouseType as string) ?? "airMouse"}
            onChange={(e) => update("mouseType", e.target.value)}
          >
            <option value="airMouse">Air Mouse</option>
            <option value="touchMouse">Touch Mouse</option>
          </select>
        )}

        {meta?.fields.includes("tracking") && (
          <label className="flex items-center gap-2 text-sm">
            <input
              type="checkbox"
              checked={action.tracking !== false}
              onChange={(e) => update("tracking", e.target.checked)}
              className="h-5 w-5 accent-primary"
            />
            Tracking
          </label>
        )}
      </div>

      <button
        type="button"
        className="btn btn-sm btn-danger self-start"
        onClick={onRemove}
        title="Remove action"
      >
        <Trash2 size={14} />
      </button>
    </div>
  );
}
