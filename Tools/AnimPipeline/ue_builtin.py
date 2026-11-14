"""Backend of ue.py for the editor's built-in MCP server (Unreal 5.9, http://127.0.0.1:8000/mcp) instead of UE_MCP_Bridge.

The built-in server exposes no per-method handlers like the bridge did: everything goes through its `execute_python` tool.
The four bridge methods the pipeline uses (run_python_file, sample_pose, read_anim_sequence, import_animation) are
rebuilt here as small editor scripts that return the SAME result shapes, so no caller changes.
`UE_MCP_URL` overrides the address. The server belongs to whichever editor took port 8000 first: one editor per machine.
"""
import json, os, time, urllib.request

URL = os.environ.get("UE_MCP_URL", "http://127.0.0.1:8000/mcp")
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
        _rpc("initialize", {"protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "animpipe", "version": "1"}}, timeout=timeout)
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


_RUN_FILE = '''
import importlib.util, json, traceback, asyncio
_k = json.loads(%(kwargs)r)
try:
    _s = importlib.util.spec_from_file_location("_animpipe_remote", %(path)r)
    _m = importlib.util.module_from_spec(_s)
    _s.loader.exec_module(_m)
    _r = getattr(_m, %(entry)r)(**_k) if %(entry)r else None
    if asyncio.iscoroutine(_r):
        _r = await _r
    _o = {"result": _r if isinstance(_r, str) else str(_r), "logErrorCount": 0, "log_output": ""}
except Exception:
    _o = {"result": traceback.format_exc(), "logErrorCount": 1, "log_output": traceback.format_exc()}
json.dumps(_o)
'''

_SAMPLE = '''
import json
_a = unreal.load_asset(%(asset)r)
_space = unreal.AnimPoseSpaces.LOCAL if %(space)r == "local" else unreal.AnimPoseSpaces.WORLD
_opt = unreal.AnimPoseEvaluationOptions()
_out = []
for _f in %(frames)r:
    _p = unreal.AnimPoseExtensions.get_anim_pose_at_frame(_a, _f, _opt)
    _bones = []
    for _b in %(bones)r:
        _t = unreal.AnimPoseExtensions.get_bone_pose(_p, _b, _space)
        _r = _t.rotation.rotator()
        _bones.append({"name": _b, "location": {"x": _t.translation.x, "y": _t.translation.y, "z": _t.translation.z},
                       "rotation": {"pitch": _r.pitch, "yaw": _r.yaw, "roll": _r.roll},
                       "quat": {"x": _t.rotation.x, "y": _t.rotation.y, "z": _t.rotation.z, "w": _t.rotation.w}})
    _out.append({"frame": _f, "bones": _bones})
json.dumps({"samples": _out} if _a else {"error": "asset not found"})
'''

_READ = '''
import json
_a = unreal.load_asset(%(asset)r)
_n = unreal.AnimationLibrary.get_num_frames(_a) if _a else 0
_len = _a.get_play_length() if _a else 0.0
# the bridge's "numFrames" was the number of sampled keys (frames + 1): callers do range(numFrames), keep that
json.dumps({"numFrames": unreal.AnimationLibrary.get_num_keys(_a), "numIntervals": _n,
            "frameRate": (round(_n / _len, 4) if _len > 0 else 0.0), "sequenceLength": _len,
            "skeleton": _a.get_skeleton().get_path_name()} if _a else {"error": "asset not found"})
'''


def call(method, params=None, timeout=300):
    p = params or {}
    if method == "run_python_file":
        return execute(_RUN_FILE % {"path": p["filePath"], "entry": p.get("entryPoint") or "", "kwargs": json.dumps(p.get("kwargs") or {})}, timeout)
    if method == "sample_pose":
        return execute(_SAMPLE % {"asset": p["assetPath"], "bones": list(p["boneNames"]), "frames": list(p["frames"]),
                                  "space": p.get("space", "component")}, timeout)
    if method == "read_anim_sequence":
        return execute(_READ % {"asset": p["assetPath"]}, timeout)
    raise NotImplementedError(f"ue_builtin: bridge method '{method}' has no built-in MCP version yet (set UE_MCP_BACKEND=bridge to use UE_MCP_Bridge)")
