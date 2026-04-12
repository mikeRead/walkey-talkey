"use client";

import type { Binding, Action } from "@/types/config";
import {
  INPUT_TYPES,
  TOUCH_TRIGGERS,
  BUTTON_TRIGGERS,
} from "@/types/config";
import { ActionEditor } from "./action-editor";
import { Plus, Trash2 } from "lucide-react";
import { triggerLabel } from "@/lib/utils";

interface BindingEditorProps {
  binding: Binding;
  onChange: (binding: Binding) => void;
  onRemove: () => void;
}

export function BindingEditor({
  binding,
  onChange,
  onRemove,
}: BindingEditorProps) {
  const triggers =
    binding.input === "boot_button" ? BUTTON_TRIGGERS : TOUCH_TRIGGERS;

  const updateAction = (index: number, action: Action) => {
    const actions = [...binding.actions];
    actions[index] = action;
    onChange({ ...binding, actions });
  };

  const removeAction = (index: number) => {
    const actions = binding.actions.filter((_, i) => i !== index);
    onChange({ ...binding, actions });
  };

  const addAction = () => {
    onChange({
      ...binding,
      actions: [...binding.actions, { type: "noop" }],
    });
  };

  return (
    <div className="rounded-xl border-2 border-border bg-surface-raised p-4">
      <div className="mb-3 flex flex-col gap-2 sm:flex-row sm:items-center">
        <select
          className="select text-sm sm:w-36"
          value={binding.input}
          onChange={(e) =>
            onChange({
              ...binding,
              input: e.target.value,
              trigger: e.target.value === "boot_button" ? "press" : "tap",
            })
          }
        >
          {INPUT_TYPES.map((i) => (
            <option key={i} value={i}>
              {i === "touch" ? "Touch" : "Boot Button"}
            </option>
          ))}
        </select>

        <select
          className="select text-sm sm:w-40"
          value={binding.trigger}
          onChange={(e) => onChange({ ...binding, trigger: e.target.value })}
        >
          {triggers.map((t) => (
            <option key={t} value={t}>
              {triggerLabel(t)}
            </option>
          ))}
        </select>

        <div className="ml-auto">
          <button
            type="button"
            className="btn btn-sm btn-danger"
            onClick={onRemove}
            title="Remove binding"
          >
            <Trash2 size={14} />
            <span className="ml-1 hidden sm:inline">Remove</span>
          </button>
        </div>
      </div>

      <div className="space-y-2">
        <div className="text-xs font-bold uppercase tracking-wider text-text-muted">
          Actions
        </div>
        {binding.actions.map((action, i) => (
          <ActionEditor
            key={i}
            action={action}
            onChange={(a) => updateAction(i, a)}
            onRemove={() => removeAction(i)}
          />
        ))}
        <button
          type="button"
          className="btn btn-sm btn-ghost w-full"
          onClick={addAction}
        >
          <Plus size={14} className="mr-1" />
          Add Action
        </button>
      </div>
    </div>
  );
}
