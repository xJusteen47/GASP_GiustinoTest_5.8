"""Minimal client for the Unreal Editor's built-in MCP server (UE 5.9+, default http://127.0.0.1:8000/mcp).

Two calls: available() and execute(script). Everything goes through the server's execute_python tool; the script's last
expression must be a json string. Address: env UE_MCP_URL, else engine.mcp_url in kit.config.json.
The server belongs to whichever editor took the port first: one editor per machine.
"""
import json, os, time, urllib.request

import kit_config  # noqa: E402
URL = os.environ.get("UE_MCP_URL") or kit_config.get("engine.mcp_url", "http://127.0.0.1:8000/mcp")
PY_TOOLSET = "editor_toolset.toolsets.python_scripting.PythonScriptingTools"
_sid = None


def _rpc(method, params=None, notify=False, timeout=300):
    global _sid
    body = {"jsonrpc": "2.0", "method": method}
    if not notify:
        body["id"] = 1
    if params is not None:
        body["params"] = params
    h = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
    if _sid:
        h["Mcp-Session-Id"] = _sid
    r = urllib.request.urlopen(urllib.request.Request(URL, data=json.dumps(body).encode(), headers=h), timeout=timeout)
    txt = r.read().decode()
    _sid = _sid or r.headers.get("Mcp-Session-Id")
    if "text/event-stream" in (r.headers.get("Content-Type") or ""):     # a streamed answer: the last data line is the result
        data = [ln[5:].strip() for ln in txt.splitlines() if ln.startswith("data:")]
        txt = data[-1] if data else ""
    return json.loads(txt) if txt.strip().startswith("{") else {}


def available(timeout=2):
    """True when an editor answers on the built-in MCP address (opens the session as a side effect)."""
    global _sid
    if _sid:
        return True
    try:
        _rpc("initialize", {"protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "harness-kit", "version": "1"}}, timeout=timeout)
        _rpc("notifications/initialized", notify=True, timeout=timeout)
        return bool(_sid)
    except Exception:
        _sid = None
        return False


def execute(script, timeout=300):
    """Run a script in the editor; its final expression must be a json string, returned parsed."""
    if not available():
        raise ConnectionError(f"no editor on {URL}")
    for attempt in range(40):
        r = _rpc("tools/call", {"name": "call_tool", "arguments": {"toolset_name": PY_TOOLSET, "tool_name": "execute_python",
                                                                  "arguments": {"script": {"content": script, "language": "Python"}}}}, timeout=timeout)
        if "error" in r:
            raise RuntimeError(r["error"])
        res = r.get("result", {})
        txt = "".join(c.get("text", "") for c in res.get("content", []))
        # right after a save or an import the editor refuses tools ("Not run: ... slow task ... made no changes"): safe to wait
        if res.get("isError") and txt.startswith("Not run:") and "made no changes" in txt:
            time.sleep(3)
            continue
        break
    if res.get("isError"):
        raise RuntimeError(txt[-1500:])
    v = json.loads(txt)
    v = v.get("returnValue", v) if isinstance(v, dict) else v
    return json.loads(v) if isinstance(v, str) else v
