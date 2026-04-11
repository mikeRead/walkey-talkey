# Mode System Reference

## Useful Information for Humans

This firmware exposes a mode-driven input system for the ESP32-S3 Touch AMOLED device.

The device is no longer modeled as a dedicated push-to-talk appliance. Instead, it behaves as a configurable touch controller with:

- a global mode-switch button on `BOOT`
- a temporary `boot_mode` control layer while `BOOT` is held
- LVGL-backed touch gestures normalized into high-level triggers
- per-mode action bindings
- a small built-in action engine for HID, microphone, and UI behavior

The `BOOT` button is reserved for mode control and does not participate in PTT. While held, it activates a dedicated mode-control UI and gesture surface.

## Useful Information for AI

- Treat `BOOT` as a global control input that can activate a temporary `boot_mode`.
- Treat LVGL events as implementation details behind normalized triggers such as `tap`, `hold_start`, and `swipe_left`.
- Prefer describing configuration in terms of `globalBindings`, `bootMode`, `modes`, `bindings`, and `actions`.
- Keep JSON declarative. Do not turn config into an embedded scripting language.

## Overview

The firmware uses five layers:

1. `input layer`
   Raw device sources such as `boot_button` and `touch`.
2. `trigger layer`
   Normalized events such as `tap`, `hold_start`, `hold_end`, `swipe_left`, and `swipe_right`.
3. `mode layer`
   A current mode selects which bindings are active.
4. `boot control layer`
   A temporary `boot_mode` overrides normal touch bindings while `BOOT` is held.
5. `action layer`
   Built-in actions execute HID reports, microphone gating, UI updates, and mode changes.

This keeps LVGL-specific details isolated from user-facing configuration.

## Load Priority

The runtime load order is:

1. external JSON from `/spiffs/mode-config.json`
2. built-in fallback JSON compiled into firmware
3. hardcoded failsafe config if JSON parsing fails

This keeps the user-editable file format and the shipped default behavior aligned.

## Config Portal

The firmware can expose its local config portal over the router network or, if needed, a fallback device-hosted access point and HTTP server.

Current intended behavior:

- the device first attempts STA join using the JSON-configured router SSID and password
- when STA join succeeds, the portal is reachable at the assigned router IP and by default at `http://walkey-talkey.local/`
- if STA join fails, the firmware falls back to a SoftAP at `http://192.168.4.1/`
- the fallback SoftAP uses SSID `walkey-talkey` with password `secretKEY`
- the portal intentionally starts about 8 seconds after boot so the display/touch stack can settle before Wi-Fi startup
- the same server exposes REST endpoints for config export, validation, save, and reset
- successful save operations normalize accepted JSON into the canonical output form before writing it back to `/spiffs/mode-config.json`
- save and reset both reload runtime state and immediately reapply the Wi-Fi configuration
- reset restores the built-in firmware JSON as the active external config source, then reloads runtime state from that restored config
- the hardcoded failsafe config still exists, but it is an internal last-resort fallback rather than the normal API reset target
- save/reset failures return a structured `STORAGE_FAILED` payload with `stage`, `formatAttempted`, `path`, `partition`, optional low-level error fields, and recovery suggestions
- the BOOT overlay shows `Connecting...` immediately during portal startup, then replaces it with the active portal address on the line directly below `Swipe to switch mode`
- in AP fallback mode, the BOOT overlay uses the format `AP: walkey-talkey (<ip>)`

Implementation notes for the current board build:

- Wi-Fi startup is intentionally delayed because bringing up the radio too early on this hardware was corrupting the AMOLED/touch UI
- the display driver now uses smaller internal-RAM LVGL draw buffers instead of the earlier PSRAM-backed configuration to keep the UI stable while Wi-Fi is active

## Core Behavior

The device behaves as follows:

- pressing and holding `BOOT` enters `boot_mode`
- while `boot_mode` is active, touch gestures are routed to the dedicated mode-control bindings instead of the current app mode
- `BOOT + swipe_right` moves to the next mode
- `BOOT + swipe_left` moves to the previous mode
- the display shows a dedicated control overlay while `boot_mode` is active
- when `BOOT` is released, the control overlay closes and normal mode bindings resume
- touch gestures are interpreted by LVGL and normalized before they reach the binding engine
- actions are executed in order, allowing one gesture to drive multiple outputs

## Why The System Uses Normalized Triggers

LVGL provides the low-level event model, but the configuration layer does not expose raw LVGL event names directly.

For example:

- LVGL raises `LV_EVENT_GESTURE`, then the firmware reads the direction and converts it into `swipe_up`, `swipe_down`, `swipe_left`, or `swipe_right`
- LVGL raises press and release events, and the firmware converts hold behavior into `hold_start` and `hold_end`

This gives the config format stable names even if gesture handling evolves internally.

## Recommended JSON Shape

The mode system uses one top-level config with global bindings, a dedicated `bootMode`, and mode-specific bindings.

```json
{
  "version": 1,
  "activeMode": "cursor",
  "defaults": {
    "touch": {
      "holdMs": 400,
      "doubleTapMs": 350,
      "swipeMinDistance": 40
    }
  },
  "globalBindings": [
    {
      "input": "boot_button",
      "trigger": "press",
      "actions": [
        { "type": "enter_boot_mode" }
      ]
    },
    {
      "input": "boot_button",
      "trigger": "release",
      "actions": [
        { "type": "exit_boot_mode" }
      ]
    }
  ],
  "bootMode": {
    "label": "Mode Control",
    "ui": {
      "title": "Mode Control",
      "showModeList": true,
      "showGestureHints": true
    },
    "bindings": [
      {
        "input": "touch",
        "trigger": "swipe_right",
        "actions": [
          { "type": "cycle_mode", "direction": "next" },
          { "type": "ui_show_mode" }
        ]
      },
      {
        "input": "touch",
        "trigger": "swipe_left",
        "actions": [
          { "type": "cycle_mode", "direction": "previous" },
          { "type": "ui_show_mode" }
        ]
      }
    ]
  },
  "modes": [
    {
      "id": "cursor",
      "cycleOrder": 0,
      "label": "Cursor",
      "bindings": []
    }
  ]
}
```

## Top-Level Fields

- `version`
  Config format version for migration safety.
- `activeMode`
  Default mode selected at boot.
- `defaults`
  Shared thresholds and timing values.
- `globalBindings`
  Bindings that are always active, regardless of mode.
- `bootMode`
  A temporary dedicated control mode entered while `BOOT` is held.
- `modes`
  Per-mode definitions. Prefer the array form with stable `id` values and explicit `cycleOrder`. The loader still accepts the older object-map form for compatibility.

## Inputs

The built-in input sources are:

- `boot_button`
- `touch`

The model also leaves room for future sources:

- `encoder`
- `usb_host_key`
- `timer`
- `imu`

## Triggers

The firmware exposes a normalized trigger vocabulary:

- `press`
- `release`
- `tap`
- `double_tap`
- `long_press`
- `hold_start`
- `hold_end`
- `swipe_up`
- `swipe_down`
- `swipe_left`
- `swipe_right`

Not every input source supports every trigger. For example, `boot_button` commonly uses `press`, `release`, and `long_press`, while `touch` supports the broader gesture set.

Current touch semantics:

- `tap` is deferred until the `doubleTapMs` timeout expires
- a second tap inside that timeout emits `double_tap` instead of a second `tap`
- `long_press` and `hold_start` are emitted together when the hold threshold is crossed, in that order
- `hold_end` is emitted on release after a hold has started

## Actions

The action engine uses fixed action types implemented in firmware.

Common actions include:

- `hid_key_down`
- `hid_key_up`
- `hid_key_tap`
- `hid_shortcut_tap`
- `hid_modifier_down`
- `hid_modifier_up`
- `hid_usage_down`
- `hid_usage_up`
- `hid_usage_tap`
- `sleep_ms`
- `enter_boot_mode`
- `exit_boot_mode`
- `mic_gate`
- `mic_gate_toggle`
- `ui_hint`
- `ui_show_mode`
- `set_mode`
- `cycle_mode`
- `noop`

Actions are intentionally limited to known built-in behaviors so the JSON stays easy to validate and debug. `sleep_ms` is the intended timing primitive for short, reliable keyboard macro gaps without turning the config into a scripting language.

Preferred HID payload rules:

- Use canonical key tokens like `A`, `ENTER`, `F13`, `MEDIA_NEXT_TRACK`, `VOLUME_UP`.
- For keyboard chords, prefer `hid_shortcut_tap` or `hid_key_tap` plus a `modifiers` array.
- For consumer/system HID, use `hid_usage_*` actions.
- For advanced cases, use a raw usage object with `report`, `usagePage`, and `usage`.
- The machine-readable schema lives at `config/mode-config.schema.json`.

## Binding Shape

Each binding matches one `input` plus one `trigger`, then executes one or more `actions`.

```json
{
  "input": "touch",
  "trigger": "swipe_left",
  "actions": [
    { "type": "hid_key_tap", "key": "LEFT_ARROW" }
  ]
}
```

Structured HID examples:

```json
{
  "type": "hid_shortcut_tap",
  "modifiers": ["CTRL", "SHIFT"],
  "key": "A"
}
```

```json
{
  "type": "hid_usage_tap",
  "usage": "MEDIA_NEXT_TRACK"
}
```

```json
{
  "type": "hid_usage_tap",
  "usage": {
    "report": "consumer",
    "usagePage": 12,
    "usage": 205
  }
}
```

This structure scales better than embedding logic in event names or creating separate `on` and `off` maps.

Runtime expectation:

- swipe bindings should behave like edge-triggered one-shots and fire once per gesture
- if a macro needs a small timing gap between steps, use an explicit `sleep_ms` action instead of relying on repeated trigger delivery
- `actions` arrays are the supported macro model; execution order is exactly the array order

## Macro Behavior

The JSON config does not embed a scripting language. Instead, every binding uses an ordered `actions` array, and that array is the macro.

Important runtime semantics:

- the action engine executes one action at a time from the first array entry to the last
- later actions do not start until the current action finishes
- `hid_key_tap`, `hid_shortcut_tap`, and `hid_usage_tap` already include a built-in press-to-release gap inside the firmware
- the current built-in tap gap is `20 ms`
- `sleep_ms` is only for extra spacing between steps in a longer macro
- `sleep_ms` with `duration_ms: 0` is a no-op
- if any action fails at runtime, the engine stops that binding immediately and skips the remaining steps
- if a binding fails while the app is dispatching matched bindings for the same event, later matched bindings for that event are not run
- the runtime collects at most `8` matching bindings for a single input+trigger dispatch (`globalBindings` plus the active mode or `bootMode`); configs that exceed that fan-out should be treated as invalid for editor/API output

Practical meaning:

- use a single `hid_*_tap` action when you only need one press-and-release
- add `sleep_ms` only when you need additional delay between multiple actions
- do not assume partial rollback; if step 3 fails, steps 4+ will not run, and steps 1-2 are not automatically undone

### Output Reset Behavior

Some actions intentionally reset currently active outputs before changing higher-level state:

- `enter_boot_mode`
- `set_mode`
- `cycle_mode`

In the current firmware, that reset path is used to keep transitions deterministic. It clears active HID output state, turns off mic gating, and cancels in-progress touch routing before the new mode state takes effect.

This means mode-changing actions should be treated as boundaries in a macro. If a sequence needs to keep holding a key or preserve mic state, do that before the mode change only when the reset behavior is acceptable.

## Writing Macros In JSON

For users, the simplest way to think about the system is:

1. choose an `input`
2. choose a `trigger`
3. list the `actions` in the exact order you want them to happen

The preferred JSON forms are:

- use `hid_key_tap` for a single keyboard key
- use `hid_shortcut_tap` with `modifiers` plus `key` for keyboard chords
- use `hid_usage_*` for consumer or system HID
- use `sleep_ms` only when the built-in tap timing is not enough
- use `set_mode`, `cycle_mode`, `ui_show_mode`, `ui_hint`, and `mic_gate` as normal array entries when a macro should mix HID and device behavior

Examples:

Single action:

```json
{
  "input": "touch",
  "trigger": "tap",
  "actions": [
    { "type": "hid_key_tap", "key": "ENTER" }
  ]
}
```

Shortcut tap:

```json
{
  "input": "touch",
  "trigger": "swipe_left",
  "actions": [
    { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "N" }
  ]
}
```

Multi-step macro with timing gap:

```json
{
  "input": "touch",
  "trigger": "swipe_up",
  "actions": [
    { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "A" },
    { "type": "sleep_ms", "duration_ms": 20 },
    { "type": "hid_key_tap", "key": "BACKSPACE" }
  ]
}
```

Mode change plus UI feedback:

```json
{
  "input": "touch",
  "trigger": "long_press",
  "actions": [
    { "type": "set_mode", "mode": "cursor" },
    { "type": "ui_show_mode" }
  ]
}
```

Hold/release pairing:

```json
{
  "input": "touch",
  "trigger": "hold_start",
  "actions": [
    { "type": "hid_key_down", "key": "F13" },
    { "type": "mic_gate", "enabled": true }
  ]
}
```

```json
{
  "input": "touch",
  "trigger": "hold_end",
  "actions": [
    { "type": "mic_gate", "enabled": false },
    { "type": "hid_key_up", "key": "F13" }
  ]
}
```

## BOOT Button Behavior

The `BOOT` button is dedicated to mode control and activates its own temporary `boot_mode`.

The default flow is:

- `press` -> enter `boot_mode`
- `BOOT + swipe_right` -> next mode
- `BOOT + swipe_left` -> previous mode
- `BOOT + tap` -> show the currently selected mode
- `release` -> exit `boot_mode`

This keeps mode switching consistent and prevents application modes from fighting over the hardware button. It also gives the user one predictable gesture vocabulary for mode navigation no matter which mode is currently active.

Example:

```json
{
  "globalBindings": [
    {
      "input": "boot_button",
      "trigger": "press",
      "actions": [
        { "type": "enter_boot_mode" }
      ]
    },
    {
      "input": "boot_button",
      "trigger": "release",
      "actions": [
        { "type": "exit_boot_mode" }
      ]
    }
  ],
  "bootMode": {
    "label": "Mode Control",
    "bindings": [
      {
        "input": "touch",
        "trigger": "swipe_right",
        "actions": [
          { "type": "cycle_mode", "direction": "next" },
          { "type": "ui_show_mode" }
        ]
      },
      {
        "input": "touch",
        "trigger": "swipe_left",
        "actions": [
          { "type": "cycle_mode", "direction": "previous" },
          { "type": "ui_show_mode" }
        ]
      },
      {
        "input": "touch",
        "trigger": "tap",
        "actions": [
          { "type": "ui_show_mode" }
        ]
      },
      {
        "input": "touch",
        "trigger": "long_press",
        "actions": [
          { "type": "set_mode", "mode": "cursor" },
          { "type": "ui_show_mode" }
        ]
      }
    ]
  }
}
```

## Dedicated BOOT Control UI

While `BOOT` is held, the screen switches to a dedicated control overlay instead of showing only the active mode UI.

For the current firmware build, this overlay is intentionally simpler than the fuller reference layout below. What is currently shipped is:

- top instruction text: `Swipe to switch mode`
- bottom confirm hint: `Release BOOT = Confirm`
- mode-selection feedback primarily through the BOOT-colored main card state rather than centered previous/current/next mode labels

The richer layout described below remains a valid reference direction if the BOOT UI is expanded later.

If the BOOT UI is expanded later, the `boot_mode` overlay can show:

- the currently selected mode name
- the previous and next mode names
- a simple gesture legend
- optional icons for common control actions

A richer reference layout is:

- center: current mode card
- left edge hint: `Swipe Left = Previous`
- right edge hint: `Swipe Right = Next`
- bottom hint: `Release BOOT = Confirm`

This gives the user direct on-device feedback that touch input is temporarily acting as a mode selector rather than as a mode-specific command surface.

Example UI block:

```json
{
  "bootMode": {
    "label": "Mode Control",
    "ui": {
      "title": "Mode Control",
      "subtitle": "Hold BOOT and swipe to change modes",
      "showModeList": true,
      "showGestureHints": true,
      "showCurrentModeCard": true
    }
  }
}
```

## Example: Cursor Dictation Mode

This mode mirrors the intent of the AutoHotkey `CB_mic_v3.ahk` workflow, where one mode is optimized for dictation and Cursor interaction.

In the AutoHotkey script, the `Cursor` mode maps one button to dictation and hold behavior, while the other button handles chat and field control. The firmware version keeps the same idea, but moves interaction to touch gestures and reserves `BOOT` for global mode switching.

Example configuration:

```json
{
  "modes": {
    "cursor": {
      "label": "Cursor",
      "bindings": [
        {
          "input": "touch",
          "trigger": "hold_start",
          "actions": [
            { "type": "hid_key_down", "key": "F13" },
            { "type": "mic_gate", "enabled": true },
            { "type": "ui_hint", "text": "Dictation active" }
          ]
        },
        {
          "input": "touch",
          "trigger": "hold_end",
          "actions": [
            { "type": "mic_gate", "enabled": false },
            { "type": "hid_key_up", "key": "F13" },
            { "type": "ui_hint", "text": "Cursor mode" }
          ]
        },
        {
          "input": "touch",
          "trigger": "tap",
          "actions": [
            { "type": "hid_key_tap", "key": "F14" },
            { "type": "ui_hint", "text": "Cursor mode" }
          ]
        },
        {
          "input": "touch",
          "trigger": "double_tap",
          "actions": [
            { "type": "hid_key_tap", "key": "ENTER" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_up",
          "actions": [
            { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "A" },
            { "type": "sleep_ms", "duration_ms": 20 },
            { "type": "hid_key_tap", "key": "BACKSPACE" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_down",
          "actions": [
            { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "PERIOD" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_left",
          "actions": [
            { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "N" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_right",
          "actions": [
            { "type": "hid_key_tap", "key": "ENTER" }
          ]
        }
      ]
    }
  }
}
```

Useful interpretation:

- hold starts and stops the same `F13`-driven dictation flow used by the host workflow
- tap sends `F14`
- double tap sends `Enter`
- swipe up clears the current field
- swipe down toggles Cursor text mode
- swipe left sends new chat
- swipe right sends enter

## Example: Presentation Remote Mode

This mode turns the device into a slide controller.

```json
{
  "modes": {
    "presentation": {
      "label": "Presentation",
      "bindings": [
        {
          "input": "touch",
          "trigger": "swipe_left",
          "actions": [
            { "type": "hid_key_tap", "key": "PAGE_DOWN" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_right",
          "actions": [
            { "type": "hid_key_tap", "key": "PAGE_UP" }
          ]
        },
        {
          "input": "touch",
          "trigger": "tap",
          "actions": [
            { "type": "hid_key_tap", "key": "SPACE" }
          ]
        },
        {
          "input": "touch",
          "trigger": "double_tap",
          "actions": [
            { "type": "hid_key_tap", "key": "B" }
          ]
        }
      ]
    }
  }
}
```

Useful interpretation:

- swipe left advances
- swipe right goes back
- tap starts or advances a deck
- double tap blacks the screen in many presentation apps

## Example: Media Control Mode

This mode is useful at a desk, workbench, or streaming setup.

The current firmware still uses keyboard-safe placeholder keys here because consumer/media HID usages are not yet wired through the USB report path.

```json
{
  "modes": {
    "media": {
      "label": "Media",
      "bindings": [
        {
          "input": "touch",
          "trigger": "tap",
          "actions": [
            { "type": "hid_key_tap", "key": "SPACE" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_left",
          "actions": [
            { "type": "hid_key_tap", "key": "LEFT_ARROW" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_right",
          "actions": [
            { "type": "hid_key_tap", "key": "RIGHT_ARROW" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_up",
          "actions": [
            { "type": "hid_key_tap", "key": "UP_ARROW" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_down",
          "actions": [
            { "type": "hid_key_tap", "key": "DOWN_ARROW" }
          ]
        }
      ]
    }
  }
}
```

## Example: CAD Or Editing Navigation Mode

This mode is useful for scroll, pan, and frequent tool shortcuts.

```json
{
  "modes": {
    "navigation": {
      "label": "Navigation",
      "bindings": [
        {
          "input": "touch",
          "trigger": "swipe_up",
          "actions": [
            { "type": "hid_key_tap", "key": "UP_ARROW" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_down",
          "actions": [
            { "type": "hid_key_tap", "key": "DOWN_ARROW" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_left",
          "actions": [
            { "type": "hid_key_tap", "key": "LEFT_ARROW" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_right",
          "actions": [
            { "type": "hid_key_tap", "key": "RIGHT_ARROW" }
          ]
        },
        {
          "input": "touch",
          "trigger": "double_tap",
          "actions": [
            { "type": "hid_key_tap", "key": "ESC" }
          ]
        }
      ]
    }
  }
}
```

## Global Versus Mode-Local Bindings

Use `globalBindings` for behaviors that should always work:

- entering and exiting `boot_mode`
- emergency mute
- returning to a home mode
- opening a mode picker

Use `bootMode` for temporary global control gestures while `BOOT` is held:

- next mode
- previous mode
- jump to a favorite mode
- show mode details

Use mode-local `bindings` for behaviors that should change by context:

- dictation controls
- slide navigation
- media commands
- editor shortcuts

## Suggested Storage Layout

For a small number of modes, one config file is enough.

For a larger setup, split config by concern:

- `config/manifest.json`
- `config/modes/cursor.json`
- `config/modes/presentation.json`
- `config/modes/media.json`
- `config/modes/navigation.json`

This makes mode sharing and per-mode editing easier.

For the current firmware build, the single-file example lives at `/spiffs/mode-config.json`, with a repo copy at `config/mode-config.json`.

## Design Rules

The most important rules for this system are:

- keep LVGL details behind normalized trigger names
- keep `BOOT` reserved for global control
- keep `boot_mode` temporary and visually obvious
- keep actions declarative and built-in
- let one gesture execute a short ordered list of actions
- keep mode switching outside individual modes

## Practical Summary

The mode engine turns the device into a reusable controller platform:

- `BOOT` enters a dedicated control mode
- `BOOT + swipe_right` changes to the next mode
- `BOOT + swipe_left` changes to the previous mode
- the display shows a dedicated mode-control overlay while `BOOT` is held
- touch performs mode-specific work
- LVGL stays the gesture backend
- JSON remains readable and extendable
- the same firmware image can support dictation, presentation, media, and navigation workflows
