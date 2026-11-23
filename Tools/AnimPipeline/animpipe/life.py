"""Life metrics of the original animation skill (OG, archived 2026-11-15, contract/archive/gstn-move-animation_2026-11-15.md
r.199-214, r.401-408), back as numbers on the BUILT clip. Why: from 15/11 the numeric checks only measured cleanliness
(lengths, penetrations, pops) and left life to the eye, which then received still grids at real speed; K7 and K8 passed
with the body stopping on every card key (diagnosis dropkick-k8-3). Owner 2026-11-24: quality equal to the OG; a metric
that gives false alarms is FIXED by calibration with a proof (life_calibrate.py), never softened or switched off.

What is measured (component space or capture space, cm, one sample per anim frame; limbs relative to the pelvis, the
pelvis itself absolute, as curves.body_steps does):
  stops        startup with no whole-body still step, largest step <= JUMP_RATIO x median   (reuses curves.stop_and_go)
  still_runs   whole clip (take-off, landing, settle, loop): no run of more than STILL_RUN still steps (curves.long_stills)
  holds        declared holds are the ONLY exemptions from the two above. Owner 2026-11-24: a stop is a defect by default,
               acceptable only as intended motion, exceptional and declared in the card with a reason; so a declaration
               without `why`, more than HOLD_MAX_COUNT holds, or a hold longer than HOLD_MAX_FRAMES (unless the card class
               is a smash, where OG's reference holds go to 12 frames) is itself a FAIL, and the steps inside a declared
               hold are re-judged: on a strong move a perfectly frozen hold is a defect (OG r.207), the effector must keep
               a small motion (>= HOLD_ALIVE_CM over the hold) and stay under 6% of the reach per frame (OG r.403).
  stop_sync    share of speed minima that most parts reach together (kb_motion stop_sync_share, here on points instead of
               joint rotations): pose-interpolated clips stop everything on the key, mocap and good hand animation stagger.
  cascade      in the strike window the speed peaks travel pelvis -> forearm -> hand (OG r.208-209, r.405): spread >= 1
               frame, each lag >= 0.5 frame, never hand before pelvis.
  impact       share of the effector path covered in the last 2 frames before contact >= 0.4 (OG r.401, motion_check
               spacing_metrics reused), speed peak on the frame into contact.
  anticipation before the strike the effector or the pelvis first moves against the attack direction by >= ANTICIP_CM.
  even_run     measure only: longest run of near-equal steps of a moving effector (linear interpolation signature).
`stops`, `still_runs`, `holds` and `stop_sync` apply to every clip; `cascade`, `impact`, `anticipation` need a strike
(contact frame and effector), so they are n/a on locomotion, settle and loop clips.

Inputs (load()): a UE joints dump (clip_joints_dump_ue.py / runstart_clip_joints_ue.py: {"joints": {name: [[x,y,z]..]}},
names like hand_MainPoint_l), a capture frames.jsonl (one JSON per tick with body.{pelvis,head,hand_*,foot_*} and
body.actor) or the frames list animpipe check builds. Missing points (captures have no forearms) are simply not used.

CLI: python -m animpipe life <dump> [--window=a-b] [--contact=c] [--effector=hand_l] [--holds=a-b:why,..] [--class=tilt]
     [--card=<card.json>] [--json]. Prints one JSON object with --json (exit 0 whatever the verdict: the verdict is
     inside, final_review_pack.py reads it), otherwise at most 5 lines; exit 1 on FAIL without --json.
Thresholds: motion_thresholds.json keys life_* (calibration notes there and in life_calibration.json)."""
import json
import os

from animpipe import curves as CV

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

DEF = {
    "life_stop_cm": CV.STOP_CM,          # diagnosis dropkick-k8-3 (K7 pelvis 3 2 0 0 5 ...)
    "life_jump_ratio": CV.JUMP_RATIO,
    "life_beat_max": 2,                  # anticipation reversals allowed in one action window (wind-up + one turn)
    "life_still_run": CV.STILL_RUN,
    "life_dead_cm": 0.4,                 # whole clip: a step this small on every point is a DEAD frame. Stock GASP mocap in
                                         # action (land, skid, jumpsquat captures) never goes under 0.5 cm/frame; at 3.5 cm
                                         # it would fail every stock landing recovery (1-3 cm/frame for 9-27 frames)
    "life_hold_max_count": 2,            # owner 2026-11-24: holds are exceptional
    "life_hold_max_frames": 6,           # UFD tilts: Terry ftilt 5 frozen, Ganondorf ftilt ~9 at 69% (a smash may hold 12)
    "life_hold_max_frames_smash": 12,    # Little Mac fsmash 12 frozen, Ryu fsmash ~13 with recoil
    "life_hold_alive_cm": 1.0,           # = recoil_min_cm: a strong hold keeps a few cm of recoil (OG r.206)
    "life_hold_speed_rel": 0.06,         # = hold_speed_max (OG r.403)
    "life_stop_sync_max": 0.5,           # kb_motion: mocap 0-0.25, pose-interpolated 0.88-0.96
    "life_stop_sync_min_cm": 0.5,        # a part slower than this all clip long has no minima worth counting
    "life_cascade_spread_min": 1.0,      # = overlap_min_spread_frames
    "life_cascade_lag_min": 0.5,         # = overlap_min_lag_frames
    "life_impact_share_min": 0.4,        # = impact_share_last2_min (UFD: Little Mac 45%, even 8-frame move 25%)
    "life_anticip_cm": 2.0,              # = counterbalance_min_cm
    "life_even_rel": 0.06,
}
STRONG = {"smash", "strong", "heavy"}
POINTS = CV.GATE_POINTS


def thresholds():
    t = dict(DEF)
    p = os.path.join(HERE, "motion_thresholds.json")
    if os.path.exists(p):
        t.update({k: v for k, v in json.load(open(p)).items() if k.startswith("life_")})
    return t


# ------------------------------------------------------------------ input
def _name(n):
    return n.replace("_MainPoint", "").replace("middle_01", "hand")


def load(path):
    """-> [{point: (x, y, z)}] one per frame, points among POINTS + pelvis."""
    if path.endswith(".jsonl"):
        F = []
        for line in open(path):
            if not line.strip():
                continue
            b = json.loads(line).get("body") or {}
            if not isinstance(b.get("pelvis"), list):
                continue
            off = b.get("actor") or [0.0, 0.0, 0.0]     # limbs are actor-relative; the pelvis gets the actor travel back
            fr = {"pelvis": tuple(float(p) + float(o) for p, o in zip(b["pelvis"], off))}
            for k in POINTS:
                if isinstance(b.get(k), list):
                    fr[k] = tuple(float(p) + float(o) for p, o in zip(b[k], off))
            F.append(fr)
        return F
    d = json.load(open(path))
    if "frames" in d and isinstance(d["frames"], list):      # list of {point: xyz}
        return [{_name(k): tuple(v) for k, v in fr.items()} for fr in d["frames"]]
    J = d.get("joints", d)
    J = {_name(k): v for k, v in J.items() if isinstance(v, list) and v and isinstance(v[0], list)}
    n = min(len(v) for v in J.values())
    return [{k: tuple(float(x) for x in J[k][f]) for k in J if k in POINTS or k == "pelvis"} for f in range(n)]


def dedupe(F):
    """Capture chain artefact: a tick rendered twice gives a step of exactly 0 between two moving steps (land/skid/jumpsquat
    stock captures, 0.0 between 16 and 17 cm). Drop the repeated sample, so a duplicate tick never reads as a stop."""
    out = [F[0]] if F else []
    for f in F[1:]:
        if any(CV._dist(f[k], out[-1][k]) > 1e-4 for k in f if k in out[-1]):
            out.append(f)
    return out


def trim(F, rng):
    a, b = rng
    return F[a:b + 1]


def _pts(F):
    return [p for p in POINTS if all(p in f for f in F)]


def _rel(F, p, f):
    a, b = F[f][p], F[f]["pelvis"]
    return tuple(x - y for x, y in zip(a, b))


def _speed(F, p):
    """cm per frame of point p (relative to the pelvis; the pelvis absolute)."""
    if p == "pelvis":
        return [CV._dist(F[f]["pelvis"], F[f + 1]["pelvis"]) for f in range(len(F) - 1)]
    return [CV._dist(_rel(F, p, f), _rel(F, p, f + 1)) for f in range(len(F) - 1)]


# ------------------------------------------------------------------ holds (owner 2026-11-24)
def parse_holds(spec):
    """'7-9:why,12-13:why' or card list [{"frames": [a, b], "why": "..."}] -> [(a, b, why)] steps a..b-1 exempt."""
    out = []
    if isinstance(spec, str):
        for h in filter(None, spec.split(",")):
            rng, _, why = h.partition(":")
            a, b = (int(x) for x in rng.split("-"))
            out.append((a, b, why.strip()))
    else:
        for h in spec or []:
            a, b = h["frames"]
            out.append((int(a), int(b), str(h.get("why", "")).strip()))
    return out


def judge_holds(F, holds, cls, eff, T):
    """A declaration cannot cover a defect: reason required, count and length capped, and on a strong move the held
    effector must stay alive (recoil) without drifting off the pose."""
    probs = []
    if len(holds) > T["life_hold_max_count"]:
        probs.append(f"{len(holds)} holds declared (max {T['life_hold_max_count']})")
    cap = T["life_hold_max_frames_smash"] if cls in STRONG else T["life_hold_max_frames"]
    reach = max((CV._dist(_rel(F, eff, f), (0, 0, 0)) for f in range(len(F))), default=0.0) if eff in _pts(F) else 0.0
    for a, b, why in holds:
        if not why:
            probs.append(f"hold {a}-{b} has no reason")
        if b - a > cap:
            probs.append(f"hold {a}-{b} = {b - a} frames (max {cap} for class {cls or '-'})")
        if eff in _pts(F) and b > a and b < len(F):
            v = _speed(F, eff)[a:b]
            amp = max(CV._dist(_rel(F, eff, f), _rel(F, eff, a)) for f in range(a, b + 1))
            if cls in STRONG and amp < T["life_hold_alive_cm"]:
                probs.append(f"hold {a}-{b} frozen: {eff} moves {amp:.1f} cm (strong move needs >= {T['life_hold_alive_cm']} cm recoil)")
            if reach and max(v) > T["life_hold_speed_rel"] * reach:
                probs.append(f"hold {a}-{b} is not a hold: {eff} {max(v):.1f} cm/frame > {T['life_hold_speed_rel']:.0%} of reach {reach:.0f}")
    return probs


# ------------------------------------------------------------------ metrics
def stop_sync(F, T):
    """Share of the parts' speed minima that >= 60% of the other parts also reach within +-1 frame."""
    mins = []
    for p in ["pelvis"] + _pts(F):
        s = _speed(F, p)
        if max(s, default=0) < T["life_stop_sync_min_cm"]:
            continue
        sm = [(s[max(0, i - 1)] + s[i] + s[min(len(s) - 1, i + 1)]) / 3 for i in range(len(s))]
        thr = 0.35 * sorted(sm)[int(0.9 * (len(sm) - 1))]
        mins.append({k for k in range(1, len(sm) - 1) if sm[k] <= sm[k - 1] and sm[k] <= sm[k + 1] and sm[k] < thr})
    if len(mins) < 3:
        return None, 0
    shared = total = 0
    for i, ms in enumerate(mins):
        for k in ms:
            total += 1
            n = sum(1 for o, mo in enumerate(mins) if o != i and any(abs(k - x) <= 1 for x in mo))
            shared += n >= 0.6 * (len(mins) - 1)
    return (round(shared / total, 2) if total else None), total


def _peak(s, lo, hi):
    seg = s[lo:hi]
    i = max(range(len(seg)), key=lambda j: seg[j]); k = lo + i
    off = 0.0
    if 0 < i < len(seg) - 1:
        y0, y1, y2 = seg[i - 1], seg[i], seg[i + 1]
        den = y0 - 2 * y1 + y2
        off = 0.5 * (y0 - y2) / den if abs(den) > 1e-9 else 0.0
    return k + 0.5 + off


def cascade(F, lo, hi, eff):
    """Peak times pelvis -> forearm -> hand of the striking side (or foot for kicks: pelvis -> foot only)."""
    side = eff[-1]
    chain = ["pelvis"] + [p for p in (f"lowerarm_{side}", eff) if p in _pts(F) and p != "pelvis"]
    if len(chain) < 2 or hi - lo < 3:
        return None
    ts = [round(_peak(_speed(F, p), lo, hi), 2) for p in chain]
    return {"chain": chain, "peaks": ts, "spread": round(max(ts) - min(ts), 2), "lags": [round(ts[i + 1] - ts[i], 2) for i in range(len(ts) - 1)]}


def impact(F, c0, eff):
    """motion_check.spacing_metrics on the effector relative to the pelvis, from the anticipation key (furthest back along
    the attack) to contact."""
    pos = [_rel(F, eff, f) for f in range(len(F))]
    a = [x - y for x, y in zip(pos[c0], pos[0])]
    k0 = min(range(0, c0), key=lambda f: sum(p * q for p, q in zip(pos[f], a)))
    d = [CV._dist(pos[t], pos[t + 1]) for t in range(k0, c0)]
    tot = sum(d) or 1e-9
    return {"anticipation_key": k0, "share_last2": round(sum(d[-2:]) / tot, 3),
            "peak_on_contact_frame": bool(d) and max(range(len(d)), key=lambda i: d[i]) == len(d) - 1}


def anticipation(F, c0, eff):
    """Largest move AGAINST the attack direction before contact, for the effector (relative) and the pelvis (absolute)."""
    out = {}
    for p in (eff, "pelvis"):
        pos = [(_rel(F, p, f) if p != "pelvis" else F[f]["pelvis"]) for f in range(c0 + 1)]
        a = [x - y for x, y in zip(pos[c0], pos[0])]
        n = sum(x * x for x in a) ** 0.5 or 1e-9
        proj = [sum((q - r) * x for q, r, x in zip(pt, pos[0], a)) / n for pt in pos]
        out[p] = round(-min(proj), 1)
    return out


def even_run(F, eff, rel):
    s = _speed(F, eff)
    best = run = 0
    for k in range(1, len(s)):
        if s[k] > 0.5 and s[k - 1] > 0.5 and abs(s[k] - s[k - 1]) <= rel * max(s[k], s[k - 1]):
            run += 1
        else:
            run = 0
        best = max(best, run)
    return best + 1 if best else 0


def stop_go(F, win, held, contact, T, pts):
    """Stop-and-go in the action window, calibrated 2026-11-24 against the OG strike profile. curves.stop_and_go as it was
    (every step >= 3.5 cm, largest <= 4x median) FAILS a textbook OG strike (long ease-in, anticipation reversal, 1-2
    frame snap with >= 40% of the path: test_good_strike_passes), i.e. it would bounce ftilt v3.1 and Terry's ftilt.
    The defect the owner saw on K7/K8 is GO -> STOP -> GO in the SAME direction (flat tangents on keys). So:
      stop  = a step whose whole-body travel is < life_stop_cm AND < half the travel both in the 3 steps before and in
              the 3 steps after (a dip between moving steps; an ease-in from rest or an ease-out to the end is no dip),
      beat  = the same dip when the body's motion REVERSES across it (anticipation turn, OG r.232 'a beat, not jitter'):
              allowed for at most life_beat_max steps,
      pop   = largest step > life_jump_ratio x median, ignoring the 2 steps into contact (the impact snap OG wants).
    Returns (ok, line)."""
    steps = CV.body_steps(F, pts)
    a, b = int(win[0]), min(int(win[1]), len(steps))
    sp = [x[0] for x in steps]
    stops, beats = [], []
    for f in range(a, b):
        if f in held or sp[f] >= T["life_stop_cm"]:
            continue
        pre, post = sp[max(a, f - 3):f], sp[f + 1:min(b, f + 4)]
        if not pre or not post or max(pre) < 2 * sp[f] or max(post) < 2 * sp[f]:
            continue
        # reversal = the body's motion turns back across the dip: sum over the points of d_before . d_after < 0
        lo, hi = max(0, f - 2), min(len(F) - 1, f + 3)
        def disp(p, i, j):
            u = _rel(F, p, j) if p != "pelvis" else F[j]["pelvis"]
            v = _rel(F, p, i) if p != "pelvis" else F[i]["pelvis"]
            return [x - y for x, y in zip(u, v)]
        turn = sum(sum(x * y for x, y in zip(disp(p, lo, f), disp(p, f + 1, hi))) for p in ["pelvis"] + pts)
        (beats if turn < 0 else stops).append(f)
    # a beat is one reversal: consecutive reversal steps are one beat; a reversal that lingers > 2 steps is a stop
    runs, cur = [], []
    for f in beats + [None]:
        if cur and (f is None or f != cur[-1] + 1):
            runs.append(cur); cur = []
        if f is not None:
            cur.append(f)
    for r in runs:
        if len(r) > 2:
            stops += r
    nbeats = sum(1 for r in runs if len(r) <= 2)
    snap = set(range(contact - 2, contact)) if contact is not None else set()
    srt = sorted(sp[a:b]) or [0.0]
    med = srt[len(srt) // 2]
    # pop = an isolated spike: > ratio x the smaller neighbour AND > ratio x the window median (an accelerating snap
    # 7 16 30 is no spike; K8 hand 56-59 cm then 0-1 cm is). The 2 steps into contact are the wanted impact.
    pops = [f for f in range(a, b) if f not in held and f not in snap and med > 1e-6 and sp[f] > T["life_jump_ratio"] * med
            and sp[f] > T["life_jump_ratio"] * min(sp[f - 1] if f > 0 else sp[f], sp[f + 1] if f + 1 < len(sp) else sp[f])]
    ok = not stops and nbeats <= T["life_beat_max"] and not pops
    line = (f"steps {a}-{b} cm/frame " + " ".join(f"{x:.0f}" for x in sp[a:b]) + f"; stop-and-go dips {stops or 'none'}; "
            f"reversal beats {nbeats} at {beats or '-'} (max {T['life_beat_max']}); pops {pops or 'none'} (> {T['life_jump_ratio']}x "
            f"median {med:.1f} and the smaller neighbour; impact snap {sorted(snap) or '-'} excluded)")
    return ok, line


def run(F, window=None, contact=None, effector=None, holds=(), cls="", T=None):
    """-> {"verdict": PASS|FAIL, "block": {name: [ok, line]}, "measure": {...}}."""
    T = T or thresholds()
    res = {"frames": len(F), "points": ["pelvis"] + _pts(F), "block": {}, "measure": {}}
    H = parse_holds(holds) if isinstance(holds, str) else list(holds)
    held = set()
    for a, b, _ in H:
        held |= set(range(a, b))
    pts = _pts(F)
    hp = judge_holds(F, H, cls, effector or "hand_l", T)
    res["block"]["holds"] = [not hp, "; ".join(hp) or f"{len(H)} declared, all with reason, inside the caps"]
    # stops: the ACTION window (startup up to contact, or the card's window); a strike-less clip (locomotion, settle,
    # loop) has no startup and is judged by still_runs and stop_sync only
    win = window or ([0, contact] if contact is not None else None)
    if win:
        ok, line = stop_go(F, win, held, contact, T, pts)
        res["block"]["stops"] = [ok, line]
    else:
        res["measure"]["stops"] = "n/a: no action window (give --window or a contact frame)"
    runs = CV.long_stills(F, held, T["life_dead_cm"], int(T["life_still_run"]), points=pts)
    res["block"]["still_runs"] = [not runs, f"runs > {T['life_still_run']} dead steps (< {T['life_dead_cm']} cm/frame): " + (", ".join(f"{a}-{b + 1}" for a, b in runs) or "none")]
    ss, n = stop_sync(F, T)
    res["block"]["stop_sync"] = [ss is None or ss <= T["life_stop_sync_max"],
                                 f"{ss} of {n} speed minima shared by most parts (max {T['life_stop_sync_max']})" if ss is not None else "n/a: fewer than 3 moving parts"]
    if contact is not None and effector in pts and contact >= 2:
        c = cascade(F, max(0, win[0]), contact + 1, effector)
        if c:
            okc = c["spread"] >= T["life_cascade_spread_min"] and all(l >= T["life_cascade_lag_min"] for l in c["lags"])
            res["block"]["cascade"] = [okc, f"{' > '.join(c['chain'])} peaks {c['peaks']} spread {c['spread']} (>= {T['life_cascade_spread_min']}) lags {c['lags']} (each >= {T['life_cascade_lag_min']})"]
        im = impact(F, contact, effector)
        res["block"]["impact"] = [im["share_last2"] >= T["life_impact_share_min"] and im["peak_on_contact_frame"],
                                  f"{effector} {im['share_last2']:.0%} of the path in the last 2 frames from key {im['anticipation_key']} (>= {T['life_impact_share_min']:.0%}), peak on contact {im['peak_on_contact_frame']}"]
        an = anticipation(F, contact, effector)
        res["block"]["anticipation"] = [max(an.values()) >= T["life_anticip_cm"], f"against the attack: {an} cm (one >= {T['life_anticip_cm']})"]
        res["measure"]["even_run"] = even_run(F, effector, T["life_even_rel"])
    else:
        for k in ("cascade", "impact", "anticipation"):
            res["measure"][k] = "n/a: no strike (contact frame and effector not given)"
    res["verdict"] = "PASS" if all(v[0] for v in res["block"].values()) else "FAIL"
    return res


def from_card(path):
    """Card -> keyword args: window = startup, contact = first active frame, effector, class, holds (card `life_holds`)."""
    from animpipe import card as CD
    c = CD.load(path)
    try:                                       # the same windows animpipe check uses (needs the timing csv in Saved/)
        contact = int(CD.windows(c)["anim"]["active"][0])
    except (OSError, KeyError):
        act = c.get("active") or c.get("hitbox_active")
        contact = int(act[0]) if isinstance(act, list) and act else c.get("contact")
    eff = _name(c.get("effector", "hand_l")).replace("ball", "foot")
    holds = parse_holds(c.get("life_holds", []))
    keys = [int(p["frame"]) for p in c["poses"]]
    for p in c["poses"]:                       # same rule as checks._life: a held/impact pose is a declared hold
        if p.get("hold") or p.get("impact"):
            fr = sorted(CV.held_frames([int(p["frame"])], keys))
            if fr:
                holds.append((fr[0], fr[-1] + 1, str(p.get("why", "")).strip() or ("impact pose" if p.get("impact") and not p.get("hold") else "")))
    return {"contact": contact, "effector": eff, "cls": c.get("class", ""), "holds": holds}


def main(argv):
    opt = dict(a[2:].split("=", 1) if "=" in a else (a[2:], "1") for a in argv if a.startswith("--"))
    args = [a for a in argv if not a.startswith("--")]
    kw = from_card(opt["card"]) if "card" in opt else {}
    if "window" in opt:
        kw["window"] = [int(x) for x in opt["window"].split("-")]
    if "contact" in opt:
        kw["contact"] = int(opt["contact"])
    if "effector" in opt:
        kw["effector"] = opt["effector"]
    F = load(args[0])
    if "range" in opt:
        F = trim(F, [int(x) for x in opt["range"].split("-")])
    if "dedupe" in opt or args[0].endswith(".jsonl"):
        F = dedupe(F)
    if "holds" in opt:
        kw["holds"] = opt["holds"]
    if "class" in opt:
        kw["cls"] = opt["class"]
    r = run(F, **kw)
    r["clip"] = args[0]
    if "json" in opt:
        print(json.dumps(r))
        return 0
    bad = [k for k, v in r["block"].items() if not v[0]]
    print(f"LIFE: {r['verdict']} {os.path.basename(args[0])} ({r['frames']} frames)")
    for k in bad[:4]:
        print(f"  {k}: {r['block'][k][1][:160]}")
    return 0 if r["verdict"] == "PASS" else 1
