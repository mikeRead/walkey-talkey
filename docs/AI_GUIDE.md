# Standalone AI Macro Config Guide

This document is a complete, standalone specification for AI systems that must generate advanced, feature-rich macro mode configurations for this firmware from a user's prompt.

Do not depend on any other document to understand the format, behavior, naming rules, or supported feature set. Everything needed to generate a high-quality config is described here.

The objective is to generate JSON that is:

- structurally valid
- behaviorally realistic
- safe to load through the web portal
- rich enough to express real workflows
- easy for a human to maintain after generation

## Primary Goal

Given a user's request, produce a complete or partial mode configuration that maps device inputs to ordered macro actions.

This firmware is not a scripting engine. It is a declarative macro system:

- one event comes in
- matching bindings are resolved
- each binding runs its `actions` array in order

The AI must generate valid declarative JSON, not pseudo-code.

## Hard Rules

These rules are mandatory:

- Do not invent top-level fields.
- Do not invent action types.
- Do not invent triggers.
- Do not invent inputs.
- Do not add metadata fields such as `description`, `notes`, `author`, or `generatedBy`.
- Do not remove BOOT recovery behavior unless the user explicitly asks for that risk.
- Do not output legacy compatibility shapes unless preserving an existing file requires it.
- Do not output giant noisy configs when a smaller one satisfies the prompt.

## Top-Level Config Shape

A valid config uses this overall shape:

```json
{
  "version": 1,
  "activeMode": "cursor",
  "defaults": {
    "touch": {
      "holdMs": 400,
      "doubleTapMs": 350,
      "swipeMinDistance": 40
    },
    "defaultMouse": "airMouse",
    "airMouse": {
      "sensitivity": 1.0,
      "deadZoneDps": 6.0,
      "easingExponent": 1.25,
      "maxDps": 300.0,
      "emaAlpha": 0.35,
      "rewindDepth": 12,
      "rewindDecay": 0.7,
      "calibrationSamples": 128
    },
    "touchMouse": {
      "sensitivity": 1.0,
      "moveThresholdPx": 5,
      "tapDragWindowMs": 180
    }
  },
  "wifi": {
    "sta": {
      "ssid": "YourNetworkName",
      "password": "YourPassword"
    },
    "ap": {
      "ssid": "walkey-talkey",
      "password": "secretKEY"
    },
    "hostname": "walkey-talkey",
    "localUrl": "walkey-talkey.local"
  },
  "globalBindings": [],
  "bootMode": {
    "label": "Mode Control",
    "ui": {
      "title": "Swipe to switch mode"
    },
    "bindings": []
  },
  "modes": []
}
```

### Required Root Keys

These root keys are required:

- `version`
- `globalBindings`
- `bootMode`
- `modes`

### Optional Root Keys

These root keys are optional but commonly used:

- `activeMode`
- `defaults`
- `wifi`

### Root Field Rules

- `version` is currently `1`
- `activeMode` must match one of the generated mode `id` values if present
- unknown root fields are invalid
- prefer stable ordering of keys for readability

Recommended key order:

1. `version`
2. `activeMode`
3. `defaults`
4. `wifi`
5. `globalBindings`
6. `bootMode`
7. `modes`

## Canonical Modeling Style

Always prefer the canonical style below:

- `modes` is an array
- each mode has a stable string `id`
- each mode has a readable `label`
- use `cycleOrder` when mode order matters
- use `modifiers` arrays for shortcuts
- use symbolic HID usage tokens when available
- keep action arrays explicit and readable

Avoid:

- object-shaped `modes`
- hidden conventions that are not encoded in the JSON
- compatibility-only fields when canonical fields exist

## Defaults Block

The `defaults` block controls touch timing and mouse backend configuration:

```json
"defaults": {
  "touch": {
    "holdMs": 400,
    "doubleTapMs": 350,
    "swipeMinDistance": 40
  },
  "defaultMouse": "airMouse",
  "airMouse": {
    "sensitivity": 1.0,
    "deadZoneDps": 6.0,
    "easingExponent": 1.25,
    "maxDps": 300.0,
    "emaAlpha": 0.35,
    "rewindDepth": 12,
    "rewindDecay": 0.7,
    "calibrationSamples": 128
  },
  "touchMouse": {
    "sensitivity": 1.0,
    "moveThresholdPx": 5,
    "tapDragWindowMs": 180
  }
}
```

### Touch Timing

- `holdMs` -- milliseconds before a press becomes a long-press
- `doubleTapMs` -- window for a second tap to count as double-tap
- `swipeMinDistance` -- minimum pixel distance for a swipe gesture

### Mouse Configuration

- `defaultMouse` -- which mouse backend is active: `"airMouse"` (gyro/IMU) or `"touchMouse"` (trackpad-style)
- `airMouse` -- tuning for the gyroscope-based air mouse:
  - `sensitivity` -- overall cursor speed multiplier (default 1.0)
  - `deadZoneDps` -- degrees/second below which gyro input is ignored (default 6.0)
  - `easingExponent` -- power curve exponent; lower = more linear, higher = more acceleration (default 1.25)
  - `maxDps` -- gyro saturation point in degrees/second (default 300.0)
  - `emaAlpha` -- EMA smoothing factor 0..1; lower = smoother but laggier (default 0.35)
  - `rewindDepth` -- number of recent samples rewound on release to cancel jitter (max 16, default 12)
  - `rewindDecay` -- exponential decay weight for rewind; lower = more aggressive compensation (default 0.7)
  - `calibrationSamples` -- IMU samples taken at startup for drift calibration (default 128)
- `touchMouse` -- tuning for the touch-based mouse:
  - `sensitivity` -- cursor speed multiplier (default 1.0)
  - `moveThresholdPx` -- minimum pixels of movement before cursor starts tracking (default 5)
  - `tapDragWindowMs` -- window after a tap to detect tap-and-drag (default 180)

### Defaults Guidance

- keep defaults conservative unless the prompt specifically wants different gesture timing or mouse tuning
- do not set absurdly low hold or double-tap thresholds
- if the prompt is only about macros, leave defaults near the existing values
- all `airMouse` and `touchMouse` fields are optional; omitted fields use firmware defaults
- the old `"mouseMode": "air"` / `"touch"` string is still accepted for backward compatibility but `defaultMouse` is preferred

## Wi-Fi Block

The intended Wi-Fi block is:

```json
"wifi": {
  "sta": {
    "ssid": "YourNetworkName",
    "password": "YourPassword"
  },
  "ap": {
    "ssid": "walkey-talkey",
    "password": "secretKEY"
  },
  "hostname": "walkey-talkey",
  "localUrl": "walkey-talkey.local"
}
```

Meaning:

- `sta` is the router network the device tries first
- `ap` is the fallback access point the device hosts itself
- `hostname` is the mDNS hostname stem
- `localUrl` is the preferred human-facing local URL

Do not swap the STA and AP roles.

If the user's prompt is unrelated to networking, preserve this block as-is.

## Binding Model

A binding always has:

- `input`
- `trigger`
- `actions`

Example:

```json
{
  "input": "touch",
  "trigger": "tap",
  "actions": [
    { "type": "hid_key_tap", "key": "ENTER" }
  ]
}
```

Interpretation:

- `input` says what source generated the event
- `trigger` says what happened
- `actions` is the macro that runs in order

## Inputs

Currently supported practical inputs:

- `boot_button`
- `touch`

Do not generate configs using any other input unless the user explicitly wants speculative future-only design, which is not the normal case.

## Triggers

Currently supported normalized triggers:

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

### Trigger Semantics

Important touch timing behavior:

- `tap` is delayed until the double-tap timeout expires
- a second tap inside the timeout becomes `double_tap`
- crossing the hold threshold emits `long_press` and then `hold_start`
- releasing after a hold emits `hold_end`
- swipe triggers are edge-triggered one-shots, not continuous repeats

### Typical Input And Trigger Pairings

Common real pairings:

- `boot_button + press`
- `boot_button + release`
- `touch + tap`
- `touch + double_tap`
- `touch + long_press`
- `touch + hold_start`
- `touch + hold_end`
- `touch + swipe_up`
- `touch + swipe_down`
- `touch + swipe_left`
- `touch + swipe_right`

## Action Types

Supported action types:

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
- `mouse_on`
- `mouse_off`
- `mouse_toggle`
- `noop`

## Action Reference

### Keyboard Key Actions

Use normal keyboard keys when the action is a plain key press.

Tap:

```json
{ "type": "hid_key_tap", "key": "ENTER" }
```

Hold and release:

```json
{ "type": "hid_key_down", "key": "F13" }
{ "type": "hid_key_up", "key": "F13" }
```

Use down/up pairs when a workflow spans separate triggers.

### Shortcut Actions

Use `hid_shortcut_tap` for keyboard chords.

```json
{ "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "N" }
```

Good examples:

- `CTRL + N`
- `CTRL + SHIFT + A`
- `ALT + TAB`

Always prefer the `modifiers` array, even for one modifier.

### Modifier Hold Actions

Use only when a modifier must remain down across multiple action steps.

```json
{ "type": "hid_modifier_down", "modifier": "CTRL" }
{ "type": "hid_key_tap", "key": "S" }
{ "type": "hid_modifier_up", "modifier": "CTRL" }
```

Do not use this when `hid_shortcut_tap` would already solve the task.

### HID Usage Actions

Use these for consumer or system HID controls.

Symbolic examples:

```json
{ "type": "hid_usage_tap", "usage": "PLAY_PAUSE" }
{ "type": "hid_usage_tap", "usage": "MEDIA_NEXT_TRACK" }
{ "type": "hid_usage_tap", "usage": "VOLUME_UP" }
```

Raw advanced usage form:

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

Use the raw object form only when there is no clean symbolic token.

### Timing Action

Use `sleep_ms` only for extra delay between multiple actions.

```json
{ "type": "sleep_ms", "duration_ms": 20 }
```

Rules:

- tap actions already include built-in press-to-release timing
- `sleep_ms` is for extra spacing, not as a substitute for hold logic
- `sleep_ms` with `duration_ms: 0` is pointless

### Mode Actions

Mode actions:

- `enter_boot_mode`
- `exit_boot_mode`
- `set_mode`
- `cycle_mode`

Examples:

```json
{ "type": "set_mode", "mode": "cursor" }
```

```json
{ "type": "cycle_mode", "direction": "next" }
```

`cycle_mode.direction` must be:

- `next`
- `previous`

### UI Actions

Use UI actions for simple device feedback.

```json
{ "type": "ui_hint", "text": "Next slide" }
```

```json
{ "type": "ui_show_mode" }
```

Rules:

- keep hint text short
- avoid paragraphs
- use action-oriented wording

### Microphone Actions

Microphone actions:

- `mic_gate`
- `mic_gate_toggle`

Examples:

```json
{ "type": "mic_gate", "enabled": true }
{ "type": "mic_gate", "enabled": false }
{ "type": "mic_gate_toggle" }
```

### Mouse Overlay Actions

Activate or deactivate mouse functionality as an overlay on the current mode without switching to the dedicated Mouse mode.

- `mouse_on` -- activates mouse overlay. Cursor tracking starts immediately by default (no click on release).
  - `mouseType` (optional): `"airMouse"` or `"touchMouse"`. Overrides `defaultMouse` for this activation.
  - `tracking` (optional, default `true`): if `true`, cursor moves immediately; if `false`, the full gesture handler is used (long-press = cursor, tap = click, etc.).
- `mouse_off` -- deactivates mouse overlay. Stops tracking, releases buttons, no click produced.
- `mouse_toggle` -- toggles overlay on/off. Same optional fields as `mouse_on` (used only when toggling ON).

When the overlay is active, touch events go to the mouse gesture handler. Release events are also forwarded to the input router so `hold_end` bindings can fire (enabling hold-to-mouse patterns). The overlay auto-deactivates on mode change.

```jsonc
// Hold-to-mouse: long press activates air mouse, release deactivates
{ "input": "touch", "trigger": "hold_start", "actions": [{ "type": "mouse_on" }] }
{ "input": "touch", "trigger": "hold_end", "actions": [{ "type": "mouse_off" }] }

// Toggle mouse on/off with double-tap
{ "input": "touch", "trigger": "double_tap", "actions": [{ "type": "mouse_toggle" }] }

// Explicit touch backend override
{ "type": "mouse_on", "mouseType": "touchMouse" }

// Full gesture mode (not tracking-only)
{ "type": "mouse_on", "tracking": false }
```

### No-Op

`noop` intentionally does nothing.

Use it only as a placeholder when explicitly needed.

## Macro Runtime Rules

These rules shape how good macros should be generated:

- actions run from top to bottom
- if one action fails, later actions in that same binding are skipped
- the runtime can dispatch at most 8 bindings for one resolved input+trigger combination
- macros should be explicit, not clever

Implication:

- do not create a 15-step macro if a 2-step macro achieves the same result

## Mode System Design

The configuration has three main behavioral layers:

- `globalBindings`
- `bootMode`
- per-mode bindings inside `modes`

### Global Bindings

Use these for behavior that must always work regardless of the active mode.

Recommended use:

- entering `bootMode`
- exiting `bootMode`

Safe baseline:

```json
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
]
```

### Boot Mode

`bootMode` is the temporary BOOT-held control layer.

Recommended fields:

```json
"bootMode": {
  "label": "Mode Control",
  "ui": {
    "title": "Swipe to switch mode"
  },
  "bindings": []
}
```

Use `bootMode.bindings` for temporary control gestures such as:

- cycling modes
- selecting a specific mode
- showing the active mode

### Modes Array

Each mode should look like:

```json
{
  "id": "cursor",
  "label": "Cursor",
  "cycleOrder": 0,
  "bindings": []
}
```

Rules:

- `id` must be stable and machine-safe
- `label` should be human-readable
- `cycleOrder` should be unique when used
- `bindings` is an array

## Mode-Change Constraints

Mode changes are boundaries.

When mode changes occur:

- active HID output state is cleared
- held keys are cleared
- mic gate state is cleared

Do not generate macros that expect held state to survive `set_mode` or `cycle_mode`.

Bad idea:

- hold `CTRL`
- change mode
- expect `CTRL` to still be held

That assumption is invalid.

## Good Generation Patterns

### Simple Single Action

```json
{
  "input": "touch",
  "trigger": "tap",
  "actions": [
    { "type": "hid_key_tap", "key": "ENTER" }
  ]
}
```

### Shortcut With Feedback

```json
{
  "input": "touch",
  "trigger": "swipe_right",
  "actions": [
    { "type": "ui_hint", "text": "Submit" },
    { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "ENTER" }
  ]
}
```

### Dictation Or Push-To-Talk Style Hold

```json
{
  "input": "touch",
  "trigger": "hold_start",
  "actions": [
    { "type": "mic_gate", "enabled": true },
    { "type": "hid_key_down", "key": "F13" }
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

### Text Editing Macro

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

### Media Mode Pattern

```json
{
  "id": "media",
  "label": "Media",
  "cycleOrder": 2,
  "bindings": [
    {
      "input": "touch",
      "trigger": "swipe_left",
      "actions": [
        { "type": "hid_usage_tap", "usage": "MEDIA_PREV_TRACK" }
      ]
    },
    {
      "input": "touch",
      "trigger": "swipe_right",
      "actions": [
        { "type": "hid_usage_tap", "usage": "MEDIA_NEXT_TRACK" }
      ]
    },
    {
      "input": "touch",
      "trigger": "tap",
      "actions": [
        { "type": "hid_usage_tap", "usage": "PLAY_PAUSE" }
      ]
    }
  ]
}
```

## Bad Generation Patterns

Avoid these:

- unsupported trigger names like `triple_tap`
- unsupported action names
- duplicate bindings with contradictory meanings on the same input+trigger
- empty modes unless scaffolding was requested
- long action chains when one shortcut would do
- random Wi-Fi changes when the prompt is unrelated to networking
- deleting global BOOT recovery to save space

## Naming Rules

Use:

- short stable mode ids such as `cursor`, `media`, `navigation`, `presentation`
- labels that are readable by humans
- ui hints that are short and action-oriented

Avoid:

- ids with spaces
- vague labels like `Mode1`
- flashy or unclear hint text

## Full Example Config

This is a valid end-to-end example:

```json
{
  "version": 1,
  "activeMode": "cursor",
  "defaults": {
    "touch": {
      "holdMs": 400,
      "doubleTapMs": 350,
      "swipeMinDistance": 40
    },
    "defaultMouse": "airMouse",
    "airMouse": {
      "sensitivity": 1.0,
      "deadZoneDps": 6.0,
      "easingExponent": 1.25,
      "maxDps": 300.0,
      "emaAlpha": 0.35,
      "rewindDepth": 12,
      "rewindDecay": 0.7,
      "calibrationSamples": 128
    },
    "touchMouse": {
      "sensitivity": 1.0,
      "moveThresholdPx": 5,
      "tapDragWindowMs": 180
    }
  },
  "wifi": {
    "sta": {
      "ssid": "YourNetworkName",
      "password": "YourPassword"
    },
    "ap": {
      "ssid": "walkey-talkey",
      "password": "secretKEY"
    },
    "hostname": "walkey-talkey",
    "localUrl": "walkey-talkey.local"
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
      "title": "Swipe to switch mode"
    },
    "bindings": [
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
        "trigger": "swipe_right",
        "actions": [
          { "type": "cycle_mode", "direction": "next" },
          { "type": "ui_show_mode" }
        ]
      }
    ]
  },
  "modes": [
    {
      "id": "cursor",
      "label": "Cursor",
      "cycleOrder": 0,
      "bindings": [
        {
          "input": "touch",
          "trigger": "tap",
          "actions": [
            { "type": "hid_key_tap", "key": "F14" }
          ]
        },
        {
          "input": "touch",
          "trigger": "hold_start",
          "actions": [
            { "type": "mic_gate", "enabled": true },
            { "type": "hid_key_down", "key": "F13" }
          ]
        },
        {
          "input": "touch",
          "trigger": "hold_end",
          "actions": [
            { "type": "mic_gate", "enabled": false },
            { "type": "hid_key_up", "key": "F13" }
          ]
        }
      ]
    },
    {
      "id": "media",
      "label": "Media",
      "cycleOrder": 1,
      "bindings": [
        {
          "input": "touch",
          "trigger": "swipe_left",
          "actions": [
            { "type": "hid_usage_tap", "usage": "MEDIA_PREV_TRACK" }
          ]
        },
        {
          "input": "touch",
          "trigger": "swipe_right",
          "actions": [
            { "type": "hid_usage_tap", "usage": "MEDIA_NEXT_TRACK" }
          ]
        },
        {
          "input": "touch",
          "trigger": "tap",
          "actions": [
            { "type": "hid_usage_tap", "usage": "PLAY_PAUSE" }
          ]
        }
      ]
    },
    {
      "id": "presentation",
      "label": "Presentation",
      "cycleOrder": 2,
      "bindings": [
        {
          "input": "touch",
          "trigger": "tap",
          "actions": [
            { "type": "hid_key_tap", "key": "RIGHT_ARROW" },
            { "type": "ui_hint", "text": "Next slide" }
          ]
        },
        {
          "input": "touch",
          "trigger": "hold_start",
          "actions": [
            { "type": "mouse_on" },
            { "type": "ui_hint", "text": "Air mouse" }
          ]
        },
        {
          "input": "touch",
          "trigger": "hold_end",
          "actions": [
            { "type": "mouse_off" },
            { "type": "ui_hint", "text": "Presentation" }
          ]
        }
      ]
    }
  ]
}
```

## How To Build A Config From A User Prompt

When given a prompt, follow this process:

1. Identify the requested modes.
2. Identify the required touch or BOOT gestures.
3. Map each requested behavior to a supported trigger.
4. Choose the simplest supported action type for each behavior.
5. Preserve global BOOT recovery.
6. Keep Wi-Fi unchanged unless networking was requested.
7. Use readable mode labels and stable ids.
8. Prefer explicit macros over hidden assumptions.
9. Produce the full JSON or the precise patch requested.
10. Verify all generated bindings are supported by the current runtime.

## Final Validation Checklist

Before returning generated JSON, verify all of the following:

1. all required root keys exist
2. there are no unknown root keys
3. every binding has `input`, `trigger`, and `actions`
4. every action uses a supported `type`
5. every mode id is unique
6. `activeMode` points at a real mode id if present
7. BOOT recovery exists through `globalBindings`
8. mode changes do not rely on persistent held state
9. Wi-Fi still uses STA for router and AP for fallback
10. the config is readable and maintainable by a human

## Output Quality Standard

A high-quality AI-generated config is:

- valid
- minimal but capable
- readable
- safe
- feature-rich where useful
- consistent in style
- immediately usable from start to finish

If the user's prompt is ambiguous, choose the safer valid interpretation rather than inventing unsupported behavior.

## MCP Server (AI Client Integration)

This device ships with an MCP (Model Context Protocol) server that AI clients like Cursor and Claude Code can use to read and update the device configuration directly.

### Setup

The MCP server is published on npm as `walkey-talkey-mcp`. No manual installation is needed -- `npx` downloads and runs it automatically.

### Cursor Configuration

Add to `.cursor/mcp.json` (project-level) or global Cursor MCP settings:

```json
{
  "mcpServers": {
    "walkey-talkey": {
      "command": "npx",
      "args": ["walkey-talkey-mcp"]
    }
  }
}
```

To override the device URL, add an `env` block:

```json
{
  "mcpServers": {
    "walkey-talkey": {
      "command": "npx",
      "args": ["walkey-talkey-mcp"],
      "env": {
        "WALKEY_URL": "http://192.168.0.43"
      }
    }
  }
}
```

### Claude Code Configuration

```bash
claude mcp add walkey-talkey npx walkey-talkey-mcp
```

Or add to `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "walkey-talkey": {
      "command": "npx",
      "args": ["walkey-talkey-mcp"]
    }
  }
}
```

### Available MCP Tools

The MCP server exposes 23 tools under the `walkey.*` namespace:

**Discovery**: `walkey.ping`, `walkey.get_config`, `walkey.get_config_canonical`, `walkey.get_schema`

**Mode CRUD**: `walkey.list_modes`, `walkey.get_mode`, `walkey.set_mode`, `walkey.create_mode`, `walkey.delete_mode`

**Binding-level**: `walkey.get_bindings`, `walkey.set_binding`, `walkey.remove_binding`

**Wi-Fi**: `walkey.get_wifi`, `walkey.set_wifi`

**Defaults**: `walkey.get_defaults`, `walkey.set_defaults`

**Active mode**: `walkey.get_active_mode`, `walkey.set_active_mode`

**Boot mode**: `walkey.get_boot_mode`

**Global**: `walkey.get_global_bindings`

**Escape hatch**: `walkey.set_config`, `walkey.validate_config`, `walkey.reset_config`

### AI Best Practices for MCP

When using MCP tools to update the device:

1. **Prefer per-mode tools** (`walkey.set_mode`, `walkey.set_binding`) over the full-config escape hatch (`walkey.set_config`). Per-mode tools only touch the targeted mode -- they never accidentally overwrite Wi-Fi or other modes.
2. **Read before write**: call `walkey.get_mode` to get the current state, then modify and pass to `walkey.set_mode`.
3. **Validate first**: use `walkey.validate_config` before `walkey.set_config` if replacing the full config.
4. **Never silently change Wi-Fi** unless the user explicitly requests it.

### Granular REST API Reference

The MCP server calls these firmware HTTP endpoints:

| Method | Path | Description |
|---|---|---|
| GET | `/api/modes` | List all modes (summary) |
| GET | `/api/mode?id=X` | Get single mode |
| PUT | `/api/mode?id=X` | Replace single mode |
| POST | `/api/mode` | Create new mode |
| DELETE | `/api/mode?id=X` | Delete mode |
| GET | `/api/wifi` | Get Wi-Fi config |
| PUT | `/api/wifi` | Update Wi-Fi (merge) |
| GET | `/api/defaults` | Get defaults (touch, mouse) |
| PUT | `/api/defaults` | Update defaults (touch, mouse; merge) |
| PUT | `/api/active-mode` | Set active mode |
| GET | `/api/boot-mode` | Get boot mode |
| GET | `/api/global-bindings` | Get global bindings |

All write endpoints use atomic read-modify-write internally. PUT endpoints for `/api/wifi` and `/api/defaults` use merge semantics (only provided fields are updated).
