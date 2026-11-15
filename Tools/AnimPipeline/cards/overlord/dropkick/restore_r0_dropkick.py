"""Owner decision 1A (2026-11-15): put the Antigravity r0 startup + loop poses back (legs together, arms crossed),
keep timing / hold / hitbox code, and re-key only the START of the land clip so it leaves from the r0 loop pose.

    python restore_r0_dropkick.py      (run in this folder; writes the 3 cards, backs the current ones up as *.r1.backup.json)

What it does, per card:
- startup, loop: author poses, pose list, spacing and part_delays from `*.r0.backup.json` (r0 has overlap delays on
  the startup: chest 1, head 2, forearm 2, hand 3 = the secondary motion the owner asked for). Added on every pose:
  fists closed (`fingers` 1) and `thumb: along`, as the r1 cards had (fists-closed rule, no shape change).
- land: f0 = r0 loop pose at the r1 land f0 pelvis height (the ground contact the game cut was tuned on); f1, f2 =
  f0 blended toward r1 f3 at 1/3, 2/3 (the crossed arms open into the impact spread: follow-through); f3..f31 = r1.
"""
import copy, json, os, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
P = lambda n, s="": os.path.join(HERE, f"overlord_dropkick_{n}{s}.json")


def lerp(a, b, t):
    if isinstance(a, dict):
        return {k: lerp(a[k], b[k], t) if k in b else copy.deepcopy(a[k]) for k in a}
    if isinstance(a, list):
        return [round(x + (y - x) * t, 3) for x, y in zip(a, b)]
    return a


def fist(pose):
    pose["fingers"] = [0.0, 0.0, 0.0]     # 0 = template fists (author.py); r1's 1.0 was the open villain hand
    return pose


def main():
    for n in ("startup", "loop", "land"):
        if not os.path.exists(P(n, ".r1.backup")):
            shutil.copy(P(n), P(n, ".r1.backup"))
    r1 = {n: json.load(open(P(n, ".r1.backup"))) for n in ("startup", "loop", "land")}
    r0 = {n: json.load(open(P(n, ".r0.backup"))) for n in ("startup", "loop")}
    note = ("r2 (2026-11-15, owner decision 1A): Antigravity r0 poses restored (legs together and straight, arms crossed on "
            "the chest). r1 (26733f24, legs staggered, arms flung) kept as *.r1.backup.json. Timing, hold-until-landed and "
            "hitbox code untouched.")
    for n in ("startup", "loop"):
        card = copy.deepcopy(r0[n])
        card["author"]["thumb"] = "along"
        ps = card["author"]["poses"]
        n_fr = card["author"]["frames"]
        if str(n_fr) in ps:   # r0 loop keyed f12 (= f0) on a 12-frame clip (frames 0-11): key f11 on the way back instead
            ps[str(n_fr - 1)] = lerp(ps["6"], ps.pop(str(n_fr)), 5 / 6)
            for p in card["poses"]:
                if p["frame"] == n_fr:
                    p["frame"] = n_fr - 1
        for p in ps.values():
            fist(p)
        card["_design_r1"] = r1[n].get("_design", "")
        card["_changes"] = note
        json.dump(card, open(P(n), "w"), indent=1)
    land = copy.deepcopy(r1["land"])
    # land f0 = r0 loop pose on the ground (r1 land f0 pelvis loc), then blend into r1 f3
    loop0 = fist(copy.deepcopy(r0["loop"]["author"]["poses"]["0"]))
    lp = land["author"]["poses"]
    f0 = copy.deepcopy(loop0)
    f0["pelvis"]["loc"] = list(lp["0"]["pelvis"]["loc"])
    f0.setdefault("wrist_out", lp["0"].get("wrist_out", [0.0, 0.0, 0.0]))
    f3 = copy.deepcopy(lp["3"])           # blend target only: r1 f3 itself is not changed
    for k in ("leg_l", "leg_r"):          # r1 legs carry no foot_rot (= 0): r0's -45 fades to 0 over f1-f2
        f3[k].setdefault("foot_rot", [0.0, 0.0, 0.0])
    lp["0"] = f0
    lp["1"] = lerp(f0, f3, 1 / 3)
    lp["2"] = lerp(f0, f3, 2 / 3)
    land["_changes"] = (land.get("_changes", "") + " " + note +
                        " Land: f0 = r0 loop pose on the ground, f1-f2 blend into r1 f3 (arms open on impact), f3-f31 = r1.").strip()
    json.dump(land, open(P("land"), "w"), indent=1)
    print("ok: startup", sorted(map(int, json.load(open(P("startup")))["author"]["poses"])),
          "loop", sorted(map(int, json.load(open(P("loop")))["author"]["poses"])), "land f0-f2 re-keyed")


if __name__ == "__main__":
    main()
