# Mode Config Schema

## Useful Information for Humans

- `mode-config.schema.json` is the machine-readable contract for the editable mode configuration.
- It documents the preferred array-based `modes` shape with stable `id` values while still describing the legacy object form for compatibility.
- UI/API tooling should validate against this schema before sending JSON to the device.
- The schema is not just for validation: it is also the best compact reference for which JSON action forms are supported.

### For Users

- Prefer the example config in `mode-config.json` when learning by copying working patterns.
- Think of each binding's `actions` array as a macro that runs in order.
- Prefer `hid_shortcut_tap` with `modifiers` plus `key` for keyboard shortcuts, and `hid_usage_*` for media/system HID.

### For Developers

- Add field descriptions here when the macro system evolves so editors and future UI/API tooling surface the same guidance as the prose docs.
- Keep the schema focused on the canonical JSON forms even when the firmware still accepts compatibility forms.

## Useful Information for AI

- Keep this schema aligned with `mode_json_loader.c`, `config/mode-config.json`, and `docs/mode-system-reference.md`.
- Prefer expanding shared action/usage definitions here instead of inventing undocumented JSON fields in examples.
- When the firmware adds new HID aliases or action types, update the schema and loader tests in the same change.
