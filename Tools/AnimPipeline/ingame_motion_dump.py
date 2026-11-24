"""In-game motion of a clip from a capture's frames.jsonl (C5 2026-11-24, question of C3: "is the dair v9 a4 hold alive in game through
the capsule motion?"). No editor: pie_capture_move already writes, per rendered frame, the actor (capsule) world position, the mesh
pelvis/head/hands/feet relative to the actor and the montage position of the playing clip.

    python Tools/AnimPipeline/ingame_motion_dump.py <capture_dir>/<sequence>/frames.jsonl <clip substring> [--to=14] [--out=x.json]

Per anim frame: world step of the pelvis and head (capsule + pose), the capsule step alone and the POSE step alone (relative to the
actor). A held pose that rides the capsule shows pose step ~0 with a world step equal to the capsule step: the body translates as a
block, nothing inside it moves. Whether that reads alive is the reviewer's call (rule 6), the numbers only say which one it is."""
import json
import math
import sys


def rows_of(path, clip, to_frame):
    out = []
    prev = None
    for line in open(path, encoding="utf-8"):
        if not line.strip():
            continue
        r = json.loads(line)
        a = next((x for x in (r.get("combat") or {}).get("anim") or [] if clip in x), None)
        if not a or "body" not in r:
            continue
        _, w, t = a.split()
        af = float(t) * 60.0
        if af > to_frame:
            break
        b = r["body"]
        act = b["actor"]
        rel = {k: b[k] for k in ("pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r") if k in b}
        row = {"f": r["f"], "a": round(af, 2), "weight": float(w), "actor": act}
        if prev:
            row["capsule_step_cm"] = round(math.dist(act, prev["actor"]), 2)
            row["pose_step_cm"] = {k: round(math.dist(v, prev["rel"][k]), 2) for k, v in rel.items() if k in prev["rel"]}
            row["world_step_cm"] = {k: round(math.dist([act[i] + v[i] for i in range(3)],
                                                       [prev["actor"][i] + prev["rel"][k][i] for i in range(3)]), 2)
                                    for k, v in rel.items() if k in prev["rel"]}
        out.append(row)
        prev = {"actor": act, "rel": rel}
    return out


def main(argv):
    path, clip = argv[1], argv[2]
    to = float(next((a.split("=", 1)[1] for a in argv if a.startswith("--to=")), 14))
    rows = rows_of(path, clip, to)
    dst = next((a.split("=", 1)[1] for a in argv if a.startswith("--out=")), None)
    if dst:
        json.dump({"source": path, "clip": clip, "rows": rows}, open(dst, "w", encoding="utf-8"), indent=1)
    for r in rows[1:]:
        ps = r["pose_step_cm"]
        print(f"a{r['a']:5.2f} capsule {r['capsule_step_cm']:5.1f} cm | pose max {max(ps.values()):4.1f} cm "
              f"(pelvis {ps.get('pelvis', 0):4.1f}, head {ps.get('head', 0):4.1f}, hands {ps.get('hand_l', 0):4.1f}/{ps.get('hand_r', 0):4.1f})")


if __name__ == "__main__":
    main(sys.argv)
