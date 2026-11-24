"""Default pre-run gate of Run-Timed.ps1 (cleanup plan 2026-11-24, block 1, fixes 5, 6 and 8).

Why it exists: an editor run is minutes to an hour. Three classes of failure were found only AFTER the editor was up:
a driver with a syntax error (one night: an hour for nothing), an asset that is not in the project the command runs from
(the T3 dair dump was static because the lifecal worktree has no skeleton), and a local LLM holding the GPU (slow capture
or TIMEOUT). All three are cheap to see before the launch, so Run-Timed refuses first (ESITO: PRECHECK_FAIL, exit 4).

Checks (each prints one line starting with OK / FAIL / SKIP; exit 0 only when no FAIL):
  syntax   every .py passed to the command compiles. Compiled in memory: no __pycache__ is written next to the drivers.
  asset    every /Game/... asset the command asks for exists under <project>/Content. Sources: --asset, `# REQUIRES_ASSET:
           /Game/...` header lines of the drivers, and /Game/... tokens in the command line.
  vram     --vram: refuse when a local LLM server (ollama, llama.cpp, LM Studio...) is listed by nvidia-smi as a GPU
           process. Without nvidia-smi the check is SKIP (it never blocks a machine that has no NVIDIA tooling).

Usage: python precheck.py --project <dir with the .uproject> [--py a.py ...] [--asset /Game/X ...] [--cmdline "<text>"] [--vram]
"""
import argparse
import os
import re
import subprocess
import sys

LLM_PROCESS = re.compile(r"ollama|llama[-_.]?(server|cpp|cli)?|lm[ -_]?studio|koboldcpp|vllm|text-generation|tabbyapi|ggml", re.I)
GAME_TOKEN = re.compile(r"/Game/[A-Za-z0-9_/\-]+(?:\.[A-Za-z0-9_]+)?")
REQUIRES = re.compile(r"^\s*#\s*REQUIRES_ASSET:\s*(/Game/\S+)", re.M)
PY_TOKEN = re.compile(r"""[^\s"'=]+\.py\b""")


def check_syntax(py_files):
    out, bad = [], 0
    for p in py_files:
        try:
            with open(p, "rb") as f:
                src = f.read()
            compile(src, p, "exec")
            out.append("OK syntax: %s" % os.path.basename(p))
        except FileNotFoundError:
            bad += 1
            out.append("FAIL syntax: script not found: %s" % p)
        except SyntaxError as e:
            bad += 1
            out.append("FAIL syntax: %s line %s: %s" % (p, e.lineno, e.msg))
    return out, bad


def asset_file(project, game_path):
    """/Game/A/B.B or /Game/A/B -> <project>/Content/A/B.uasset (or .umap)."""
    rel = game_path[len("/Game/"):].split(".")[0]
    base = os.path.join(project, "Content", *rel.split("/"))
    for ext in (".uasset", ".umap"):
        if os.path.isfile(base + ext):
            return base + ext
    return None


def check_assets(project, assets):
    out, bad = [], 0
    for a in sorted(set(assets)):
        if asset_file(project, a):
            out.append("OK asset: %s" % a)
        else:
            bad += 1
            out.append("FAIL asset: %s is not under %s (wrong worktree? run the command from the project that has it)"
                       % (a, os.path.join(project, "Content")))
    return out, bad


def gpu_processes():
    """Names of GPU compute processes, or None when nvidia-smi is not available."""
    try:
        r = subprocess.run(["nvidia-smi", "--query-compute-apps=pid,process_name,used_memory",
                            "--format=csv,noheader"], capture_output=True, text=True, timeout=15)
    except (OSError, subprocess.SubprocessError):
        return None
    if r.returncode != 0:
        return None
    return [l.strip() for l in r.stdout.splitlines() if l.strip()]


def check_vram(apps=None):
    apps = gpu_processes() if apps is None else apps
    if apps is None:
        return ["SKIP vram: nvidia-smi not available"], 0
    hits = [l for l in apps if LLM_PROCESS.search(l)]
    if hits:
        return ["FAIL vram: local LLM holds the GPU (%s). Stop it (e.g. `ollama stop`) or pass -NoPreCheck on purpose"
                % hits[0]], 1
    return ["OK vram: no local LLM among %d GPU process(es)" % len(apps)], 0


def collect_py(cmdline, py_files):
    found = list(py_files or [])
    for tok in PY_TOKEN.findall(cmdline or ""):
        found.append(tok)
    seen, res = set(), []
    for p in found:
        p = p.replace("/", os.sep) if os.sep == "\\" else p
        if p not in seen:
            seen.add(p)
            res.append(p)
    return res


def collect_assets(cmdline, explicit, py_files):
    assets = list(explicit or []) + GAME_TOKEN.findall(cmdline or "")
    for p in py_files:
        try:
            with open(p, "r", encoding="utf-8", errors="replace") as f:
                assets += REQUIRES.findall(f.read(65536))
        except OSError:
            pass
    return assets


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--project", required=True, help="directory of the .uproject the command will open")
    ap.add_argument("--py", action="append", default=[])
    ap.add_argument("--asset", action="append", default=[])
    ap.add_argument("--cmdline", default="")
    ap.add_argument("--vram", action="store_true")
    a = ap.parse_args(argv)
    lines, bad = [], 0
    py = collect_py(a.cmdline, a.py)
    for fn, args in ((check_syntax, (py,)), (check_assets, (a.project, collect_assets(a.cmdline, a.asset, py)))):
        o, b = fn(*args)
        lines += o
        bad += b
    if a.vram:
        o, b = check_vram()
        lines += o
        bad += b
    # failures first: Run-Timed shows only the first lines of the output
    lines.sort(key=lambda l: 0 if l.startswith("FAIL") else 1)
    print("PRECHECK: %s" % ("FAIL (%d)" % bad if bad else "OK"))
    for l in lines:
        print(l)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
