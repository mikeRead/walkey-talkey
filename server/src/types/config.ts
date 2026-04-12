export interface Action {
  type: string;
  [key: string]: unknown;
}

export interface Binding {
  input: string;
  trigger: string;
  actions: Action[];
}

export interface Mode {
  id: string;
  label: string;
  cycleOrder: number;
  bindings: Binding[];
  builtIn?: boolean;
}

export interface ModeSummary {
  id: string;
  label: string;
  cycleOrder: number;
  bindingCount: number;
  builtIn?: boolean;
}

export interface TouchDefaults {
  holdMs: number;
  doubleTapMs: number;
  swipeMinDistance: number;
}

export interface AirMouseDefaults {
  sensitivity: number;
  deadZoneDps: number;
  easingExponent: number;
  maxDps: number;
  emaAlpha: number;
  rewindDepth: number;
  rewindDecay: number;
  calibrationSamples: number;
}

export interface TouchMouseDefaults {
  sensitivity: number;
  moveThresholdPx: number;
  tapDragWindowMs: number;
}

export interface Defaults {
  touch: TouchDefaults;
  defaultMouse: "airMouse" | "touchMouse";
  airMouse: AirMouseDefaults;
  touchMouse: TouchMouseDefaults;
}

export interface WifiConfig {
  sta: { ssid: string; password: string };
  ap: { ssid: string; password: string };
  hostname: string;
  localUrl: string;
}

export interface BootModeUI {
  title: string;
  subtitle: string;
  showModeList: boolean;
  showGestureHints: boolean;
  showCurrentModeCard: boolean;
}

export interface BootMode {
  label: string;
  ui: BootModeUI;
  bindings: Binding[];
}

export interface DeviceConfig {
  version: number;
  activeMode: string;
  defaults: Defaults;
  wifi: WifiConfig;
  globalBindings: Binding[];
  bootMode: BootMode;
  modes: Mode[];
}

export interface RecordingSettings {
  enabled: boolean;
  format?: string;
}

export interface RecordingFile {
  path: string;
  size: number;
}

export interface LogEntry {
  type: string;
  message: string;
  runtime: number;
  source: "device" | "web";
}

// Action type metadata for the editor
export const ACTION_TYPES: { value: string; label: string; fields: string[] }[] = [
  { value: "hid_key_tap", label: "Key Tap", fields: ["key"] },
  { value: "hid_key_down", label: "Key Down", fields: ["key"] },
  { value: "hid_key_up", label: "Key Up", fields: ["key"] },
  { value: "hid_shortcut_tap", label: "Shortcut", fields: ["key", "modifiers"] },
  { value: "hid_modifier_down", label: "Modifier Down", fields: ["modifier"] },
  { value: "hid_modifier_up", label: "Modifier Up", fields: ["modifier"] },
  { value: "hid_usage_tap", label: "HID Usage Tap", fields: ["usage"] },
  { value: "mic_gate", label: "Mic Gate", fields: ["enabled"] },
  { value: "mic_gate_toggle", label: "Mic Toggle", fields: [] },
  { value: "ui_hint", label: "UI Hint", fields: ["text"] },
  { value: "sleep_ms", label: "Sleep (ms)", fields: ["duration_ms"] },
  { value: "set_mode", label: "Set Mode", fields: ["mode"] },
  { value: "cycle_mode", label: "Cycle Mode", fields: [] },
  { value: "mouse_on", label: "Mouse On", fields: ["mouseType", "tracking"] },
  { value: "mouse_off", label: "Mouse Off", fields: [] },
  { value: "mouse_toggle", label: "Mouse Toggle", fields: ["mouseType", "tracking"] },
  { value: "noop", label: "No-op", fields: [] },
];

export const INPUT_TYPES = ["touch", "boot_button"] as const;

export const TOUCH_TRIGGERS = [
  "tap", "double_tap", "long_press", "hold_start", "hold_end",
  "swipe_up", "swipe_down", "swipe_left", "swipe_right",
] as const;

export const BUTTON_TRIGGERS = [
  "press", "release", "tap", "double_tap", "long_press", "hold_start", "hold_end",
] as const;

export const COMMON_KEYS = [
  "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
  "0","1","2","3","4","5","6","7","8","9",
  "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","F13","F14","F15","F16","F17","F18","F19","F20","F21","F22","F23","F24",
  "ENTER","ESC","BACKSPACE","TAB","SPACE","DELETE",
  "HOME","END","PAGE_UP","PAGE_DOWN",
  "UP_ARROW","DOWN_ARROW","LEFT_ARROW","RIGHT_ARROW",
  "PRINT_SCREEN","SCROLL_LOCK","PAUSE",
] as const;

export const MODIFIERS = ["CTRL", "SHIFT", "ALT", "GUI"] as const;

export const MEDIA_USAGES = [
  "MEDIA_PLAY_PAUSE", "MEDIA_NEXT_TRACK", "MEDIA_PREV_TRACK",
  "MEDIA_STOP", "VOLUME_UP", "VOLUME_DOWN", "MUTE",
] as const;
