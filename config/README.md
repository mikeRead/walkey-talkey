# Config Files

## Useful Information for Humans

- `mode-config.json` is the editable example mode-system JSON file.
- `mode-config.schema.json` is the machine-readable contract for validation and future UI/API tooling.
- The firmware first tries to load `/spiffs/mode-config.json` from the `storage` SPIFFS partition.
- If the external JSON file is missing or invalid, the firmware falls back to the built-in copy compiled into `main/mode_config.c`.
- The device now also exposes its own local config portal over Wi-Fi with a tiny web UI and REST endpoints for export, validation, save, and reset.
- The portal also lets users download `mode-config.schema.json` and `docs/USER_GUIDE.md` directly from the device UI.
- The portal intentionally starts a few seconds after boot so the board UI stays stable before Wi-Fi comes up.
- Save/reset storage failures now return structured diagnostics instead of a generic `STORAGE_FAILED` message. The response includes the failure stage (`mount`, `open`, `write`, or `flush`), `formatAttempted`, the target path, the partition label, the low-level ESP/errno details when available, and suggestions for likely recovery steps.
- The most common outside-of-code causes are a blank or corrupted `storage` SPIFFS partition, a partition table mismatch, or a filesystem that needs to be reformatted after earlier flashes.
- Prefer the array-based `modes` format with stable `id` values and explicit `cycleOrder`.
- Prefer `modifiers` arrays over the compatibility-only singular `modifier` field.
- Keep the example JSON, schema, and built-in fallback in sync when you change mode bindings or add supported actions.

### Macro System Quick Guide

- A binding is one `input` plus one `trigger` plus an ordered `actions` array.
- The `actions` array is the macro system. The firmware executes it from top to bottom.
- `hid_key_tap`, `hid_shortcut_tap`, and `hid_usage_tap` already include the firmware's built-in tap gap.
- Use `sleep_ms` only when you need extra delay between steps in a longer macro.
- If one action fails at runtime, later actions in that same binding are skipped.
- The runtime can dispatch at most `8` bindings for one input+trigger after global bindings are combined with the current mode or `bootMode`.
- Prefer `hid_shortcut_tap` with `modifiers` plus `key` for keyboard chords.
- Prefer `hid_usage_*` for consumer/system HID such as media and volume controls.

### Which File To Read

- End users editing JSON should start with `mode-config.json`.
- End users using the device and portal should start with `../docs/USER_GUIDE.md`.
- AI systems generating or repairing configs should start with `../docs/AI_GUIDE.md`.
- Developers defining fields, validation, or UI/API tooling should start with `mode-config.schema.json`.
- API/editor code that needs canonical output should use the firmware-aligned canonical JSON exporter instead of preserving compatibility-only input shapes.
- API/UI code that resets config should treat the built-in JSON as the normal reset target and the hardcoded failsafe config as an internal last-resort fallback.
- Anyone who needs exact runtime semantics should read `docs/mode-system-reference.md`.

## Useful Information for AI

- Treat `config/mode-config.json` as the human-editable source example for the runtime JSON loader.
- Treat `config/mode-config.schema.json` as the UI/API-facing contract.
- Keep the JSON declarative: bindings are `input` + `trigger` + ordered `actions`.
- Prefer updating this file alongside `config/mode-config.schema.json`, `docs/mode-system-reference.md`, `gesture-support-status.md`, and the built-in fallback string in `main/mode_config.c`.
