# AI Guide

## Useful Information for Humans

- `AI_GUIDE.md` is the standalone AI-focused authoring guide for generating valid, feature-rich mode configs for this firmware.
- It is intended to be sufficient on its own for an AI agent to generate a full config from a user prompt.
- It should stay aligned with `config/mode-config.schema.json`, `config/mode-config.json`, `docs/mode-system-reference.md`, and `gesture-support-status.md`.

## Useful Information for AI

- Keep this file short and focused on how `AI_GUIDE.md` should be maintained.
- If the schema, shipped trigger set, or Wi-Fi defaults change, review `AI_GUIDE.md` in the same pass and keep it self-sufficient.
- Prefer describing canonical JSON and shipped behavior rather than speculative future capabilities.
