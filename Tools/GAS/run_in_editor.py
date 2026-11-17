"""Runs Tools/GAS/<module>.run() inside the open editor through the MCP bridge; prints the output (max 40 lines).
  python -I Tools/GAS/run_in_editor.py setup_presentation_vfx_materials
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ue_bridge_client import Bridge  # noqa: E402

TOOLS = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
module = sys.argv[1]
if module.endswith(".py"):  # a file elsewhere (a scratch probe): import it from its own folder
    TOOLS = os.path.dirname(os.path.abspath(module)).replace("\\", "/")
    module = os.path.splitext(os.path.basename(module))[0]
func = sys.argv[2] if len(sys.argv) > 2 else "run"
code = (f"import importlib, sys\nsys.path.insert(0, r'{TOOLS}')\nimport {module} as _m\nimportlib.reload(_m)\n"
        f"print('RESULT', _m.{func}())\n")
result = Bridge().call("execute_python", {"code": code})
out = (result or {}).get("output", "") if isinstance(result, dict) else str(result)
lines = [l for l in out.splitlines() if l.strip()]
print("\n".join(lines[-40:]))
if isinstance(result, dict) and (result.get("logErrorCount") or not result.get("success", True)):
    print("ERRORS:", json.dumps([e for e in result.get("log_output", []) if e.get("type") != "Info"])[:1500])
