#!/usr/bin/env python3
"""Canary build for a GSTN plugin (modularity-and-tags.md 3.7, rule 7).

Generates a blank code-less project that enables only <PluginName> and the plugins its .uplugin lists
(project plugins under Plugins/ are copied, engine plugins are only enabled), builds the UnrealEditor
target and runs `Automation RunTests <PluginName>` headless. Reads the repo, writes only to --out.

usage: python Tools/Canary/make_canary.py <PluginName> [--out <dir>] [--no-build] [--no-tests]
"""
import argparse, json, os, re, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGINE = Path(os.environ.get("UE_ENGINE_DIR", r"C:\Program Files\Epic Games\UE_5.8\Engine"))
SKIP_DIRS = {"Binaries", "Intermediate", "Saved"}


def fail(msg):
    print("CANARY ERROR: " + msg)
    sys.exit(2)


def index_uplugins(base):
    found = {}
    for dirpath, dirnames, filenames in os.walk(base):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for f in filenames:
            if f.endswith(".uplugin"):
                found.setdefault(f[:-8], Path(dirpath) / f)
    return found


def load(p):
    # .uplugin files may carry a BOM
    return json.loads(p.read_text(encoding="utf-8-sig"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("plugin")
    ap.add_argument("--out")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--no-tests", action="store_true")
    a = ap.parse_args()

    out = Path(a.out).resolve() if a.out else ROOT / "Saved" / "Canary" / a.plugin
    for forbidden in (ROOT / "Plugins", ROOT / "Content"):
        if out == forbidden or forbidden in out.parents:
            fail(f"--out must not be inside {forbidden}")

    project_plugins = index_uplugins(ROOT / "Plugins")
    if a.plugin not in project_plugins:
        fail(f"{a.plugin}.uplugin not found under Plugins/")
    engine_plugins = None  # lazy, the engine tree is big

    project_deps, engine_deps = [], []
    seen = set()

    def visit(name, via):
        nonlocal engine_plugins
        if name in seen:
            return
        seen.add(name)
        if name in project_plugins:
            project_deps.append(name)
            for d in load(project_plugins[name]).get("Plugins", []):
                if d.get("Enabled", True):
                    visit(d["Name"], name)
            return
        if engine_plugins is None:
            engine_plugins = index_uplugins(ENGINE / "Plugins")
        if name in engine_plugins:
            engine_deps.append(name)
            return
        fail(f"dependency '{name}' (required by {via}) is neither a project plugin under Plugins/ nor an engine plugin")

    visit(a.plugin, "<root>")

    if out.exists():
        # only ever clean our own canary folder
        shutil.rmtree(out)
    out.mkdir(parents=True)
    for n in project_deps:
        src = project_plugins[n].parent
        shutil.copytree(src, out / "Plugins" / n, ignore=shutil.ignore_patterns(*SKIP_DIRS))
    uproject = out / "Canary.uproject"
    enabled = [{"Name": n, "Enabled": True} for n in project_deps + engine_deps]
    uproject.write_text(json.dumps({"FileVersion": 3, "EngineAssociation": "5.8", "Category": "",
                                    "Description": "GSTN canary", "Plugins": enabled}, indent="\t"), encoding="utf-8")

    summary = {"plugin": a.plugin,
               "deps": "project: " + (", ".join(n for n in project_deps if n != a.plugin) or "-")
                       + " | engine: " + (", ".join(engine_deps) or "-")}
    res_build = "skipped"
    if not a.no_build:
        with open(out / "build.out", "w", encoding="utf-8", errors="replace") as f:
            subprocess.run([str(ENGINE / "Build" / "BatchFiles" / "Build.bat"), "UnrealEditor", "Win64", "Development",
                            f"-Project={uproject}", "-WaitMutex"], stdout=f, stderr=subprocess.STDOUT)
        text = (out / "build.out").read_text(encoding="utf-8", errors="replace")
        m = re.findall(r"Result: (Succeeded|Failed)", text)
        errs = [l.strip()[:220] for l in text.splitlines() if ": error " in l]
        res_build = (m[-1] if m else "no result") + f" errors={len(errs)}"
        for l in errs[:2]:
            res_build += "\n  " + l
    res_tests = "skipped"
    if not a.no_tests and not res_build.startswith("Failed") and not res_build.startswith("no result"):
        log = out / "tests.log"
        with open(out / "editor.out", "w", encoding="utf-8", errors="replace") as f:
            subprocess.run([str(ENGINE / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"), str(uproject),
                            f"-ExecCmds=Automation RunTests {a.plugin};Quit", "-unattended", "-nullrhi", "-nopause",
                            f"-abslog={log}"], stdout=f, stderr=subprocess.STDOUT)
        t = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        ok = len(re.findall(r"Test Completed\. Result=\{Success\}", t))
        bad = len(re.findall(r"Test Completed\. Result=\{Fail", t))
        ens = t.count("Ensure condition failed")
        res_tests = f"success={ok} fail={bad} ensure={ens}" + ("" if t else " (no log)")
    lines = [f"plugin: {a.plugin}", f"deps: {summary['deps']}", f"build: {res_build}", f"tests: {res_tests}", f"out: {out}"]
    (out / "summary.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
