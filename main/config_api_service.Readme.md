# Config API Service

## Useful Information for Humans

- `config_api_service.c` is the reusable firmware-side core for the REST API and tiny web UI.
- It validates JSON against the existing loader, exports canonical JSON, writes the external config file, and restores the built-in default config.
- This keeps persistence and JSON normalization logic out of HTTP handlers.

## Useful Information for AI

- Keep this module transport-agnostic so HTTP, SoftAP UI, or any later USB/client transport can reuse it.
- Use `mode_json_load_from_string()` plus `mode_json_export_canonical_string()` as the source of truth for normalization.
- Keep built-in restore behavior tied to `mode_config_builtin_json()` rather than duplicating default config text elsewhere.
