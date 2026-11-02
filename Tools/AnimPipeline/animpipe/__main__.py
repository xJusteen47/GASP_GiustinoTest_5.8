"""animpipe: one entry point, versions are data (move card JSON + Level Sequence), not scripts.

    python -m animpipe roundtrip <AnimAsset> <name> [--fps=60|62.5] [--keys=card.json|0,3,9]   rig bake round trip
    python -m animpipe tangent <name>                                                             tangent probe on LS_<name>
    python -m animpipe card <card.json>                                                           validate a move card

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
        raise RuntimeError(f"{entry}: {str(v)[:300]} | log: {str(r.get('log_output'))[-600:]}")
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


def main():
    cmd, args = sys.argv[1], [a for a in sys.argv[2:] if not a.startswith("--")]
    opt = dict((a[2:].split("=", 1) + [""])[:2] for a in sys.argv[2:] if a.startswith("--"))
    if cmd == "roundtrip":
        roundtrip(args[0], args[1], opt)
    elif cmd == "tangent":
        os.makedirs(OUT, exist_ok=True)
        r = _ue_py("tangent_probe", name=args[0], out=os.path.join(OUT, f"{args[0]}_tangent.json").replace("\\", "/"))
        print(json.dumps(r)[:400])
    elif cmd == "card":
        from animpipe.card import load, key_frames, windows
        c = load(args[0])
        print(f"card {c['move']} OK: keys {key_frames(c)} windows {windows(c)}")
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
