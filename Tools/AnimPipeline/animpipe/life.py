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
  stop_sync    lockstep stop EVENTS per second (most parts stop on the same frame = one event) over the frames the game
               shows (card `shown`): pose-interpolated clips stop everything on the key, mocap and good hand animation
               stagger. A flat run of minima is one minimum; minima inside a live declared hold are skipped (2026-11-24).
  holds core   every declared hold, any class: pelvis path >= life_hold_core_cm, else the hold is DEAD (2026-11-24).
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
     [--shown=a-b:why] [--card=<card.json>] [--json]. Repeated frames are dropped only on .jsonl captures (load_clip). Prints one JSON object with --json (exit 0 whatever the verdict: the verdict is
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
    "life_hold_core_cm": 1.0,            # owner 2026-11-24 (pose holds): a hold is normal (Sakurai) but never a block of
                                         # concrete: on EVERY class the pelvis travels >= this over the hold. Slowmo
                                         # 11-24: ftilt v3.1 kick hold 10-15 pelvis 1.4 cm (alive), jab v12 5-9 0.4 (dead)
    "life_hold_speed_rel": 0.06,         # = hold_speed_max (OG r.403)
    "life_stop_events_per_s_max": 2.5,   # lockstep stop events per second of the shown clip (stop_sync, 2026-11-24).
                                         # Calibration set 11-24: PASS clips <= 1.91 (stock jump squat; w2, w3, ftilt
                                         # v3.1 0), known FAIL >= 3.24 (jab v12, K7 4.0, jabfk3 6.5, K8 r2 8.0): the
                                         # midpoint. A MEASURE until life_calibrate --write-gate gives it a proof (2A)
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


def is_capture(path):
    """Only game captures (frames.jsonl, one sample per tick) can repeat a tick; an asset dump has one sample per anim frame."""
    return str(path).endswith(".jsonl")


def load_clip(path):
    """load() + dedupe() ONLY on captures. Bug fixed 2026-11-24 (slowmo jab/ftilt): life_calibrate deduped the asset dumps
    too, so the 20 identical frames of the jab v12 dead tail (f18-37, after FAF 19) collapsed into one sample and
    still_runs passed a clip the video shows frozen. On an asset, identical frames ARE the defect."""
    F = load(path)
    return dedupe(F) if is_capture(path) else F


def dedupe(F):
    """Capture chain artefact: a tick rendered twice gives a step of exactly 0 between two moving steps (land/skid/jumpsquat
    stock captures, 0.0 between 16 and 17 cm). Drop the repeated sample, so a duplicate tick never reads as a stop.
    Captures only (load_clip): never on an asset dump, where repeated frames are a real freeze."""
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


def hold_core_cm(F, a, b):
    """Pelvis path over the hold steps a..b-1 (cm, absolute: the pelvis carries the weight of the whole body)."""
    s = _speed(F, "pelvis") if "pelvis" in (F[0] if F else {}) else []
    return sum(s[a:min(b, len(s))])


def judge_holds(F, holds, cls, eff, T, per_hold=False):
    """Owner 2026-11-24 decision 1A (scope: all animations): a LIVE hold (pelvis or head keep moving) is a normal tool, declared
    with its reason and judged on video; the defects are DEAD holds and stop-and-go. So the checks split three ways:
      reasons -> holds without a written reason (a declaration rule, always blocking: see hold_reason in run());
      alive   -> dead core (pelvis path < life_hold_core_cm on EVERY class), frozen effector on a strong move, a "hold" that
                 moves too fast to be one: a METRIC, blocking only once calibrated (2A, life_gate.json);
      guides  -> count > life_hold_max_count, length > life_hold_max_frames(_smash): numbers for the eye, never a block
                 (the owner never approved them as caps).
    per_hold=True -> (dict above, [(a, b) of the holds with no alive/reason problem]): only those exempt speed minima
    from stop_sync (a dead or undeclared hold never does)."""
    out = {"reasons": [], "alive": [], "guides": []}
    good = []
    if len(holds) > T["life_hold_max_count"]:
        out["guides"].append(f"{len(holds)} holds declared (guide {T['life_hold_max_count']}: check each is wanted)")
    cap = T["life_hold_max_frames_smash"] if cls in STRONG else T["life_hold_max_frames"]
    reach = max((CV._dist(_rel(F, eff, f), (0, 0, 0)) for f in range(len(F))), default=0.0) if eff in _pts(F) else 0.0
    for a, b, why in holds:
        bad = False
        if not why:
            out["reasons"].append(f"hold {a}-{b} has no reason")
            bad = True
        if b - a > cap:
            out["guides"].append(f"hold {a}-{b} = {b - a} frames (guide {cap} for class {cls or '-'})")
        if b > a and F and "pelvis" in F[0]:
            core = hold_core_cm(F, a, b)
            if core < T["life_hold_core_cm"]:
                out["alive"].append(f"hold {a}-{b} dead: pelvis travels {core:.1f} cm (a live hold needs >= {T['life_hold_core_cm']} cm)")
                bad = True
        if eff in _pts(F) and b > a and b < len(F):
            v = _speed(F, eff)[a:b]
            amp = max(CV._dist(_rel(F, eff, f), _rel(F, eff, a)) for f in range(a, b + 1))
            if cls in STRONG and amp < T["life_hold_alive_cm"]:
                out["alive"].append(f"hold {a}-{b} frozen: {eff} moves {amp:.1f} cm (strong move needs >= {T['life_hold_alive_cm']} cm recoil)")
                bad = True
            if reach and max(v) > T["life_hold_speed_rel"] * reach:
                out["alive"].append(f"hold {a}-{b} is not a hold: {eff} {max(v):.1f} cm/frame > {T['life_hold_speed_rel']:.0%} of reach {reach:.0f}")
                bad = True
        if not bad:
            good.append((a, b))
    return (out, good) if per_hold else out["reasons"] + out["alive"]


# ------------------------------------------------------------------ calibration gate (owner 2026-11-24 decision 2A)
GATE_FILE = os.path.join(HERE, "life_gate.json")
DECLARATION = {"hold_reason"}        # declaration rules, not measures of motion: they block from day one


def blocking_metrics():
    """Metrics with a calibration proof (life_calibrate.py writes life_gate.json only from a COMPLETE run: every approved and
    stock clip passes the metric, at least one known-bad clip fails it). A metric not listed is a MEASURE shown to the
    reviewer: owner 2A, a new check is born as a measure and blocks only after its proof. Never edited by hand."""
    try:
        return set(json.load(open(GATE_FILE)).get("blocking", {}))
    except (OSError, ValueError):
        return set()


def apply_gate(res, blocking=None):
    """Move every uncalibrated metric from block to measure ("MEASURE (no calibration proof)"), recompute the verdict."""
    bl = blocking_metrics() if blocking is None else set(blocking)
    for k in list(res["block"]):
        if k not in bl and k not in DECLARATION:
            ok, line = res["block"].pop(k)
            res["measure"][k] = ("ok " if ok else "WOULD FAIL ") + line + " [measure: no calibration proof, owner 2A]"
    res["verdict"] = "PASS" if all(v[0] for v in res["block"].values()) else "FAIL"
    return res


# ------------------------------------------------------------------ metrics
def stop_events(mins, need):
    """Group the parts' minima into stop EVENTS, greedily: the frame where the most parts have a minimum within +-1 is one
    event (8 parts on the same frame = 1, not 8), its minima (one per part, the nearest) leave the pool, repeat. Greedy and
    not a chain of minima 1 frame apart, because a chain merges stops that follow each other (K8 r2 f7-12 was 1 chained
    event, it is 2 lockstep stops). -> [(frame, parts)] in frame order; lockstep when parts >= need."""
    pool = [(k, i) for i, ms in enumerate(mins) for k in ms]
    out = []
    while pool:
        near = lambda c: {i for k, i in pool if abs(k - c) <= 1}
        c = max(sorted({k for k, _ in pool}), key=lambda c: (len(near(c)), -c))
        taken = {(min((k for k, j in pool if j == i and abs(k - c) <= 1), key=lambda k: abs(k - c)), i) for i in near(c)}
        out.append((c, len(taken)))
        pool = [x for x in pool if x not in taken]
    return sorted(out)


def stop_sync(F, T, live_holds=(), shown=None, fps=60.0):
    """Lockstep stops of the body, counted as EVENTS per second of the clip the game shows (review dair v9 stops, 2026-11-24).
    Why events: the old per-part share (kb_motion stop_sync_share on points) counted one stop of 8 parts 8 times, and a
    stop of every part on one key is ONE defect the eye sees once. Why per second: stop-and-go is a stop on every key, i.e.
    a RATE; a long live clip may stop once or twice (stock jump squat 5 lockstep stops in 158 frames, 1.9/s), a pose-built
    clip stops on most keys (jab v12 3.2/s, K7 4.0/s, K8 r2 8.0/s, flat-tangent jabfk3 6.5/s).
      - minima: smoothed speed of each part (pelvis absolute, limbs relative to it) below 35% of its 90th percentile; a flat
        run of minima is ONE minimum (its first frame, jab v12 dead tail);
      - minima inside a DECLARED hold that passes judge_holds (reason, caps, live core) are the hold, not a stop: skipped;
        a dead, undeclared or over-cap hold is never skipped;
      - shown = (first, last) anim frames the in-game montage shows (card or calibration entry, with its source): minima
        on steps outside it are never on screen (dair v9 montage ends ~a34, the a35 stop was a false alarm) and the rate
        is over the shown steps. Without `shown` the whole clip counts (never trimmed by guess);
      - lockstep event: >= 1 + 60% of the other parts stop within +-1 frame (stop_events), the per-part rule's strictness.
    The per-part share stays in the report as a second number for the reviewer.
    -> dict(rate, lockstep [frames], events, share, minima, steps, outside) or None with fewer than 3 moving parts."""
    skip = set()
    for a, b in live_holds:
        skip |= set(range(a - 1, b + 1))       # the minimum of the smoothed speed sits within +-1 of the held steps
    lo, hi = (int(shown[0]), int(shown[1])) if shown else (0, len(F) - 1)
    mins, outside = [], set()
    for p in ["pelvis"] + _pts(F):
        s = _speed(F, p)
        if max(s, default=0) < T["life_stop_sync_min_cm"]:
            continue
        sm = [(s[max(0, i - 1)] + s[i] + s[min(len(s) - 1, i + 1)]) / 3 for i in range(len(s))]
        thr = 0.35 * sorted(sm)[int(0.9 * (len(sm) - 1))]
        ks = [k for k in range(1, len(sm) - 1) if sm[k] <= sm[k - 1] and sm[k] <= sm[k + 1] and sm[k] < thr]
        ks = [k for i, k in enumerate(ks) if i == 0 or k != ks[i - 1] + 1]     # a flat run of minima = one minimum
        outside |= {k for k in ks if not lo <= k < hi}
        mins.append([k for k in ks if k not in skip and lo <= k < hi])
    if len(mins) < 3:
        return None
    shared = total = 0
    for i, ms in enumerate(mins):
        for k in ms:
            total += 1
            n = sum(1 for o, mo in enumerate(mins) if o != i and any(abs(k - x) <= 1 for x in mo))
            shared += n >= 0.6 * (len(mins) - 1)
    need = 1 + 0.6 * (len(mins) - 1)
    ev = stop_events(mins, need)
    lock = [c for c, n in ev if n >= need]
    steps = max(1, hi - lo)
    return {"rate": round(len(lock) / steps * fps, 2), "lockstep": lock, "events": len(ev), "share": round(shared / total, 2) if total else 0.0,
            "minima": total, "steps": steps, "outside": sorted(outside), "need": round(need, 1)}


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


def run(F, window=None, contact=None, effector=None, holds=(), cls="", T=None, gate=True, shown=None, fps=None):
    """-> {"verdict": PASS|FAIL, "block": {name: [ok, line]}, "measure": {...}}. gate=True (default) applies the calibration
    gate (apply_gate); life_calibrate.py runs gate=False to measure every metric raw."""
    T = T or thresholds()
    res = {"frames": len(F), "points": ["pelvis"] + _pts(F), "block": {}, "measure": {}}
    H = parse_holds(holds) if isinstance(holds, str) else list(holds)
    held = set()
    for a, b, _ in H:
        held |= set(range(a, b))
    pts = _pts(F)
    hp, live = judge_holds(F, H, cls, effector or "hand_l", T, per_hold=True)
    res["block"]["hold_reason"] = [not hp["reasons"], "; ".join(hp["reasons"]) or f"{len(H)} declared, all with a reason"]
    res["block"]["hold_alive"] = [not hp["alive"], "; ".join(hp["alive"]) or "every declared hold is live"]
    res["measure"]["hold_guides"] = "; ".join(hp["guides"]) or "inside the guides (count, length)"
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
    sy = stop_sync(F, T, live, shown, fps or 60.0)
    res["block"]["stop_sync"] = [True, "n/a: fewer than 3 moving parts"] if sy is None else [
        sy["rate"] <= T["life_stop_events_per_s_max"],
        f"{sy['rate']}/s lockstep stop events (max {T['life_stop_events_per_s_max']}/s): {len(sy['lockstep'])} of {sy['events']} "
        f"events with >= {sy['need']} parts at {sy['lockstep'] or '-'} over {sy['steps']} "
        + (f"shown steps {shown[0]}-{shown[1]} (minima never on screen at {sy['outside'] or '-'})" if shown else "steps (whole clip)")
        + f"; per-part share {sy['share']} of {sy['minima']} minima; live declared holds skipped: {live or 'none'}"]
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
    kw = {"contact": contact, "effector": eff, "cls": c.get("class", ""), "holds": holds, "fps": float(c.get("fps", 60))}
    if c.get("shown"):
        kw["shown"] = parse_shown(c["shown"])
    return kw


def parse_shown(spec):
    """{"frames": [a, b], "why": "<where the montage window comes from>"} or "a-b:why" -> (a, b). The frames the in-game
    montage shows are a FACT about the game (capture or montage data), never a guess: without a why it is refused."""
    if isinstance(spec, str):
        rng, _, why = spec.partition(":")
        a, b = (int(x) for x in rng.split("-"))
    else:
        (a, b), why = spec["frames"], spec.get("why", "")
    if not str(why).strip() or not 0 <= a < b:
        raise ValueError(f"shown {spec!r}: needs frames a<b and the source of the montage window (why)")
    return (int(a), int(b))


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
    if is_capture(args[0]):                    # never on an asset dump: there repeated frames are the freeze to catch
        F = dedupe(F)
    if "holds" in opt:
        kw["holds"] = opt["holds"]
    if "shown" in opt:
        kw["shown"] = parse_shown(opt["shown"])
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
