# walkey-talkey-mcp

Model Context Protocol (MCP) server for configuring the Walkey-Talkey ESP32-S3 device from AI clients like Cursor and Claude Code.

## Quick Start

No installation needed -- `npx` handles everything:

```bash
npx walkey-talkey-mcp
```

## Cursor Setup

Add to `.cursor/mcp.json`:

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

## Claude Code Setup

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

## Environment Variables

| Variable | Default | Description |
|---|---|---|
| `WALKEY_URL` | `http://walkey-talkey.local` | Base URL of the device |

Override the device URL when needed:

```json
{
  "mcpServers": {
    "walkey-talkey": {
      "command": "npx",
      "args": ["walkey-talkey-mcp"],
      "env": { "WALKEY_URL": "http://192.168.0.43" }
    }
  }
}
```

## Available Tools (27)

### Discovery
- `walkey_ping` - Check device reachability
- `walkey_get_config` - Get full configuration
- `walkey_get_config_canonical` - Get schema-ordered configuration
- `walkey_get_schema` - Get JSON config schema

### Mode Management
- `walkey_list_modes` - List all modes summary
- `walkey_get_mode` - Get single mode by id
- `walkey_set_mode` - Replace entire mode definition
- `walkey_create_mode` - Add a new mode
- `walkey_delete_mode` - Remove a mode

### Binding Management
- `walkey_get_bindings` - Get bindings for a mode
- `walkey_set_binding` - Add/replace a binding in a mode
- `walkey_remove_binding` - Remove a binding from a mode

### Wi-Fi
- `walkey_get_wifi` - Get Wi-Fi settings (read-only)

### Defaults
- `walkey_get_defaults` - Get touch defaults
- `walkey_set_defaults` - Update touch defaults

### Recording Config
- `walkey_get_recording` - Get SD card recording settings
- `walkey_set_recording` - Enable/disable SD card recording

### Recording Files
- `walkey_list_recordings` - List all recording WAV files
- `walkey_download_recording` - Get download URL for a recording
- `walkey_delete_recording` - Delete a recording from SD card

### Active Mode
- `walkey_get_active_mode` - Get current active mode
- `walkey_set_active_mode` - Switch active mode

### BOOT Menu
- `walkey_get_boot_mode` - Get BOOT menu definition

### Global
- `walkey_get_global_bindings` - Get global bindings

### Escape Hatch
- `walkey_set_config` - Replace entire configuration
- `walkey_validate_config` - Validate without saving
- `walkey_reset_config` - Factory reset

## How It Works

This Node.js server communicates with the Walkey-Talkey device over HTTP via its REST API at `http://walkey-talkey.local`. AI clients spawn it as a child process using stdio transport (JSON-RPC 2.0 over stdin/stdout).

Per-mode tools use atomic read-modify-write: they read the mode, patch it, and write it back. Defaults use merge semantics: only provided fields are updated. Wi-Fi is read-only through MCP to prevent accidental disconnection; edit Wi-Fi via the config portal instead.

## License

MIT
