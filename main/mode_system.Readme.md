# Mode System Subsystem

## Useful Information for Humans

- `mode_types.h` defines the normalized config vocabulary shared by the mode system.
- `mode_config.c` now owns hybrid config loading: external JSON first, built-in JSON fallback second, hardcoded failsafe last.
- `mode_json_loader.c` compiles the JSON file shape into the runtime `mode_config_t` graph.
- `input_router.c` converts raw BOOT/touch input into stable triggers such as `tap`, `double_tap`, `long_press`, `hold_start`, and `swipe_left`.
- `mode_controller.c` owns `activeMode`, boot-mode state, and binding resolution.
- `action_engine.c` executes ordered built-in actions without hard-wiring mode logic into `main.c`.
- The currently shipped BOOT UI is intentionally minimal: top instruction text, bottom confirm text, and the centered active mode label remains visible on the BOOT-colored main card.
- The normal mode UI now uses the active mode as the main centered headline and shows a brief bottom swipe arrow for detected normal swipes.

## Useful Information for AI

- Keep these files ESP-IDF-light where possible so they stay host-testable.
- Treat `config/mode-config.json` and the built-in JSON in `mode_config.c` as paired sources of truth for the shipped default config.
- Keep LVGL and USB details out of `mode_controller.c`; integration-specific code should stay in `main.c`, `ui_status.c`, or `usb_composite.c`.
- When expanding the mode system, prefer adding new action types and normalized triggers here before adding special-case app branches.
- If the BOOT selector UI becomes richer again later, keep the config and control flow stable and treat the visual changes as a UI-layer concern.
