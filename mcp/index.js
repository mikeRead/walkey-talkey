#!/usr/bin/env node
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

const DEVICE_URL = process.env.WALKEY_URL || "http://walkey-talkey.local";

async function apiFetch(path, opts = {}) {
  const url = `${DEVICE_URL}${path}`;
  const res = await fetch(url, {
    ...opts,
    headers: { "Content-Type": "application/json", ...opts.headers },
  });
  const text = await res.text();
  let json;
  try {
    json = JSON.parse(text);
  } catch {
    json = { raw: text };
  }
  if (!res.ok) {
    throw new Error(`HTTP ${res.status}: ${JSON.stringify(json)}`);
  }
  return json;
}

async function apiGet(path) {
  return apiFetch(path);
}

async function apiPut(path, body) {
  return apiFetch(path, {
    method: "PUT",
    body: JSON.stringify(body),
  });
}

async function apiPost(path, body) {
  return apiFetch(path, {
    method: "POST",
    body: JSON.stringify(body),
  });
}

async function apiDelete(path) {
  return apiFetch(path, { method: "DELETE" });
}

function ok(data) {
  return { content: [{ type: "text", text: JSON.stringify(data, null, 2) }] };
}

function err(msg) {
  return { content: [{ type: "text", text: `Error: ${msg}` }], isError: true };
}

const server = new McpServer({
  name: "walkey-talkey",
  version: "1.1.0",
});

// --- Discovery tools ---

server.registerTool("walkey_ping", { description: "Check if the Walkey-Talkey device is reachable on the network. Returns 'ok' on success." }, async () => {
  try {
    const data = await apiGet("/ping");
    return ok(data);
  } catch (e) {
    return err(e.message);
  }
});

server.registerTool(
  "walkey_get_config",
  { description: "Retrieve the full device configuration as JSON. Returns all modes, Wi-Fi, defaults, and active mode." },
  async () => {
    try {
      const data = await apiGet("/config");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_get_config_canonical",
  { description: "Retrieve the configuration JSON in canonical schema-ordered format. Useful for comparison or export." },
  async () => {
    try {
      const data = await apiGet("/config/canonical");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_get_schema",
  { description: "Retrieve the JSON schema that defines valid configuration structure. Use to understand allowed fields and constraints." },
  async () => {
    try {
      const data = await apiGet("/download/schema.json");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Mode listing ---

server.registerTool(
  "walkey_list_modes",
  { description: "List all modes. Returns array of {id, label, cycleOrder, bindingCount} for each mode. The 'mouse' mode is built-in (builtIn:true) and cannot be edited or deleted via walkey_set_mode/walkey_delete_mode. To configure mouse behavior (air mouse vs touch mouse, sensitivity, etc.), use walkey_set_defaults instead." },
  async () => {
    try {
      const data = await apiGet("/api/modes");
      const hasMouseMode = data.some((m) => m.id === "mouse");
      if (!hasMouseMode) {
        data.push({ id: "mouse", label: "Mouse", cycleOrder: 65534, bindingCount: 0, builtIn: true });
      }
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Single mode CRUD ---

server.registerTool(
  "walkey_get_mode",
  {
    description: "Retrieve a single mode's full definition including all bindings. Use walkey_list_modes first to discover available mode ids.",
    inputSchema: { mode_id: z.string().describe("The mode id to retrieve") },
  },
  async ({ mode_id }) => {
    try {
      const data = await apiGet(`/api/mode?id=${encodeURIComponent(mode_id)}`);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_set_mode",
  {
    description: "Replace the entire definition of a single mode. Atomic: does not affect other modes, Wi-Fi, or defaults. Read the mode first with walkey_get_mode to avoid losing existing bindings.",
    inputSchema: {
      mode_id: z.string().describe("The mode id to update"),
      mode: z.record(z.any()).describe("Complete mode object: {id, label, cycleOrder, bindings:[...]}"),
    },
  },
  async ({ mode_id, mode }) => {
    try {
      const data = await apiPut(`/api/mode?id=${encodeURIComponent(mode_id)}`, mode);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_create_mode",
  {
    description: "Create a new mode with a unique id. See walkey_set_binding description for available triggers and action types.",
    inputSchema: {
      mode: z.record(z.any()).describe("Mode object: {id, label, cycleOrder, bindings:[{input, trigger, actions:[...]}]}. Use empty bindings:[] and add them with walkey_set_binding."),
    },
  },
  async ({ mode }) => {
    try {
      const data = await apiPost("/api/mode", mode);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_delete_mode",
  {
    description: "Delete a mode by id. Fails if the mode is currently active. Switch to a different mode first with walkey_set_active_mode.",
    inputSchema: { mode_id: z.string().describe("The mode id to delete") },
  },
  async ({ mode_id }) => {
    try {
      const data = await apiDelete(`/api/mode?id=${encodeURIComponent(mode_id)}`);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Binding-level helpers ---

server.registerTool(
  "walkey_get_bindings",
  {
    description: "Retrieve all bindings for a mode. Returns array of {input, trigger, actions} objects.",
    inputSchema: { mode_id: z.string().describe("The mode id to get bindings for") },
  },
  async ({ mode_id }) => {
    try {
      const mode = await apiGet(`/api/mode?id=${encodeURIComponent(mode_id)}`);
      return ok(mode.bindings || []);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_set_binding",
  {
    description: `Add or replace a single binding in a mode. Identifies by input+trigger pair. Reads the mode, patches the binding, writes back.

BINDING STRUCTURE: {input:"<input>", trigger:"<trigger>", actions:[<action>, ...]}
  input is one of: touch, boot_button (only these two are active in current firmware; encoder, usb_host_key, timer, imu are reserved for future use)

TRIGGERS (one per binding):
  Touch: tap, double_tap, long_press, hold_start, hold_end, swipe_up, swipe_down, swipe_left, swipe_right
  Button: press, release, tap, double_tap, long_press, hold_start, hold_end

ACTION TYPES (one or more per binding):
  HID keys:
    hid_key_tap     - press+release a key:      {type:"hid_key_tap", key:"ENTER"}
    hid_key_down    - press and hold a key:      {type:"hid_key_down", key:"SPACE"}
    hid_key_up      - release a held key:        {type:"hid_key_up", key:"SPACE"}
    hid_shortcut_tap - key with modifiers:       {type:"hid_shortcut_tap", key:"S", modifiers:["CTRL"]}
    hid_modifier_down - hold a modifier:         {type:"hid_modifier_down", modifier:"SHIFT"}
    hid_modifier_up   - release a modifier:      {type:"hid_modifier_up", modifier:"SHIFT"}
  Microphone:
    mic_gate        - SET mic to explicit state:  {type:"mic_gate", enabled:true} or {enabled:false}
    mic_gate_toggle - TOGGLE mic on/off:          {type:"mic_gate_toggle"}
  UI:
    ui_hint         - show text on screen:        {type:"ui_hint", text:"Hello"}
  Timing:
    sleep_ms        - delay between actions:      {type:"sleep_ms", duration_ms:20}
  Mode switching:
    set_mode        - switch to a named mode:     {type:"set_mode", mode:"<mode_id>"}
    cycle_mode      - cycle to the next mode:     {type:"cycle_mode"}
  Mouse overlay (use mouse within any mode without switching to dedicated Mouse mode):
    mouse_on        - activate mouse overlay:     {type:"mouse_on"}
                      optional: mouseType ("airMouse" or "touchMouse"), tracking (bool, default true)
                      tracking:true (default) = cursor moves immediately, no click on release
                      tracking:false = full gesture handler (long-press=cursor, tap=click, etc.)
    mouse_off       - deactivate mouse overlay:   {type:"mouse_off"} -- stops tracking, releases buttons
    mouse_toggle    - toggle overlay on/off:       {type:"mouse_toggle"} -- same optional fields as mouse_on
  Other:
    noop            - do nothing:                 {type:"noop"}

RULES:
- "toggle mic" or "toggle microphone" -> use mic_gate_toggle, NOT mic_gate with enabled:true
- mic_gate is ONLY for explicit on/off (e.g. hold_start=on, hold_end=off)
- Do NOT combine unrelated actions (e.g. mic toggle + unrelated HID key) unless the user specifically asks
- Each action type has its own required fields as shown above; do not invent fields`,
    inputSchema: {
      mode_id: z.string().describe("The mode id to add the binding to"),
      binding: z.record(z.any()).describe("Binding: {input:'touch', trigger:'<trigger>', actions:[{type:'<action_type>', ...}]}"),
    },
  },
  async ({ mode_id, binding }) => {
    try {
      const mode = await apiGet(`/api/mode?id=${encodeURIComponent(mode_id)}`);
      const bindings = mode.bindings || [];
      const idx = bindings.findIndex(
        (b) => b.input === binding.input && b.trigger === binding.trigger
      );
      if (idx >= 0) {
        bindings[idx] = binding;
      } else {
        bindings.push(binding);
      }
      mode.bindings = bindings;
      const data = await apiPut(`/api/mode?id=${encodeURIComponent(mode_id)}`, mode);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_remove_binding",
  {
    description: "Remove a binding from a mode by its input+trigger pair. Use walkey_get_bindings first to see existing bindings.",
    inputSchema: {
      mode_id: z.string().describe("The mode id"),
      input: z.string().describe("Normalized input, e.g. 'touch' or 'boot_button'"),
      trigger: z.string().describe("Normalized trigger, e.g. 'tap', 'double_tap', 'long_press', 'hold_start', 'hold_end', 'swipe_left'"),
    },
  },
  async ({ mode_id, input, trigger }) => {
    try {
      const mode = await apiGet(`/api/mode?id=${encodeURIComponent(mode_id)}`);
      const before = (mode.bindings || []).length;
      mode.bindings = (mode.bindings || []).filter(
        (b) => !(b.input === input && b.trigger === trigger)
      );
      if (mode.bindings.length === before) {
        return err(`Binding ${input}/${trigger} not found in mode ${mode_id}`);
      }
      const data = await apiPut(`/api/mode?id=${encodeURIComponent(mode_id)}`, mode);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Wi-Fi ---

server.registerTool(
  "walkey_get_wifi",
  { description: "Retrieve current Wi-Fi settings (read-only). Returns {sta:{ssid, password}, ap:{ssid, password}, hostname, localUrl}. Wi-Fi cannot be changed through MCP because a bad write would disconnect the device. To change Wi-Fi: edit the wifi section in /spiffs/mode-config.json via the config portal at http://walkey-talkey.local/, or connect to the fallback AP (SSID: walkey-talkey, password: secretKEY) at http://192.168.4.1/ if the device is unreachable." },
  async () => {
    try {
      const data = await apiGet("/api/wifi");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Defaults ---

server.registerTool(
  "walkey_get_defaults",
  { description: `Retrieve all defaults: touch timing, mouse backend selection, and per-backend mouse config.

RESPONSE SHAPE:
  touch: {holdMs, doubleTapMs, swipeMinDistance}
  defaultMouse: "airMouse" | "touchMouse"
  airMouse: {sensitivity, deadZoneDps, easingExponent, maxDps, emaAlpha, rewindDepth, rewindDecay, calibrationSamples}
  touchMouse: {sensitivity, moveThresholdPx, tapDragWindowMs}

AIR MOUSE FIELDS (all optional, defaults shown):
  sensitivity (1.0)        - cursor speed multiplier
  deadZoneDps (6.0)        - gyro degrees/sec below which input is ignored
  easingExponent (1.25)    - power curve: 1.0=linear, higher=more precision at low speed
  maxDps (300.0)           - gyro saturation point in degrees/sec
  emaAlpha (0.35)          - EMA smoothing 0..1; lower=smoother but laggier
  rewindDepth (12)         - samples rewound on release to cancel jitter (max 16)
  rewindDecay (0.7)        - exponential decay for rewind weighting
  calibrationSamples (128) - IMU samples averaged at startup for drift compensation

TOUCH MOUSE FIELDS (all optional, defaults shown):
  sensitivity (1.0)        - cursor speed multiplier
  moveThresholdPx (5)      - min pixels before cursor starts tracking
  tapDragWindowMs (180)    - ms window after tap to detect tap-and-drag` },
  async () => {
    try {
      const data = await apiGet("/api/defaults");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_set_defaults",
  {
    description: `Update defaults. Merge semantics: only provided fields change.

SUPPORTED FIELDS:
  touch:        {holdMs, doubleTapMs, swipeMinDistance}
  defaultMouse: "airMouse" | "touchMouse" - which mouse backend is active
  airMouse:     {sensitivity, deadZoneDps, easingExponent, maxDps, emaAlpha, rewindDepth, rewindDecay, calibrationSamples}
  touchMouse:   {sensitivity, moveThresholdPx, tapDragWindowMs}

All airMouse/touchMouse fields are optional; omitted fields keep current values.`,
    inputSchema: {
      defaults: z
        .record(z.any())
        .describe("Defaults to update, e.g. {touch:{holdMs:500}}, {defaultMouse:'touchMouse'}, {airMouse:{sensitivity:1.5, deadZoneDps:8.0}}, {touchMouse:{moveThresholdPx:10}}"),
    },
  },
  async ({ defaults }) => {
    try {
      const data = await apiPut("/api/defaults", defaults);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Recording ---

server.registerTool(
  "walkey_get_recording",
  {
    description:
      "Retrieve current SD card recording settings. Returns {enabled, format}. When enabled, mic_gate activations also write WAV files to the SD card at /sdcard/recordings/<modeId>/<sessionId>_<uptimeSec>.wav.",
  },
  async () => {
    try {
      const data = await apiGet("/api/recording");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_set_recording",
  {
    description:
      "Enable or disable SD card recording. When enabled, every mic_gate activation writes a WAV file (48kHz/16-bit/mono) to the SD card. Files are saved at /sdcard/recordings/<modeId>/<sessionId>_<uptimeSec>.wav. Merge semantics: only provided fields change.",
    inputSchema: {
      recording: z
        .record(z.any())
        .describe(
          'Recording settings, e.g. {enabled: true} or {enabled: false}'
        ),
    },
  },
  async ({ recording }) => {
    try {
      const data = await apiPut("/api/recording", recording);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Active mode ---

server.registerTool(
  "walkey_get_active_mode",
  { description: "Retrieve which mode is currently active on the device. Returns {activeMode: '<mode_id>'}." },
  async () => {
    try {
      const cfg = await apiGet("/config");
      return ok({ activeMode: cfg.activeMode });
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_set_active_mode",
  {
    description: "Switch the device to a different mode. Use walkey_list_modes to discover available mode ids. 'mouse' is a built-in mode that uses either gyro-based air mouse or touch trackpad depending on the defaultMouse setting in defaults (configurable via walkey_set_defaults).",
    inputSchema: { mode_id: z.string().describe("The mode id to switch to (includes built-in 'mouse' mode)") },
  },
  async ({ mode_id }) => {
    try {
      const data = await apiPut("/api/active-mode", { activeMode: mode_id });
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Boot mode ---

server.registerTool(
  "walkey_get_boot_mode",
  { description: "Retrieve the BOOT menu definition (bootMode). The BOOT menu is the temporary control layer active while the BOOT button is held, used for mode-switching gestures and overlay UI." },
  async () => {
    try {
      const data = await apiGet("/api/boot-mode");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Global bindings ---

server.registerTool(
  "walkey_get_global_bindings",
  { description: "Retrieve global bindings that apply across all modes. Default: BOOT button press enters boot mode overlay, release exits it. These ensure mode-switching always works regardless of active mode." },
  async () => {
    try {
      const data = await apiGet("/api/global-bindings");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Escape-hatch tools ---

server.registerTool(
  "walkey_set_config",
  {
    description: "Replace the ENTIRE device configuration. DESTRUCTIVE: overwrites all modes, Wi-Fi, defaults. Prefer per-mode/per-section tools (walkey_set_mode, walkey_set_defaults, etc.). Read current config first with walkey_get_config.",
    inputSchema: {
      config: z.record(z.any()).describe("Complete config JSON. Required keys: version (1), globalBindings, bootMode, modes. Optional: activeMode, defaults, wifi. See walkey_get_config for current shape."),
    },
  },
  async ({ config }) => {
    try {
      const data = await apiPut("/config", config);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_validate_config",
  {
    description: "Validate a configuration JSON without saving it. Returns validation errors if any. Use before walkey_set_config.",
    inputSchema: {
      config: z.record(z.any()).describe("Configuration JSON to validate (not saved)"),
    },
  },
  async ({ config }) => {
    try {
      const data = await apiPost("/config/validate", config);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_reset_config",
  { description: "Reset the device to its built-in factory default configuration. DESTRUCTIVE: erases all custom modes and settings." },
  async () => {
    try {
      const data = await apiPost("/config/reset", {});
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Recording file tools ---

server.registerTool(
  "walkey_list_recordings",
  { description: "List all audio recordings on the SD card. Returns path and size for each WAV file under /sdcard/recordings/." },
  async () => {
    try {
      const data = await apiGet("/api/recordings");
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

server.registerTool(
  "walkey_download_recording",
  {
    description: "Download a recording WAV file from the device. Returns the download URL. Use walkey_list_recordings first to get available file paths.",
    inputSchema: {
      file: z.string().describe("Relative path of the recording, e.g. 'whisper/ABC123_00042.wav'"),
    },
  },
  async ({ file }) => {
    const url = `${DEVICE_URL}/api/recordings/download?file=${file}`;
    return { content: [{ type: "text", text: `Download URL: ${url}\n\nUse this URL to download the WAV file directly.` }] };
  }
);

server.registerTool(
  "walkey_delete_recording",
  {
    description: "Delete a recording WAV file from the SD card. Use walkey_list_recordings first to get available file paths.",
    inputSchema: {
      file: z.string().describe("Relative path of the recording to delete, e.g. 'whisper/ABC123_00042.wav'"),
    },
  },
  async ({ file }) => {
    try {
      const data = await apiGet(`/api/recordings/delete?file=${file}`);
      return ok(data);
    } catch (e) {
      return err(e.message);
    }
  }
);

// --- Start ---

const transport = new StdioServerTransport();
await server.connect(transport);
