# HID Token Registry

## Useful Information for Humans

- `mode_hid_tokens.c` centralizes HID token parsing for keyboard, consumer, and system usages.
- This file replaces the scattered hand-coded key/modifier parsing logic that previously lived inside `mode_json_loader.c`.
- The goal is to keep one firmware-side source of truth for canonical token names and common aliases while still allowing raw usage-based JSON for advanced cases.
- The registry now also exposes catalog accessors and canonical-token helpers so future UI/API code can reuse the firmware vocabulary directly instead of duplicating it.

## Useful Information for AI

- Prefer adding new HID aliases and usage tokens here instead of extending parser `if` chains elsewhere.
- Keep the exported catalog metadata aligned with parser behavior so aliases normalize back to the same canonical token names.
- Keep the parsing helpers host-buildable and independent from ESP-IDF/TinyUSB-specific headers.
- If the JSON schema expands further, update this registry, the schema docs, and loader tests together.
