# Main Module Notes

## Useful Information for Humans

- `main.c` wires together the mode system, UI, composite USB device, microphone input, and BOOT button input.
- `mode_config.c` contains the built-in declarative mode table for `Cursor`, `Presentation`, `Media`, and `Navigation`.
- `mode_controller.c` tracks the active mode plus temporary `boot_mode` state while `BOOT` is held.
- `input_router.c` normalizes raw BOOT and touch events into triggers such as `tap`, `hold_start`, `swipe_left`, and `swipe_right`.
- `action_engine.c` executes built-in actions such as mode changes, HID key events, mic-gate toggles, and UI refresh requests.
- `ptt_state.c` still exists as a small host-testable PTT transition state machine for the dictation workflow.
- `audio_input.c` initializes the ES7210 microphone path and reads PCM frames through `esp_codec_dev`.
- `boot_button.c` handles GPIO0 polling plus debounce and emits clean press/release callbacks.
- `usb_composite.c` owns the composite USB HID + microphone descriptors, TinyUSB callbacks, and function-key press/release reports.
- `ui_status.c` owns the current status UI, swipe detection, BOOT overlay, and keeps LVGL updates behind BSP display locks.

## Useful Information for AI

- Keep BOOT button logic in `boot_button.*` instead of reintroducing GPIO polling into `main.c`.
- Keep mode-selection and binding logic in `mode_controller.*`, `input_router.*`, and `action_engine.*` instead of reintroducing special-case branches into `main.c`.
- Keep transition-only dictation logic in `ptt_state.*` so it stays host-testable and free of LVGL, TinyUSB, and BSP dependencies.
- Keep microphone capture details inside `audio_input.*` rather than reading the codec directly from `main.c`.
- Keep TinyUSB descriptor and callback code inside `usb_composite.c` so later USB composite/audio work can extend it without touching UI code.
- Keep touchscreen gesture timing and transient swipe-indicator rendering localized to `ui_status.c` and report only high-level touch events back to `main.c`.
- Any LVGL updates from non-display tasks must continue to use `bsp_display_lock()` and `bsp_display_unlock()`.
