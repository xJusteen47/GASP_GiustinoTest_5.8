"""Where the minutes of an animpipe round go (import-veloce-1, 2026-11-24).

    python import_timing_report.py <timing.jsonl> [--ue-log=<Run-Timed log>] [--out=<folder>]

Reads the round runner's timing file (steps, editor boot, every bridge call with its cache result) and, when given, the session's
UE log (<log>.ue.log next to the Run-Timed log): editor startup, shader compilation, DDC, PoseSearch indexing, FBX import, the
session's jobs. Each UE figure is a span between the first and last line of that kind, from the log's own timestamps, so it is a
measurement, not an estimate. Writes <out>/<name>_report.{md,json} (default: next to the jsonl) and prints at most 5 lines.
The verify lines (ANIMPIPE_SAMPLE_CACHE_VERIFY=1) are summarised as the largest cache-vs-editor difference: it must be 0.
"""
import collections, datetime, json, os, re, sys

TS = re.compile(r"^\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d):(\d{3})\]")
KINDS = {   # name: (line pattern, what it measures)
    "shaders": (re.compile(r"LogShaderCompilers|ShaderCompileWorker|shaders? left to compile", re.I), "shader compilation"),
    "ddc": (re.compile(r"LogDerivedDataCache|LogZen", re.I), "derived data cache"),
    "posesearch": (re.compile(r"LogPoseSearch|PoseSearchDatabase", re.I), "PoseSearch indexing"),
    "fbx": (re.compile(r"LogFbx|FbxImport|Interchange", re.I), "FBX / Interchange import"),
    "pie": (re.compile(r"PIE: |LogPlayLevel", re.I), "Play in editor"),
}
STARTUP = re.compile(r"Total Editor Startup Time, took=([\d.]+)")
READY = re.compile(r"\[animpipe_session\] ready")
JOB = re.compile(r"\[animpipe_session\] job (\S+) (ok|FAILED) in ([\d.]+) s")
SHADER_N = re.compile(r"Compiling (\d+) shaders?|(\d+) Shaders? Left", re.I)


def _ts(line):
    m = TS.match(line)
    if not m:
        return None
    y, mo, d, h, mi, s, ms = (int(x) for x in m.groups())
    return datetime.datetime(y, mo, d, h, mi, s, ms * 1000).timestamp()


def ue_log(path):
    out = {"log": path, "startup_s": None, "ready_s": None, "spans": {}, "jobs": [], "shader_max_pending": 0, "ddc_summary": []}
    if not path or not os.path.exists(path):
        out["missing"] = True
        return out
    first = None
    kinds = {k: [None, None, 0] for k in KINDS}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            t = _ts(line)
            if t is not None and first is None:
                first = t
            m = STARTUP.search(line)
            if m:
                out["startup_s"] = float(m.group(1))
            if READY.search(line) and t is not None and first is not None and out["ready_s"] is None:
                out["ready_s"] = round(t - first, 1)
            m = JOB.search(line)
            if m:
                out["jobs"].append({"id": m.group(1), "ok": m.group(2) == "ok", "sec": float(m.group(3))})
            m = SHADER_N.search(line)
            if m:
                out["shader_max_pending"] = max(out["shader_max_pending"], int(m.group(1) or m.group(2)))
            if "LogDerivedDataCache" in line and re.search(r"hit|miss|%", line, re.I) and len(out["ddc_summary"]) < 6:
                out["ddc_summary"].append(line.strip()[:200])
            for k, (pat, _) in KINDS.items():
                if pat.search(line):
                    s = kinds[k]
                    s[2] += 1
                    if t is not None:
                        s[0] = t if s[0] is None else s[0]
                        s[1] = t
    for k, (a, b, n) in kinds.items():
        out["spans"][k] = {"lines": n, "span_s": round(b - a, 1) if a is not None else 0.0, "what": KINDS[k][1],
                           "from_boot_s": round(a - first, 1) if a is not None and first is not None else None}
    return out


def timing(path):
    steps, calls, verify, boot, rnd = [], collections.defaultdict(lambda: {"n": 0, "sec": 0.0}), [], None, {}
    for line in open(path, encoding="utf-8"):
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if "method" in r:
            if r.get("cache") == "verify":
                verify.append(r)
                continue
            c = calls[(r["method"], r.get("cache", "off"))]
            c["n"] += 1
            c["sec"] += float(r.get("sec") or 0)
        elif r.get("phase") == "round":
            rnd.update(r)
        elif r.get("phase") == "editor_boot":
            boot = r
        elif r.get("phase") == "editor_session":
            boot = boot or r
        elif "phase" in r:
            steps.append(r)
    return steps, calls, verify, boot, rnd


def main(argv):
    if not argv:
        print(__doc__.splitlines()[2]); return 2
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in argv[1:] if a.startswith("--"))
    jl = argv[0]
    steps, calls, verify, boot, rnd = timing(jl)
    ue = None
    if opt.get("ue-log"):
        p = opt["ue-log"]
        root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
        p = p if os.path.isabs(p) else os.path.join(root, p)
        ue = ue_log(p + ".ue.log" if os.path.exists(p + ".ue.log") else p)
    name = os.path.basename(jl).rsplit(".", 1)[0]
    out_dir = opt.get("out") or os.path.dirname(os.path.abspath(jl))
    bridge_s = sum(c["sec"] for c in calls.values())
    hits = sum(c["n"] for (m, k), c in calls.items() if k == "hit")
    miss = sum(c["n"] for (m, k), c in calls.items() if k in ("miss", "bypass", "off"))
    worst = max([float(v.get("max_diff", 0) or 0) for v in verify] + [0.0])
    same_info = all(v.get("same", True) for v in verify)
    md = [f"# Tempi del giro {rnd.get('name', name)}", "",
          f"Totale giro: {rnd.get('sec', '?')} s, editor: {rnd.get('session', '?')}, uscita {rnd.get('exit', '?')}.", "",
          "| Passo | Secondi | Esito |", "|---|---|---|"]
    if boot:
        md.append(f"| avvio editor ({boot.get('event', 'boot')}) | {boot.get('sec', '-')} | {'ok' if boot.get('ok', True) else 'FAIL'} |")
    for s in steps:
        md.append(f"| {s['phase']} | {'saltato' if s.get('skipped') else s.get('sec')} | {'' if s.get('skipped') else 'rc ' + str(s.get('exit'))} |")
    md += ["", f"Chiamate al bridge: {sum(c['n'] for c in calls.values())} in {bridge_s:.1f} s (dalla cache {hits}, all'editor {miss}).",
           "", "| Metodo | Cache | N | Secondi |", "|---|---|---|---|"]
    for (m, k), c in sorted(calls.items(), key=lambda kv: -kv[1]["sec"]):
        md.append(f"| {m} | {k} | {c['n']} | {c['sec']:.1f} |")
    if verify:
        md += ["", f"Verifica cache: {len(verify)} confronti, differenza massima {worst} (deve essere 0), info uguali: {same_info}."]
    if ue:
        md += ["", f"## Log UE ({ue['log']})", ""]
        if ue.get("missing"):
            md.append("log non trovato")
        else:
            md.append(f"Avvio editor (riga 'Total Editor Startup Time'): {ue['startup_s']} s; sessione pronta a {ue['ready_s']} s dal primo rigo.")
            md.append(f"Shader in coda (massimo letto): {ue['shader_max_pending']}.")
            md += ["", "| Tipo | Righe | Durata (s, dal primo all'ultimo rigo) | Inizio (s dal boot) |", "|---|---|---|---|"]
            for k, s in ue["spans"].items():
                md.append(f"| {s['what']} | {s['lines']} | {s['span_s']} | {s['from_boot_s']} |")
            if ue["jobs"]:
                md += ["", "Lavori della sessione: " + ", ".join(f"{j['id']} {j['sec']} s{'' if j['ok'] else ' FAIL'}" for j in ue["jobs"])]
            if ue["ddc_summary"]:
                md += ["", "DDC:"] + [f"- `{l}`" for l in ue["ddc_summary"]]
    os.makedirs(out_dir, exist_ok=True)
    mdp = os.path.join(out_dir, f"{name}_report.md")
    open(mdp, "w", encoding="utf-8").write("\n".join(md) + "\n")
    json.dump({"round": rnd, "boot": boot, "steps": steps, "calls": {f"{m}|{k}": c for (m, k), c in calls.items()},
               "verify_max_diff": worst, "verify_info_same": same_info, "ue": ue},
              open(os.path.join(out_dir, f"{name}_report.json"), "w"), indent=1)
    top = sorted((s for s in steps if not s.get("skipped")), key=lambda s: -float(s.get("sec") or 0))[:3]
    print(f"round {rnd.get('name', name)}: {rnd.get('sec', '?')} s, editor {rnd.get('session', '?')}"
          + (f", boot {boot.get('sec')} s" if boot and boot.get("sec") is not None else ""))
    print("slowest: " + ", ".join(f"{s['phase']} {s.get('sec')} s" for s in top))
    print(f"bridge: {sum(c['n'] for c in calls.values())} calls {bridge_s:.0f} s, cache hits {hits}"
          + (f", VERIFY max diff {worst}" if verify else ""))
    if ue and not ue.get("missing"):
        sp = ue["spans"]
        print(f"UE: startup {ue['startup_s']} s, shaders {sp['shaders']['span_s']} s, ddc {sp['ddc']['span_s']} s, "
              f"posesearch {sp['posesearch']['span_s']} s, fbx {sp['fbx']['span_s']} s")
    print(mdp)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
