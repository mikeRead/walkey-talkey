# User Guide

## Overview

This firmware turns the ESP32-S3 Touch AMOLED board into a configurable touch-driven macro controller with:

- a USB microphone
- a USB HID keyboard/media controller
- a touchscreen for taps, holds, double taps, and swipes
- a BOOT-button mode selector
- a built-in Wi-Fi config portal for editing the JSON config file

The built-in default modes are:

- `Cursor`
- `Presentation`
- `Media`
- `Navigation`

## Normal Device Use

When the device is running normally:

- the main screen shows the active mode
- touchscreen gestures trigger the bindings for that mode
- holding `BOOT` enters the temporary `bootMode` control layer
- while `BOOT` is held, swipe left and right changes modes
- releasing `BOOT` confirms the selected mode and returns to normal use

## Config Portal

The device first tries to join the router configured in the JSON file.

- Preferred URL: `http://walkey-talkey.local/`
- If router join succeeds: browse to the IP shown on the BOOT screen or use `walkey-talkey.local`
- If router join fails: the device falls back to its own Wi-Fi network
- Fallback SSID: `walkey-talkey`
- Fallback Password: `secretKEY`
- Fallback URL: `http://192.168.4.1/`

Important:

- the portal starts about 8 seconds after boot
- on the BOOT overlay, the line under `Swipe to switch mode` shows `Connecting...` immediately, then changes to the active address once Wi-Fi is ready
- if the device falls back to its own Wi-Fi network, that BOOT line shows `AP: walkey-talkey (<ip>)`
- if the page does not load immediately, wait a few more seconds and refresh once
- the portal works locally over either your router Wi-Fi or the fallback device Wi-Fi network; internet access is not required

## Portal Actions

The web UI supports four main actions:

- `Load` or opening the page reads the active config from the device
- `Validate` checks JSON without saving it
- `Save` validates, normalizes to canonical JSON, writes `/spiffs/mode-config.json`, reloads runtime state, and reapplies Wi-Fi immediately
- `Reset` writes the built-in firmware default JSON back to `/spiffs/mode-config.json`, reloads runtime state, and reapplies Wi-Fi immediately
- `Downloads` lets you download `mode-config.schema.json` and this `USER_GUIDE.md` directly from the portal

Notes:

- `Reset` restores the built-in default config, not the hardcoded failsafe config
- the hardcoded failsafe config is only used internally if both the external and built-in JSON fail
- saved JSON may come back slightly reformatted because the firmware writes canonical output

## Where To Edit The Config

The editable runtime file is:

- `/spiffs/mode-config.json`

The repo also includes:

- `config/mode-config.json` as the main example file
- `config/mode-config.schema.json` as the validation contract
- `docs/mode-system-reference.md` as the deeper developer and behavior reference

For most users, the best workflow is:

1. Start from `config/mode-config.json`.
2. Change one mode or binding at a time.
3. Use `Validate`.
4. Use `Save`.
5. Test on the device immediately.

## JSON Mental Model

The JSON model is intentionally simple:

- one config file contains global behavior, boot-mode behavior, and per-mode behavior
- a binding is `input` + `trigger` + ordered `actions`
- the `actions` array is the macro
- actions run from top to bottom
- `sleep_ms` adds extra delay between steps when needed

Think of it like this:

- `input` says what source generated the event
- `trigger` says what happened on that source
- `actions` says what the device should do in response

## Full Config Shape

A practical top-level config looks like this:

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
      "title": "Swipe to switch mode",
      "subtitle": "Hold BOOT and swipe to change modes",
      "showModeList": true,
      "showGestureHints": true,
      "showCurrentModeCard": true
    },
    "bindings": []
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

### `version`

- Use `1`.
- This is the config format version.

### `activeMode`

- This is the mode the device starts in after boot.
- The value must match one of the mode `id` values in `modes`.

### `defaults`

- Shared timing, threshold, and mouse configuration settings live here.
- `defaults.touch` controls gesture timing.
- `defaults.defaultMouse`, `defaults.airMouse`, and `defaults.touchMouse` control mouse backend selection and tuning.

### `globalBindings`

- These bindings are always active.
- Use them for things that should work no matter which mode is selected.
- The default config uses them to enter and exit `bootMode`.

### `bootMode`

- This is a temporary control layer that is active while `BOOT` is held.
- Use it for mode-switch gestures and control UI feedback.

### `modes`

- This is the list of normal working modes.
- Each mode has its own `id`, `label`, `cycleOrder`, and `bindings`.
- Prefer the array form shown above with stable `id` values.

## Updating Defaults

Touch behavior is controlled by:

- `holdMs`
- `doubleTapMs`
- `swipeMinDistance`

Example:

```json
"defaults": {
  "touch": {
    "holdMs": 450,
    "doubleTapMs": 300,
    "swipeMinDistance": 50
  }
}
```

What each one means:

- `holdMs`: how long the finger must stay down before a hold becomes a `long_press` and `hold_start`
- `doubleTapMs`: how long the firmware waits for a second tap before deciding a single `tap`
- `swipeMinDistance`: the minimum movement needed before a touch gesture counts as a swipe

Practical tuning advice:

- raise `holdMs` if holds trigger too easily
- lower `holdMs` if long-press actions feel slow
- raise `doubleTapMs` if double taps are hard to hit
- lower `doubleTapMs` if normal taps feel delayed
- raise `swipeMinDistance` if accidental swipes happen
- lower `swipeMinDistance` if swipes feel too hard to trigger

### Mouse Configuration

The device supports two mouse backends that turn the touchpad into a USB HID mouse:

- **Air Mouse** (`airMouse`) -- uses the built-in gyroscope (IMU). Touch the pad to start moving the cursor by tilting the device. Gestures like tap, double-tap, long-press, and multi-touch produce mouse clicks.
- **Touch Mouse** (`touchMouse`) -- uses the touchpad as a traditional trackpad. Drag to move the cursor, tap for clicks.

Set which backend is active with `defaultMouse`:

```json
"defaults": {
  "defaultMouse": "airMouse"
}
```

Valid values are `"airMouse"` and `"touchMouse"`.

#### Air Mouse Settings

All fields are optional. Omitted fields use firmware defaults.

```json
"airMouse": {
  "sensitivity": 1.0,
  "deadZoneDps": 6.0,
  "easingExponent": 1.25,
  "maxDps": 300.0,
  "emaAlpha": 0.35,
  "rewindDepth": 12,
  "rewindDecay": 0.7,
  "calibrationSamples": 128
}
```

What each setting does:

- `sensitivity` -- overall cursor speed multiplier. Raise for faster movement, lower for more precision.
- `deadZoneDps` -- minimum gyro speed (degrees/second) to register movement. Raise if the cursor drifts when still.
- `easingExponent` -- controls the acceleration curve. Lower values (closer to 1.0) make movement more linear. Higher values emphasize slow-speed precision and fast-speed responsiveness.
- `maxDps` -- the gyro speed at which cursor velocity tops out.
- `emaAlpha` -- smoothing strength (0 to 1). Lower values give smoother but laggier cursor movement.
- `rewindDepth` -- how many recent movement samples are rewound when you lift your finger, to cancel accidental jitter. Max 16.
- `rewindDecay` -- how much weight recent samples get during rewind. Lower means more aggressive jitter compensation.
- `calibrationSamples` -- how many IMU readings are averaged at startup for drift compensation.

#### Touch Mouse Settings

All fields are optional. Omitted fields use firmware defaults.

```json
"touchMouse": {
  "sensitivity": 1.0,
  "moveThresholdPx": 5,
  "tapDragWindowMs": 180
}
```

What each setting does:

- `sensitivity` -- cursor speed multiplier. Raise for faster tracking.
- `moveThresholdPx` -- minimum pixels of finger movement before the cursor starts tracking. Prevents accidental movement during taps.
- `tapDragWindowMs` -- time window (ms) after a tap to detect a tap-and-drag gesture.

## Inputs

The config model uses named inputs.

Currently shipped inputs:

- `boot_button`
- `touch`

Reference-reserved inputs for future expansion:

- `encoder`
- `usb_host_key`
- `timer`
- `imu`

Important:

- not every input supports every trigger
- the current firmware mainly uses `boot_button` and `touch`

## Triggers

The firmware exposes normalized trigger names:

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

Current touch behavior:

- `tap` is delayed until the `doubleTapMs` timeout expires
- if a second tap arrives inside that timeout, the firmware emits `double_tap` instead of a second `tap`
- when the hold threshold is crossed, the firmware emits `long_press` and then `hold_start`
- when the finger is released after a hold started, the firmware emits `hold_end`

Common input-to-trigger usage:

- `boot_button`: usually `press`, `release`, and sometimes `long_press`
- `touch`: usually `tap`, `double_tap`, `long_press`, `hold_start`, `hold_end`, and swipe triggers

## Actions

Actions are the building blocks of every macro. Each object in a binding's `actions` array has a `type` and then type-specific fields.

Common action types:

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

### Keyboard Actions

Use these when you want normal keyboard behavior.

`hid_key_tap`:

```json
{ "type": "hid_key_tap", "key": "ENTER" }
```

`hid_key_down` and `hid_key_up`:

```json
{ "type": "hid_key_down", "key": "F13" }
{ "type": "hid_key_up", "key": "F13" }
```

Use them when you want a press on one trigger and a release on another trigger, such as push-to-talk or dictation hold behavior.

### Shortcut Actions

Use `hid_shortcut_tap` for keyboard chords.

```json
{ "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "N" }
```

Example combinations:

- `["CTRL"]` + `N`
- `["CTRL", "SHIFT"]` + `A`
- `["ALT"]` + `TAB`

Prefer `modifiers` as an array even if there is only one modifier.

### Modifier Actions

Use these for advanced sequences where a modifier must stay down across steps.

```json
{ "type": "hid_modifier_down", "modifier": "CTRL" }
{ "type": "hid_key_tap", "key": "S" }
{ "type": "hid_modifier_up", "modifier": "CTRL" }
```

### HID Usage Actions

Use these for consumer or system HID controls such as media and volume.

Common symbolic usage names:

- `PLAY_PAUSE`
- `MEDIA_PREV_TRACK`
- `MEDIA_NEXT_TRACK`
- `VOLUME_UP`
- `VOLUME_DOWN`

Example:

```json
{ "type": "hid_usage_tap", "usage": "MEDIA_NEXT_TRACK" }
```

Advanced form:

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

Use the advanced form only when you need a specific raw HID usage that does not already have a good named token.

### Timing Actions

Use `sleep_ms` for extra spacing between steps.

```json
{ "type": "sleep_ms", "duration_ms": 20 }
```

Important:

- tap actions already include the built-in press-to-release timing
- the current built-in tap gap is `20 ms`
- use `sleep_ms` only when you need additional delay between multiple actions
- `sleep_ms` with `duration_ms: 0` does nothing

### Mode Actions

Use these to change device state:

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

Important:

- mode-changing actions act like boundaries
- the firmware resets active outputs before switching mode state
- that reset clears active HID output state and mic gating

### UI Actions

Use these to show feedback on the device screen.

Examples:

```json
{ "type": "ui_hint", "text": "Next slide" }
```

```json
{ "type": "ui_show_mode" }
```

### Microphone Actions

Use these to control the gated microphone behavior.

Examples:

```json
{ "type": "mic_gate", "enabled": true }
```

```json
{ "type": "mic_gate", "enabled": false }
```

```json
{ "type": "mic_gate_toggle" }
```

### Per-Mode Mouse Actions

You can activate mouse functionality (air mouse or touch trackpad) as an overlay on any mode without switching to the dedicated Mouse mode. This is useful for things like presentation mode where you want cursor control on demand.

**`mouse_on`** -- activates the mouse overlay. By default, cursor tracking starts immediately and releasing the touch produces no click.

```json
{ "type": "mouse_on" }
```

Optional fields:
- `mouseType`: `"airMouse"` or `"touchMouse"` to override the default backend.
- `tracking`: `true` (default) for immediate cursor movement, `false` for full gesture support (long-press = cursor, tap = click, etc.).

**`mouse_off`** -- deactivates the mouse overlay and returns touch to normal bindings.

```json
{ "type": "mouse_off" }
```

**`mouse_toggle`** -- toggles the overlay on or off. Same optional fields as `mouse_on`.

```json
{ "type": "mouse_toggle" }
```

**Hold-to-mouse example** (presentation mode):

```json
{
  "input": "touch",
  "trigger": "hold_start",
  "actions": [{ "type": "mouse_on" }, { "type": "ui_hint", "text": "Air mouse" }]
},
{
  "input": "touch",
  "trigger": "hold_end",
  "actions": [{ "type": "mouse_off" }, { "type": "ui_hint", "text": "Presentation" }]
}
```

While the overlay is active, taps and swipes go to the mouse gesture handler. The overlay auto-deactivates when you switch modes.

### No-Op Action

`noop` intentionally does nothing.

Use it only when you need a placeholder while testing or while sketching out a config.

## Binding Shape

Each binding has this shape:

```json
{
  "input": "touch",
  "trigger": "swipe_left",
  "actions": [
    { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "N" },
    { "type": "ui_hint", "text": "New item" }
  ]
}
```

What each field means:

- `input`: where the event comes from
- `trigger`: which event on that input should match
- `actions`: the ordered list of steps to execute

## Macro Execution Rules

These rules matter when building your own macros:

- actions run one at a time from top to bottom
- later actions do not start until the current action finishes
- if an action fails, the rest of that binding is skipped
- if a binding fails during a matched-event dispatch, later matched bindings for that same event are also skipped
- the runtime collects at most `8` matching bindings for one input and trigger dispatch

Practical meaning:

- keep macros short and intentional
- do not depend on hidden timing
- add `sleep_ms` only when you really need it
- do not assume the firmware will automatically undo earlier steps if a later step fails

## How To Build Your Own Modes

A mode object looks like this:

```json
{
  "id": "editing",
  "cycleOrder": 4,
  "label": "Editing",
  "bindings": [
    {
      "input": "touch",
      "trigger": "swipe_left",
      "actions": [
        { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "Z" },
        { "type": "ui_hint", "text": "Undo" }
      ]
    }
  ]
}
```

Guidelines:

- keep `id` stable once you start using it
- make `label` human-readable
- keep `cycleOrder` unique if you want predictable swipe-based mode cycling
- use `bindings` only for behavior that should depend on the current mode

## Global Bindings Versus Boot Mode Versus Mode Bindings

Use `globalBindings` for behavior that should always work:

- entering `bootMode`
- exiting `bootMode`
- emergency mute
- always-available home or reset actions

Use `bootMode.bindings` for temporary BOOT-held control gestures:

- next mode
- previous mode
- jump to a favorite mode
- show current mode information

Use per-mode `bindings` for context-specific behavior:

- dictation
- media control
- presentation control
- editor shortcuts
- navigation keys

## Recommended Authoring Patterns

### Single Key

```json
{
  "input": "touch",
  "trigger": "tap",
  "actions": [
    { "type": "hid_key_tap", "key": "ENTER" }
  ]
}
```

### Keyboard Shortcut

```json
{
  "input": "touch",
  "trigger": "swipe_left",
  "actions": [
    { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "N" },
    { "type": "ui_hint", "text": "New chat" }
  ]
}
```

### Multi-Step Macro

```json
{
  "input": "touch",
  "trigger": "swipe_up",
  "actions": [
    { "type": "hid_shortcut_tap", "modifiers": ["CTRL"], "key": "A" },
    { "type": "sleep_ms", "duration_ms": 20 },
    { "type": "hid_key_tap", "key": "BACKSPACE" },
    { "type": "ui_hint", "text": "Clear field" }
  ]
}
```

### Hold-To-Activate

```json
{
  "input": "touch",
  "trigger": "hold_start",
  "actions": [
    { "type": "hid_key_down", "key": "F13" },
    { "type": "mic_gate", "enabled": true },
    { "type": "ui_hint", "text": "Dictation active" }
  ]
}
```

```json
{
  "input": "touch",
  "trigger": "hold_end",
  "actions": [
    { "type": "mic_gate", "enabled": false },
    { "type": "hid_key_up", "key": "F13" },
    { "type": "ui_hint", "text": "Cursor mode" }
  ]
}
```

## Key And Token Advice

Prefer canonical tokens when possible:

- keyboard keys like `A`, `ENTER`, `SPACE`, `PAGE_UP`, `LEFT_ARROW`, `F13`
- modifiers like `CTRL`, `SHIFT`, `ALT`, `GUI`
- media and system tokens like `MEDIA_NEXT_TRACK`, `MEDIA_PREV_TRACK`, `PLAY_PAUSE`, `VOLUME_UP`, `VOLUME_DOWN`

Best practice:

- use `hid_key_tap` for one normal key
- use `hid_shortcut_tap` for chords
- use `hid_usage_tap` for media and system controls
- use the raw usage object only for advanced cases

## Safe Editing Rules

When editing your config:

- change one thing at a time
- keep BOOT entry and exit bindings working in `globalBindings`
- keep at least one reliable path back to a known mode
- validate before saving
- test every changed gesture right after saving
- avoid huge fan-out where many bindings all match the same event

## Troubleshooting

### Portal Does Not Appear

Check these first:

- wait at least 8 seconds after boot
- first try the address shown under `Swipe to switch mode`
- if the device fell back to AP mode, confirm you joined `walkey-talkey`
- if needed, browse to `http://192.168.4.1/`
- if needed, disconnect and reconnect Wi-Fi, then refresh the page

### Save Or Reset Fails

The portal returns detailed `STORAGE_FAILED` information when the firmware cannot write the config file.

Important fields:

- `stage`: where the failure happened: `mount`, `open`, `write`, or `flush`
- `formatAttempted`: whether the firmware allowed SPIFFS auto-format during mount
- `path`: usually `/spiffs/mode-config.json`
- `partition`: usually `storage`
- `espError`: ESP-IDF mount-layer error when available
- `errnoValue` and `errnoMessage`: file I/O details when available
- `suggestions`: likely recovery actions

Common causes:

- the `storage` SPIFFS partition was not flashed or is corrupted
- the flashed partition table does not match the firmware build
- the filesystem needs to be recreated after earlier experiments

Common recovery steps:

1. Try `Reset` again after a fresh reboot.
2. Reflash the firmware and partition table.
3. If the problem persists, do a clean rebuild and reflash so the `storage` partition layout matches the current firmware.

### Touch UI Looks Wrong After Boot

Expected behavior:

- the UI should remain stable before and after the portal starts
- the portal startup delay is intentional and should not break swipes or redraws

If the display or touch behavior becomes unstable again, reflash the current firmware build before assuming the JSON file is at fault.

## Recovery Expectations

The firmware has three config levels:

1. external JSON from `/spiffs/mode-config.json`
2. built-in JSON compiled into firmware
3. hardcoded failsafe config

This means:

- a bad saved JSON should usually recover by using `Reset`
- if the external file is unreadable, the firmware should fall back to the built-in config
- the failsafe config is a last resort and should not be the normal steady-state configuration

## MCP Server (AI-Powered Config Editing)

The device includes an MCP (Model Context Protocol) server that lets AI assistants in Cursor and Claude Code read and update your device config directly through natural language.

### How It Works

The MCP server is published on npm as `walkey-talkey-mcp`. It runs on your computer and talks to the device over Wi-Fi. AI clients like Cursor and Claude Code launch it automatically -- you just need Node.js installed.

### Cursor Setup

Add to your Cursor MCP settings (`.cursor/mcp.json` in your project, or global settings):

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

### Claude Code Setup

```bash
claude mcp add walkey-talkey npx walkey-talkey-mcp
```

### What You Can Do

Once configured, you can ask your AI assistant things like:

- "Show me all my modes"
- "Add a new mode called 'gaming' with WASD bindings"
- "Change the tap action in my cursor mode to send Enter"
- "Update my Wi-Fi SSID to MyNetwork"
- "What are my current touch defaults?"
- "Switch the active mode to media"

The AI uses 23 specialized tools to make precise, targeted changes without accidentally overwriting unrelated settings.

### Environment Variables

| Variable | Default | Description |
|---|---|---|
| `WALKEY_URL` | `http://walkey-talkey.local` | Base URL of the device |

If your device is at a different address (e.g. a direct IP), set `WALKEY_URL` accordingly.

## Which Document To Use

- Use this file for setup, JSON authoring, portal access, and troubleshooting.
- Use `config/mode-config.json` as the main starting point for your own mappings.
- Use `config/mode-config.schema.json` for validator and editor tooling.
- Use `docs/AI_GUIDE.md` when an AI is helping generate or repair configs.
- Use `docs/mode-system-reference.md` for the deeper runtime and developer reference.
