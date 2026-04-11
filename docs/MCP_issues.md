# MCP Server Implementation -- Progress and Issues

This document records the attempt to add a native MCP (Model Context Protocol) server
directly on the ESP32-S3 firmware so that AI IDEs (Cursor, Claude Code) can read and
modify the device configuration over HTTP without copy/paste workflows.

All MCP code has been **reverted** from the working tree. The clean (pre-MCP) firmware
has been rebuilt and flashed. This document exists so the work can be resumed later
without repeating dead ends.

---

## Goal

Expose a `POST /mcp` endpoint on the existing `esp_http_server` that speaks
JSON-RPC 2.0 (MCP Streamable HTTP transport). AI clients connect to
`http://walkey-talkey.local/mcp` and can discover tools, read config, and
push granular updates -- all without the user touching JSON.

---

## What Was Built

| Item | Detail |
|---|---|
| SDK | `espressif/mcp-c-sdk ^1.0.0` added to `idf_component.yml` |
| Transport | "Null transport" -- the SDK's built-in HTTP server is bypassed; requests are routed from the existing `esp_http_server` into `esp_mcp_mgr_req_handle()` |
| Endpoint | `POST /mcp` handler in `config_http_server.c` |
| Tools | 22 tools under the `walkey.` namespace (discovery, binding-level, mode-level, settings, escape hatches, reference docs) |
| Server module | `main/mcp_server.c` + `main/mcp_server.h` (~920 lines) |
| Test harness | `tests/mcp_server_test.ps1` (PowerShell) and `tests/mcp_quick_test.py` (Python raw-socket) |
| Docs | MCP setup instructions were added to `docs/AI_GUIDE.md` and `docs/USER_GUIDE.md` (Cursor and Claude Code configs) |

### Tool List

All tools used the `walkey.` prefix:

**Discovery (read-only):**
- `walkey.list_modes`, `walkey.get_mode`, `walkey.get_binding`
- `walkey.get_wifi`, `walkey.get_defaults`, `walkey.get_boot_mode`, `walkey.get_global_bindings`

**Binding-level (write):**
- `walkey.set_binding`, `walkey.remove_binding`
- `walkey.set_boot_binding`, `walkey.set_global_binding`

**Mode-level (write):**
- `walkey.add_mode`, `walkey.remove_mode`, `walkey.set_active_mode`

**Settings:**
- `walkey.update_wifi_sta`, `walkey.update_wifi_ap`, `walkey.update_touch_defaults`

**Escape hatches:**
- `walkey.get_config`, `walkey.save_config`, `walkey.validate_config`, `walkey.reset_config`

**Utility / Reference:**
- `walkey.ping`, `walkey.get_schema`, `walkey.get_ai_guide`, `walkey.get_user_guide`

---

## What Worked

1. **SDK integration and compilation** -- `espressif/mcp-c-sdk` downloaded, linked,
   and compiled cleanly with the existing project. No source-level conflicts.

2. **MCP `initialize`** -- The JSON-RPC `initialize` handshake returned a correct
   response with server info and protocol version `2024-11-05`.

3. **Small tool calls** -- `walkey.ping`, `walkey.get_defaults`, and `walkey.get_wifi`
   returned valid JSON-RPC responses when tested with Python raw sockets.

4. **Chunked transfer encoding** -- Responses were sent via the existing
   `config_http_send_chunk_sequence()` helper and parsed correctly by clients.

5. **Cursor / Claude Code client config** -- The `mcp.json` (Cursor) and
   `claude mcp add` (Claude Code) setup was documented and syntactically valid.

---

## What Failed and Why

### 1. `tools/list` crashes the HTTP server task

The `tools/list` response contains all 22 tool definitions with names, descriptions,
and JSON input schemas. This JSON payload is several KB. Building it via cJSON inside
`esp_mcp_mgr_req_handle()` exceeded the HTTP server task's 6144-byte stack, causing a
stack overflow / watchdog reset. Small responses (like `initialize` at ~200 bytes)
worked fine on the same stack.

### 2. `null_create_cfg` returning NULL

The null transport's `create_config` callback initially returned `NULL` via `*o = NULL`.
The SDK's `esp_mcp_mgr_init()` checks `if (!mcp_ctx->transport_config)` and fails.
**Fix applied:** return `(void *)1` as a non-NULL dummy pointer. This resolved the
initial HTTP 500 errors.

### 3. Worker task via `xTaskCreate` -- not enough internal RAM

To avoid blowing the HTTP server stack, MCP processing was offloaded to a temporary
FreeRTOS task with a 12288-byte stack. However, `xTaskCreate` allocates from internal
SRAM, and there was not enough contiguous free memory. The task creation failed at
runtime with "Task create failed" (HTTP 500).

### 4. Worker task via `xTaskCreateWithCaps(MALLOC_CAP_SPIRAM)` -- untested

The next iteration used `xTaskCreateWithCaps()` to allocate the worker stack from
PSRAM (8 MB available). This compiled successfully but was **never tested on the
device** because flashing difficulties prevented verification before the revert.

### 5. Global `cJSON_InitHooks` broke everything

An early attempt called `cJSON_InitHooks()` in `mcp_server_init()` to redirect all
cJSON allocations to PSRAM. This is a **global** setting that affected every cJSON
user in the firmware (config API, HTTP handlers, etc.). The device became completely
unreachable over HTTP after this change. Reverted.

### 6. Bumping `CONFIG_HTTP_SERVER_STACK_SIZE` + sdkconfig tunables

Increasing the HTTP server stack to 8192 or 10240 bytes, combined with adding
`CONFIG_MCP_TOOLLIST_MAX_SIZE=16384` and `CONFIG_MCP_TOOLCALL_STACK_SIZE=12288` to
`sdkconfig.defaults`, made the device unreachable. It was unclear whether the stack
increase alone caused the issue or the combination with the other changes.

### 7. PowerShell `Invoke-WebRequest` cannot parse chunked responses

PowerShell's `Invoke-WebRequest` and `Invoke-RestMethod` failed to parse the ESP32's
chunked transfer encoding responses (they hung or returned empty). This is a **test
tooling issue**, not a firmware bug. Python raw sockets worked correctly.

### 8. COM4 flashing difficulties

The ESP32-S3's USB-Serial/JTAG port (COM4) frequently became unavailable after
flashing due to DTR/RTS signals putting the chip into download mode. Physical
unplug/replug of the USB cable was required between flash cycles.

---

## Approaches Tried (Chronological)

| # | Approach | Result |
|---|---|---|
| 1 | Direct `mcp_server_handle_request()` call on HTTP server task (6144 stack) | Small responses OK; `tools/list` stack overflow |
| 2 | Bump stack to 8192/10240 + `cJSON_InitHooks` + sdkconfig MCP tunables | Device unreachable over HTTP |
| 3 | Revert hooks + sdkconfig, keep stack at 6144, add chunked transfer | Small calls work, large crash |
| 4 | Worker task via `xTaskCreate` with 12288-byte internal RAM stack | Runtime failure: "Task create failed" (no RAM) |
| 5 | Worker task via `xTaskCreateWithCaps(MALLOC_CAP_SPIRAM)` with 12288-byte PSRAM stack | Compiled OK; **never tested on device** |

---

## Recommended Next Steps

1. **Test approach 5 (PSRAM worker task)** -- This is the most promising path. The
   code compiled cleanly and PSRAM has plenty of room for a 12288+ byte stack. Flash
   and verify that `tools/list` succeeds.

2. **If stack is still insufficient, increase to 16384 or 20480 in PSRAM** -- There is
   approximately 8 MB of PSRAM; even 20 KB per MCP request is negligible.

3. **Reduce tool count or description length** -- The `tools/list` payload could be
   shrunk by shortening descriptions or splitting tools into groups. This reduces
   pressure on both stack and response size.

4. **Use Python for testing, not PowerShell** -- The `tests/mcp_quick_test.py` pattern
   (raw TCP sockets) works reliably. PowerShell's HTTP client cannot handle the ESP32's
   chunked encoding.

5. **Consider a hybrid approach** -- A lightweight Node.js MCP server running on the
   host computer could proxy requests to the ESP32's existing REST API (`GET /config`,
   `PUT /config`, etc.). This avoids all embedded memory constraints while still giving
   AI IDEs full config access. The trade-off is requiring Node.js on the user's machine.

6. **Isolate stack size testing** -- When resuming, change only one variable at a time.
   The previous failure was likely caused by `cJSON_InitHooks` or sdkconfig tunables,
   not the stack size increase itself. Test stack increases in isolation.

---

## Files That Were Created (Now Deleted)

These files existed during the implementation and were removed during the revert. They
can be recreated from the conversation history if needed:

- `main/mcp_server.c` -- MCP server core (22 tool callbacks, null transport, init)
- `main/mcp_server.h` -- Public API (`mcp_server_init`, `mcp_server_handle_request`)
- `main/mcp_server.Readme.md` -- Module documentation
- `tests/mcp_server_test.ps1` -- PowerShell integration test suite
- `tests/mcp_quick_test.py` -- Python raw-socket integration tests

## Files That Were Modified (Now Reverted)

- `main/config_http_server.c` -- Added `/mcp` POST handler + worker task
- `main/main.c` -- Added `mcp_server_init()` call before HTTP server start
- `main/CMakeLists.txt` -- Added `mcp_server.c` to SRCS
- `main/idf_component.yml` -- Added `espressif/mcp-c-sdk: '^1.0.0'`
- `dependencies.lock` -- Updated with mcp-c-sdk dependency tree
- `sdkconfig.defaults` -- Temporarily added MCP Kconfig tunables (later removed)
- `docs/AI_GUIDE.md` -- Added MCP setup for Cursor and Claude Code
- `docs/USER_GUIDE.md` -- Added MCP setup instructions
- `tests/README.md` -- Added MCP test documentation

## Managed Component (Still Present)

`managed_components/espressif__mcp-c-sdk/` was downloaded by the component manager
and is gitignored. It remains on disk but is unused without the `idf_component.yml`
reference. It can be safely deleted or will be re-downloaded if the dependency is
re-added later.
