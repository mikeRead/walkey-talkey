#!/usr/bin/env python3
"""
Integration tests for the Walkey-Talkey granular REST API and MCP server.

Usage:
    python tests/mcp_test.py                       # firmware API tests only
    python tests/mcp_test.py --mcp D:/mcp/index.js # include MCP stdio tests
"""

import argparse
import json
import subprocess
import sys
import time
import urllib.error
import urllib.request

BASE = "http://walkey-talkey.local"
TIMEOUT = 10


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def api_get(path):
    url = f"{BASE}{path}"
    req = urllib.request.Request(url, method="GET")
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        return r.status, json.loads(r.read().decode())


def api_put(path, body):
    url = f"{BASE}{path}"
    data = json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, method="PUT",
                                headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        return r.status, json.loads(r.read().decode())


def api_post(path, body):
    url = f"{BASE}{path}"
    data = json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, method="POST",
                                headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        return r.status, json.loads(r.read().decode())


def api_delete(path):
    url = f"{BASE}{path}"
    req = urllib.request.Request(url, method="DELETE")
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        return r.status, json.loads(r.read().decode())


passed = 0
failed = 0

def check(name, condition, detail=""):
    global passed, failed
    if condition:
        passed += 1
        print(f"  PASS: {name}")
    else:
        failed += 1
        print(f"  FAIL: {name} {detail}")


# ---------------------------------------------------------------------------
# Firmware API tests
# ---------------------------------------------------------------------------

def test_ping():
    print("\n--- Ping ---")
    try:
        req = urllib.request.Request(f"{BASE}/ping")
        with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
            body = r.read().decode()
        check("ping returns 200", r.status == 200)
        check("ping body is 'ok'", body.strip() == "ok")
    except Exception as e:
        check("ping reachable", False, str(e))


def test_get_modes():
    print("\n--- GET /api/modes ---")
    try:
        status, data = api_get("/api/modes")
        check("status 200", status == 200)
        check("returns array", isinstance(data, list))
        check("at least 1 mode", len(data) >= 1)
        if data:
            m = data[0]
            check("mode has id", "id" in m)
            check("mode has label", "label" in m)
            check("mode has bindingCount", "bindingCount" in m)
    except Exception as e:
        check("modes reachable", False, str(e))


def test_get_single_mode():
    print("\n--- GET /api/mode?id=cursor ---")
    try:
        status, data = api_get("/api/mode?id=cursor")
        check("status 200", status == 200)
        check("id is cursor", data.get("id") == "cursor")
        check("has bindings array", isinstance(data.get("bindings"), list))
    except Exception as e:
        check("mode reachable", False, str(e))


def test_get_wifi():
    print("\n--- GET /api/wifi ---")
    try:
        status, data = api_get("/api/wifi")
        check("status 200", status == 200)
        check("has sta", "sta" in data)
        check("sta has ssid", "ssid" in data.get("sta", {}))
    except Exception as e:
        check("wifi reachable", False, str(e))


def test_get_defaults():
    print("\n--- GET /api/defaults ---")
    try:
        status, data = api_get("/api/defaults")
        check("status 200", status == 200)
        check("has touch", "touch" in data)
        check("touch has holdMs", "holdMs" in data.get("touch", {}))
    except Exception as e:
        check("defaults reachable", False, str(e))


def test_get_boot_mode():
    print("\n--- GET /api/boot-mode ---")
    try:
        status, data = api_get("/api/boot-mode")
        check("status 200", status == 200)
        check("has label", "label" in data)
        check("has bindings", "bindings" in data)
    except Exception as e:
        check("boot-mode reachable", False, str(e))


def test_get_global_bindings():
    print("\n--- GET /api/global-bindings ---")
    try:
        status, data = api_get("/api/global-bindings")
        check("status 200", status == 200)
        check("returns array", isinstance(data, list))
    except Exception as e:
        check("global-bindings reachable", False, str(e))


def test_put_defaults_merge():
    print("\n--- PUT /api/defaults (merge) ---")
    try:
        _, before = api_get("/api/defaults")
        old_hold = before.get("touch", {}).get("holdMs", 400)
        new_hold = old_hold + 50

        status, result = api_put("/api/defaults", {"touch": {"holdMs": new_hold}})
        check("put status 200", status == 200)
        check("put returns ok", result.get("ok") is True or "ok" in str(result))

        _, after = api_get("/api/defaults")
        check("holdMs updated", after.get("touch", {}).get("holdMs") == new_hold)
        check("doubleTapMs preserved",
              after.get("touch", {}).get("doubleTapMs") == before.get("touch", {}).get("doubleTapMs"))

        # Restore original
        api_put("/api/defaults", {"touch": {"holdMs": old_hold}})
    except Exception as e:
        check("defaults merge", False, str(e))


def test_put_active_mode():
    print("\n--- PUT /api/active-mode ---")
    try:
        _, resp = api_get("/config")
        cfg = resp.get("config", resp)
        original = cfg.get("activeMode", "cursor")

        _, modes_list = api_get("/api/modes")
        if len(modes_list) < 2:
            check("need 2+ modes to test", False)
            return

        other = [m["id"] for m in modes_list if m["id"] != original][0]

        status, result = api_put("/api/active-mode", {"activeMode": other})
        check("put status 200", status == 200)

        time.sleep(2)
        _, resp2 = api_get("/config")
        cfg2 = resp2.get("config", resp2)
        check("active mode changed", cfg2.get("activeMode") == other)

        # Restore
        api_put("/api/active-mode", {"activeMode": original})
        time.sleep(1)
    except Exception as e:
        check("active-mode", False, str(e))


# ---------------------------------------------------------------------------
# MCP stdio tests
# ---------------------------------------------------------------------------

def test_mcp_stdio(mcp_index_path):
    print("\n--- MCP Stdio Tests ---")
    try:
        proc = subprocess.Popen(
            ["node", mcp_index_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            env={**__import__("os").environ, "WALKEY_URL": BASE},
        )

        def send_rpc(method, params=None, req_id=1):
            msg = {"jsonrpc": "2.0", "id": req_id, "method": method}
            if params is not None:
                msg["params"] = params
            line = json.dumps(msg) + "\n"
            proc.stdin.write(line.encode())
            proc.stdin.flush()
            resp_line = proc.stdout.readline().decode().strip()
            if not resp_line:
                return None
            return json.loads(resp_line)

        # Initialize
        resp = send_rpc("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "test", "version": "1.0.0"}
        }, req_id=1)
        check("initialize returns result", resp is not None and "result" in resp,
              json.dumps(resp)[:200] if resp else "no response")

        # Send initialized notification
        notif = {"jsonrpc": "2.0", "method": "notifications/initialized"}
        proc.stdin.write((json.dumps(notif) + "\n").encode())
        proc.stdin.flush()

        # List tools
        resp = send_rpc("tools/list", {}, req_id=2)
        if resp and "result" in resp:
            tools = resp["result"].get("tools", [])
            tool_names = [t["name"] for t in tools]
            check("tools/list returns tools", len(tools) > 0, f"got {len(tools)}")
            check("has walkey_ping", "walkey_ping" in tool_names)
            check("has walkey_list_modes", "walkey_list_modes" in tool_names)
            check("has walkey_set_binding", "walkey_set_binding" in tool_names)
            check("23 tools total", len(tools) == 23, f"got {len(tools)}")
        else:
            check("tools/list", False, str(resp))

        # Call walkey_ping
        resp = send_rpc("tools/call", {
            "name": "walkey_ping",
            "arguments": {}
        }, req_id=3)
        if resp and "result" in resp:
            content = resp["result"].get("content", [])
            check("walkey_ping returns content", len(content) > 0)
            text = content[0].get("text", "") if content else ""
            check("walkey_ping says ok", "ok" in text.lower())
        else:
            check("walkey_ping call", False, str(resp))

        # Call walkey_list_modes
        resp = send_rpc("tools/call", {
            "name": "walkey_list_modes",
            "arguments": {}
        }, req_id=4)
        if resp and "result" in resp:
            content = resp["result"].get("content", [])
            check("walkey_list_modes returns content", len(content) > 0)
            if content:
                modes = json.loads(content[0].get("text", "[]"))
                check("list_modes returns array", isinstance(modes, list))
                check("list_modes has modes", len(modes) >= 1)
        else:
            check("walkey_list_modes call", False, str(resp))

        proc.stdin.close()
        proc.wait(timeout=5)

    except Exception as e:
        check("mcp stdio", False, str(e))


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    global BASE
    parser = argparse.ArgumentParser(description="MCP integration tests")
    parser.add_argument("--mcp", help="Path to mcp/index.js for stdio tests")
    parser.add_argument("--base", default=BASE, help="Device base URL")
    args = parser.parse_args()

    BASE = args.base

    print(f"Testing against {BASE}")
    print("=" * 50)

    # Firmware API tests
    test_ping()
    test_get_modes()
    test_get_single_mode()
    test_get_wifi()
    test_get_defaults()
    test_get_boot_mode()
    test_get_global_bindings()
    test_put_defaults_merge()
    test_put_active_mode()

    # MCP stdio tests
    if args.mcp:
        test_mcp_stdio(args.mcp)

    print("\n" + "=" * 50)
    print(f"Results: {passed} passed, {failed} failed")
    sys.exit(1 if failed > 0 else 0)


if __name__ == "__main__":
    main()
