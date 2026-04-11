# Docs

## Useful Information for Humans

- `lvgl-gesture-reference.md` explains which touch events and gestures LVGL supports natively.
- `AI_GUIDE.md` is the standalone AI-oriented guide for generating valid, feature-rich mode configs from a user prompt without depending on other docs.
- `USER_GUIDE.md` is the end-user guide for device setup, portal access, save/reset behavior, and recovery steps.
- `mode-system-reference.md` documents the mode-driven controller design, including the dedicated `BOOT` mode-switch role and example JSON configurations. Treat it as the target/reference model, not a strict snapshot of every current UI detail.
- `mode-system-reference.md` is also the main behavior/spec document for the JSON macro system, including action ordering, tap timing, and preferred HID JSON forms.
- `../config/mode-config.json` is the practical authoring example for users.
- `../config/mode-config.schema.json` is the machine-readable contract for editors, validators, and future UI/API tooling.
- `../gesture-support-status.md` tracks which normalized triggers and mode behaviors are actually implemented in firmware right now.
- It also calls out which interactions are typically implemented in app logic rather than provided directly by LVGL.
- This folder is for short technical references that support the firmware and UI behavior.

## Useful Information for AI

- Keep docs in this folder concise and practical.
- Keep `AI_GUIDE.md` standalone, comprehensive, and aligned with the shipped trigger set, schema contract, and current Wi-Fi defaults.
- Keep `USER_GUIDE.md` focused on operator-facing setup and troubleshooting rather than internal implementation details.
- Prefer documenting current project behavior and framework-native capabilities over generic theory.
- Keep `../gesture-support-status.md` aligned with the shipped trigger set when the mode engine changes.
- When touch behavior changes, update `lvgl-gesture-reference.md` only if the LVGL usage model or guidance changes.
- Keep `mode-system-reference.md` aligned with the public config model and example workflows when the mode engine evolves, but call out where the shipped UI is intentionally simpler than the reference.
