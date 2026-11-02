"""animpipe: one entry point, versions are data (move card JSON + Level Sequence), not scripts.

    python -m animpipe roundtrip <AnimAsset> <name> [--fps=60|62.5] [--keys=card.json|0,3,9]   rig bake round trip
    python -m animpipe tangent <name>                                                             tangent probe on LS_<name>
    python -m animpipe card <card.json>                                                           validate a move card
    python -m animpipe author <card.json> [--no-write]  card "author" pose data -> source take (no clip yet)
    python -m animpipe build <card.json>        card -> sparse keys + tangents on the rig -> AnimSequence at the card fps
    python -m animpipe trails <card.json>       motion trails, one composite image (3 views, build over source)
    python -m animpipe poses <card.json> [--frames=] [--take=] [--over=]   pose checkpoint grid (one image)
    python -m animpipe check <card.json>        the three check classes on the card's built anim (animpipe/checks.py)
    python -m animpipe review <card.json> [--hitstop=8] [--at=<active frame>] [--force]   build + check + in-game render, gated:
                                                no render while a blocking check fails (--force renders anyway)

Every command writes its full result to Saved/AnimPipeline/animpipe/<name>_<cmd>.json and prints at most 5 lines."""
import json, os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, TOOLS)
OUT = os.path.join(os.path.expanduser("~"), "Documents", "Unreal Projects", "GASP_Giustino2", "Saved", "AnimPipeline", "animpipe")
BONES = ["pelvis", "spine_05", "head", "hand_l", "hand_r", "lowerarm_l", "lowerarm_r", "foot_l", "foot_r", "calf_l", "calf_r", "middle_01_l"]
RIG = "/Game/Characters/UE5_Mannequins/Rigs/CR_Mannequin_Body"


def _ue_py(entry, **kw):
    import ue
    r = ue.call("run_python_file", {"filePath": os.path.join(HERE, "ue_rig.py").replace("\\", "/"), "entryPoint": entry, "kwargs": kw}, timeout=600)
    v = r.get("result")
    if not isinstance(v, str) or not v.startswith("{"):
        raise RuntimeError(f"{entry}: {str(v)[-700:]} | log: {str(r.get('log_output'))[-600:]}")
    return json.loads(v)


def _sample(asset, frames):
    import ue
    out = {}
    for s0 in range(0, len(frames), 8):
        for _ in range(20):     # a source-control dialog after a save blocks the bridge for a few seconds
            r = ue.call("sample_pose", {"assetPath": asset, "boneNames": BONES, "frames": frames[s0:s0 + 8], "space": "component"})
            if not r.get("dialogBlocking"):
                break
            time.sleep(3)
        if "samples" not in r:
            raise RuntimeError(f"sample_pose {asset} {frames[s0:s0 + 8]}: {str(r)[:300]}")
        for s in r["samples"]:
            out[s["frame"]] = {b["name"]: [b["location"][k] for k in "xyz"] for b in s["bones"]}
    return out


def _keys(arg):
    if not arg:
        return None
    if arg.endswith(".json"):
        from animpipe.card import load, key_frames
        return key_frames(load(arg))
    return [int(x) for x in arg.split(",")]


def roundtrip(anim, name, opt):
    fps = opt.get("fps", "60")
    num, den = (125, 2) if fps == "62.5" else (int(fps), 1)
    keys = _keys(opt.get("keys"))
    os.makedirs(OUT, exist_ok=True)
    r = _ue_py("roundtrip", anim=anim, rig=opt.get("rig", RIG), name=name, fps_num=num, fps_den=den, keep_frames=keys,
               out=os.path.join(OUT, f"{name}_ue.json").replace("\\", "/"))
    rate = num / den
    if r.get("export_ok"):
        # compare at the times both clips have a frame (every frame at 60; every 24th/25th at 62.5)
        step_src, step_dst = (24, 25) if den == 2 else (1, 1)
        n_src = int(round(r["length"] * 60))
        pairs = [(i * step_src, i * step_dst) for i in range(n_src // step_src + 1)]
        A = _sample(anim, [a for a, _ in pairs]); B = _sample(r["out_asset"], [b for _, b in pairs])
        worst, per = (0.0, None), {}
        for a, b in pairs:
            for bn in BONES:
                d = sum((x - y) ** 2 for x, y in zip(A[a][bn], B[b][bn])) ** 0.5
                per[bn] = max(per.get(bn, 0.0), d)
                if d > worst[0]:
                    worst = (d, (a, bn))
        r.update({"worst_cm": round(worst[0], 3), "worst_at": worst[1], "per_bone_cm": {k: round(v, 3) for k, v in per.items()},
                  "compared_frames": len(pairs), "keys": keys})
    path = os.path.join(OUT, f"{name}_roundtrip.json")
    json.dump(r, open(path, "w"), indent=1)
    gate = 0.5 if not keys else None
    verdict = "n/a" if "worst_cm" not in r else ("PASS" if gate and r["worst_cm"] <= gate else ("FAIL" if gate else "MEASURE"))
    print(f"roundtrip {name} @{rate:g}fps: bake {r.get('bake_ok')} export {r.get('export_ok')} keys {r.get('keys_before')}->{r.get('keys_after')}")
    print(f"length {r.get('length')} -> {r.get('out_length')} frames {r.get('out_frames')}; worst {r.get('worst_cm')} cm at {r.get('worst_at')} [{verdict}]")
    print(path)


def build(path):
    from animpipe.card import load, key_frames, segments
    c = load(path)
    name = c["anim"].rsplit("/", 1)[1][3:]
    num, den = (125, 2) if str(c["fps"]) == "62.5" else (int(c["fps"]), 1)
    os.makedirs(OUT, exist_ok=True)
    spec = {"name": name, "source_anim": c["source_anim"], "rig": c.get("rig", RIG), "fps_num": num, "fps_den": den,
            "poses": key_frames(c), "segments": segments(c), "part_delays": c.get("part_delays", {}),
            "planted": c.get("planted", []), "edits": c.get("edits", []), "drop": c.get("drop", []), "hitstop_blend": c.get("hitstop_blend", []),
            "ease_out": {str(q["frame"]): q["ease_out"] for q in c["poses"] if q.get("ease_out")},
            "lock_lengths": c.get("lock_lengths", True), "out": os.path.join(OUT, f"{c['move']}_build_ue.json").replace("\\", "/")}
    t0 = time.time()
    r = _ue_py("build", spec=spec)
    if r.get("export_ok"):
        # legibility.py caches the bone dump by asset path only: a rebuild keeps the path, so drop the stale dump
        stale = os.path.join(TOOLS, "sheets", "raw", f"{name}_legbones.json")
        if os.path.exists(stale):
            os.remove(stale)
        n = int(round(r["length"] * num / den))
        frames = list(range(n + 1))
        A, B = _sample(c["source_anim"], frames), _sample(r["out_asset"], frames)
        per, at_pose = {}, {}
        for f in frames:
            for bn in BONES:
                d = sum((x - y) ** 2 for x, y in zip(A[f][bn], B[f][bn])) ** 0.5
                per[bn] = max(per.get(bn, 0.0), d)
                if f in spec["poses"]:
                    at_pose[bn] = max(at_pose.get(bn, 0.0), d)
        r.update({"vs_source_cm": {k: round(v, 2) for k, v in per.items()}, "at_poses_cm": {k: round(v, 2) for k, v in at_pose.items()}})
    r["sec"] = round(time.time() - t0, 1)
    path = os.path.join(OUT, f"{c['move']}_build.json")
    json.dump(r, open(path, "w"), indent=1)
    worst = max(r.get("vs_source_cm", {"-": -1}).items(), key=lambda kv: kv[1])
    wpose = max(r.get("at_poses_cm", {"-": -1}).items(), key=lambda kv: kv[1])
    print(f"build {c['move']}: bake {r.get('bake_ok')} export {r.get('export_ok')} keys {r.get('keys_before')}->{r.get('keys_after')} in {r['sec']} s")
    print(f"vs source: worst {worst[1]} cm ({worst[0]}); at the poses {wpose[1]} cm ({wpose[0]}) [MEASURE]")
    print(path)


def review(path, opt):
    """build -> check -> ingame_judge (hitstop + whiff + compare + strip), one command. The render is only worth the owner's
    time when the blocking checks are green or every FAIL is exempted in the card, so a FAIL stops it unless --force."""
    import subprocess
    from animpipe.card import load, windows
    c = load(path)
    build(path)
    from animpipe.checks import run
    run(path)
    v = json.load(open(os.path.join(OUT, f"{c['move']}_check.json")))
    if v["verdict"] != "PASS" and "force" not in opt:
        bad = [k for k, x in v["block"].items() if not x["ok"]]
        print(f"review {c['move']}: no render, blocking {', '.join(bad)} (fix or exempt with a reason in the card; --force renders anyway)")
        return
    at = opt.get("at") or str(windows(c)["anim"]["active"][0])
    env = dict(os.environ, MSYS_NO_PATHCONV="1", PYTHONIOENCODING="utf-8")
    p = subprocess.run([sys.executable, "ingame_judge.py", path, opt.get("hitstop", "8"), f"--at={at}"], cwd=TOOLS, env=env,
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    log = os.path.join(OUT, f"{c['move']}_review.txt")
    open(log, "w", encoding="utf-8").write((p.stdout or "") + (p.stderr or ""))
    tail = [l for l in (p.stdout or "").splitlines() if l.strip()][-2:]
    print(f"review {c['move']}: ingame rc {p.returncode}, " + " | ".join(t.strip()[:100] for t in tail))
    print(log)


def main():
    cmd, args = sys.argv[1], [a for a in sys.argv[2:] if not a.startswith("--")]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[2:] if a.startswith("--"))
    if cmd == "roundtrip":
        roundtrip(args[0], args[1], opt)
    elif cmd == "tangent":
        os.makedirs(OUT, exist_ok=True)
        r = _ue_py("tangent_probe", name=args[0], out=os.path.join(OUT, f"{args[0]}_tangent.json").replace("\\", "/"))
        print(json.dumps(r)[:400])
    elif cmd == "author":
        from animpipe.author import run
        run(args[0], opt)
    elif cmd == "build":
        build(args[0])
    elif cmd == "check":
        from animpipe.checks import run
        run(args[0], src="src" in opt)
    elif cmd == "review":
        review(args[0], opt)
    elif cmd == "trails":
        from animpipe.trails import run
        run(args[0])
    elif cmd == "poses":
        from animpipe.poses import run
        run(args[0], opt)
    elif cmd == "probe":
        r = _ue_py("probe", name=args[0], out=os.path.join(OUT, f"{args[0]}_probe.json").replace("\\", "/"))
        print(r, os.path.join(OUT, f"{args[0]}_probe.json"))
    elif cmd == "card":
        from animpipe.card import load, key_frames, windows
        c = load(args[0])
        print(f"card {c['move']} OK: keys {key_frames(c)} windows {windows(c)}")
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
