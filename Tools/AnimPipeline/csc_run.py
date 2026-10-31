"""Run a Python file inside Cascadeur through the script server (POST http://127.0.0.1:8765/run).
Usage: python -I csc_run.py <script.py> [NAME=value ...]
NAME=value pairs are prepended as Python assignments (strings unless they parse as Python literals).
Prints the Cascadeur messages; exits 1 if the run failed or logged an error."""
import ast, json, os, sys, urllib.request

path = sys.argv[1]
# The script server exec()s the code with no __file__ and Cascadeur's cwd, so scripts that locate their data folder
# from __file__ would read from Cascadeur's install folder; give them the real path.
pre = [f"__file__ = {os.path.abspath(path).replace(chr(92), '/')!r}"]
for kv in sys.argv[2:]:
    k, v = kv.split("=", 1)
    try:
        ast.literal_eval(v)
        pre.append(f"{k} = {v}")
    except Exception:
        pre.append(f"{k} = {v!r}")
code = "\n".join(pre + [open(path, encoding="utf-8").read()])
req = urllib.request.Request("http://127.0.0.1:8765/run", data=json.dumps({"code": code}).encode(),
                             headers={"Content-Type": "application/json"})
r = json.loads(urllib.request.urlopen(req, timeout=600).read())
bad = not r.get("ok")
for m in r.get("messages", []):
    print(f"[{m.get('level')}] {m.get('text')}")
    bad |= m.get("level") == "Error"
if r.get("error"):
    print("ERROR:", r["error"])
sys.exit(1 if bad else 0)
