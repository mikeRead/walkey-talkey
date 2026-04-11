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

## Available Tools (22)

### Discovery
- `walkey.ping` - Check device reachability
- `walkey.get_config` - Get full configuration
- `walkey.get_config_canonical` - Get schema-ordered configuration
- `walkey.get_schema` - Get JSON config schema

### Mode Management
- `walkey.list_modes` - List all modes summary
- `walkey.get_mode` - Get single mode by id
- `walkey.set_mode` - Replace entire mode definition
- `walkey.create_mode` - Add a new mode
- `walkey.delete_mode` - Remove a mode

### Binding Management
- `walkey.get_bindings` - Get bindings for a mode
- `walkey.set_binding` - Add/replace a binding in a mode
- `walkey.remove_binding` - Remove a binding from a mode

### Wi-Fi
- `walkey.get_wifi` - Get Wi-Fi settings (read-only)

### Defaults
- `walkey.get_defaults` - Get touch defaults
- `walkey.set_defaults` - Update touch defaults

### Active Mode
- `walkey.get_active_mode` - Get current active mode
- `walkey.set_active_mode` - Switch active mode

### BOOT Menu
- `walkey.get_boot_mode` - Get BOOT menu definition

### Global
- `walkey.get_global_bindings` - Get global bindings

### Escape Hatch
- `walkey.set_config` - Replace entire configuration
- `walkey.validate_config` - Validate without saving
- `walkey.reset_config` - Factory reset

## How It Works

This Node.js server communicates with the Walkey-Talkey device over HTTP via its REST API at `http://walkey-talkey.local`. AI clients spawn it as a child process using stdio transport (JSON-RPC 2.0 over stdin/stdout).

Per-mode tools use atomic read-modify-write: they read the mode, patch it, and write it back. Defaults use merge semantics: only provided fields are updated. Wi-Fi is read-only through MCP to prevent accidental disconnection; edit Wi-Fi via the config portal instead.

## License

MIT
