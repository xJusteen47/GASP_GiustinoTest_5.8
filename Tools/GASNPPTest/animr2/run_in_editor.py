"""python Saved/AnimR2/run_in_editor.py <script.py> [log] : runs a python file inside the running editor (built-in MCP, port 8000); prints 3 lines max"""
import sys, io, json, os
sys.path.insert(0, 'C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59/Tools/AnimPipeline')
import ue_builtin
p = os.path.abspath(sys.argv[1]).replace(chr(92), '/')
log = sys.argv[2] if len(sys.argv) > 2 else p + '.log'
code = ("import io, contextlib, json, runpy, traceback\n_b = io.StringIO()\n_ok = True\n"
        "try:\n    with contextlib.redirect_stdout(_b):\n        runpy.run_path(%r, run_name='__main__')\n"
        "except Exception:\n    _ok = False\n    _b.write(traceback.format_exc())\n"
        "json.dumps({'ok': _ok, 'out': _b.getvalue()[-6000:]})" % p)
r = ue_builtin.execute(code, timeout=600)
open(log, 'w').write(r.get('out', ''))
print('ok' if r.get('ok') else 'ERROR', '->', log)
print(r.get('out', '').strip().splitlines()[-1][:200] if r.get('out', '').strip() else '')
