# Mode JSON Loader

## Useful Information for Humans

- `mode_json_loader.c` parses mode-system JSON into the runtime `mode_config_t` graph.
- `mode_config.c` owns hybrid loading: external `/spiffs/mode-config.json` first, built-in JSON fallback second, hardcoded failsafe last.
- The loader supports ordered action arrays, mode references for `set_mode`, shortcut strings like `CTRL+N`, and the current documented trigger vocabulary including `double_tap` and `long_press`.
- The loader is kept host-buildable so desktop tests can validate the JSON contract without ESP-IDF dependencies.

## Useful Information for AI

- Keep this subsystem pure C and heap-backed so it stays testable outside firmware builds.
- Prefer adding string-to-enum mappings here rather than special-casing behavior in `main.c`.
- If the JSON schema expands, update `mode_json_loader.c`, `config/mode-config.json`, and the docs together.
