"""Pre-flight: the move's checks on the MODEL's rig points, in under a second, before any Cascadeur/Unreal pass (a full pass
costs about 2.5 min). Same rules and thresholds as motion_check / balance_check / pose_rules / arm_check
(motion_thresholds.json), measured on the Cascadeur IK points instead of the baked Unreal bones, so the numbers are
approximate (a few degrees, about 1 cm): it catches gross errors early, the real checks still run after the pass.

Input: <take>_targets_raw.json written by the motion model ("targets" {point: [per-frame xyz]}, "feet", "keyplan", "move").
Cascadeur space: X = character's left, Y up, Z forward; the ground is the XZ plane.
Point -> joint: thigh_MainPoint = hip, calf_MainPoint = knee, foot_MainPoint = ankle, ball_MainPoint = ball, upperarm_MainPoint
= shoulder, lowerarm_MainPoint = elbow, hand_MainPoint = wrist; head forward = head_AdditionalPoint - head_MainPoint.

Sections:
  1 limbs    knee and elbow bend from the hip-ankle and shoulder-wrist DISTANCES and the bone lengths (idle_points.json):
             that is the bend the rig will produce whatever the knee/elbow points say. Flags a limb that cannot reach (the
             IK stretches or pulls the root), a knee/elbow point off its bone lengths (the rig will move it), and the
             elbow direction (arm_check rules: pole up / flared out / chicken wing).
  2 balance  centre of mass (segment masses as in the v2 model) inside the support base, 2 cm (balance_check geometry);
             weight over the support foot before lift-off; support knee 10-45 deg on one foot; kicks: hips drive >= 8 cm
             from the anticipation key and the upper body counter-leans >= 2 cm relative to them.
  3 keyplan  the same key frames (and lag) on every layer of an IK chain (Pelvis/Leg/Foot, Torso/Arm/Hand); the snap and
             the contact keyed; Bezier intervals simulated (Catmull-Rom tangents) for overshoot outside the key values and
             for drift from the model's curve; clamped intervals for a key in mid-motion (the path stops there).
  4 spacing  striking limb from the anticipation key to contact: per-frame steps, share of the path in the last 2 frames
             (>= 40%), the peak step, and "too even" (peak step < 1.5 x the mean); the recovery from the hold to the guard:
             fast out (speed peak in its first 60%) and slow in (last approach step <= 0.6 x the largest; spacing_rules.py).
  5 overlap  velocity-peak order (kick chain pelvis -> hip -> knee -> ankle; body pelvis -> chest -> head) and
             simultaneous arrivals into the settle (parts reaching the final pose on the same frame).
  6 arcs     straight paths (no arc), velocity reversals and turning zig-zags outside the beats (snap, contact, wind-up
             reversal, lift-off, landing), twinning at contact and mid-hold, head off the target, planted-foot slide.

Usage: python preflight.py <targets_raw.json> [move-sheet key] [--quiet]   -> sheets/<take>_preflight.json, exit 1 on flags.
       The take is the file name before "_targets"; the move sheet defaults to the take."""
import sys, os, json, math, time
import numpy as np

T0 = time.time()
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from balance_check import hull, outside            # convex hull + signed distance, cm (shared with the Unreal check)

MASS = [("head", 0.081), ("trunk", 0.497), ("ua_l", 0.028), ("ua_r", 0.028), ("fa_l", 0.016), ("fa_r", 0.016), ("hd_l", 0.006),
        ("hd_r", 0.006), ("th_l", 0.10), ("th_r", 0.10), ("sh_l", 0.0465), ("sh_r", 0.0465), ("ft_l", 0.0145), ("ft_r", 0.0145)]
UPPER = {"head", "trunk", "ua_l", "ua_r", "fa_l", "fa_r", "hd_l", "hd_r"}
PLANT_TOL, FOOT_HALF_W = 2.5, 4.5
# layer -> IK chains it belongs to (the rig's chains span layers: hip on Pelvis, knee on Leg, ankle on Foot; shoulder on
# Torso, elbow on Arm, fist on Hand). Matched on the lower-case layer name.
CHAIN_OF = [("pelvis", ("leg_l", "leg_r")), ("hip", ("leg_l", "leg_r")), ("torso", ("arm_l", "arm_r")), ("spine", ("arm_l", "arm_r")),
            ("chest", ("arm_l", "arm_r")), ("leg_l", ("leg_l",)), ("foot_l", ("leg_l",)), ("leg_r", ("leg_r",)), ("foot_r", ("leg_r",)),
            ("arm_l", ("arm_l",)), ("hand_l", ("arm_l",)), ("arm_r", ("arm_r",)), ("hand_r", ("arm_r",))]


def unit(v):
    n = np.linalg.norm(v)
    return v / n if n > 1e-9 else v

def ang(a, b):
    return math.degrees(math.acos(float(np.clip(np.dot(unit(a), unit(b)), -1, 1))))

def g2(v):
    return np.array([v[0], v[2]])                  # ground plane (X left, Z forward)


_JSON = {}
def _cached_json(name):
    """Read-only inputs loaded once per process (auto_fix runs the pre-flight hundreds of times)."""
    if name not in _JSON:
        _JSON[name] = json.load(open(os.path.join(HERE, name)))
    return _JSON[name]


class Move:
    def __init__(self, path, sheet_key=None, data=None, take=None):
        D = data if data is not None else json.load(open(path))       # data: the targets in memory (auto_fix.py)
        self.take = take or os.path.basename(path).split("_targets")[0]
        self.P = {n: np.array(v, float) for n, v in D["targets"].items()}
        self.N = len(next(iter(self.P.values())))
        for n, v in (D.get("feet") or {}).items():       # constant points
            self.P.setdefault(n, np.tile(np.array(v, float), (self.N, 1)))
        self.kp = D.get("keyplan") or {}
        sheets = _cached_json("move_sheets.json")
        self.sheet = dict(sheets.get(sheet_key or self.take) or {})
        mv = D.get("move") or {}
        self.sheet.setdefault("contact", mv.get("contact")); self.sheet.setdefault("faf", mv.get("faf", self.N - 1))
        self.c0, self.c1 = self.sheet["contact"]
        self.faf = self.sheet["faf"]
        self.hw = self.sheet.get("hold_window", [self.c0, self.c1])
        self.kick = str(self.sheet.get("strike_limb", "")).startswith("leg")
        side = str(self.sheet.get("strike_side", "l"))[-1]
        self.side = side
        eff = self.sheet.get("effector", "")
        self.eff = f"ball_MainPoint_{side}" if self.kick or eff.startswith("ball") else f"hand_MainPoint_{side}"
        kd = mv.get("kick_dir_cascadeur")
        a = g2(np.array(kd)) if kd else g2(self.P[self.eff][self.c0] - self.P[self.eff][0])
        self.a = unit(a)
        self.TH = _cached_json("motion_thresholds.json")
        idle = {n: np.array(v) for n, v in _cached_json("idle_points.json")["points"].items()}
        ln = lambda p, q: float(np.linalg.norm(idle[p] - idle[q]))
        self.L = {s: {"thigh": ln(f"thigh_MainPoint_{s}", f"calf_MainPoint_{s}"), "shin": ln(f"calf_MainPoint_{s}", f"foot_MainPoint_{s}"),
                      "ua": ln(f"upperarm_MainPoint_{s}", f"lowerarm_MainPoint_{s}"), "fa": ln(f"lowerarm_MainPoint_{s}", f"hand_MainPoint_{s}")}
                  for s in "lr"}
        # the floor: the idle's ball and ankle heights (the move starts from the idle; frame 0 may already lift a heel or toe)
        self.floor = {s: (float(idle[f"ball_MainPoint_{s}"][1]), float(idle[f"foot_MainPoint_{s}"][1])) for s in "lr"}
        self.flags = []; self.info = {}; self.notes = []

    def p(self, n, f):
        return self.P[n][f]

    def flag(self, sec, check, **kw):
        self.flags.append({"section": sec, "check": check, **kw})

    # ---------------------------------------------------------------- helpers
    def com(self, f, upper_only=False):
        P = lambda n: self.P[n][f]
        seg = {"head": P("head_MainPoint") + np.array([0, 9.0, 0]), "trunk": 0.45 * P("pelvis_MainPoint") + 0.55 * P("spine_04_MainPoint")}
        for s in "lr":
            seg[f"ua_{s}"] = 0.5 * (P(f"upperarm_MainPoint_{s}") + P(f"lowerarm_MainPoint_{s}"))
            seg[f"fa_{s}"] = 0.5 * (P(f"lowerarm_MainPoint_{s}") + P(f"hand_MainPoint_{s}"))
            seg[f"hd_{s}"] = P(f"hand_MainPoint_{s}")
            seg[f"th_{s}"] = 0.57 * P(f"thigh_MainPoint_{s}") + 0.43 * P(f"calf_MainPoint_{s}")
            seg[f"sh_{s}"] = 0.57 * P(f"calf_MainPoint_{s}") + 0.43 * P(f"foot_MainPoint_{s}")
            seg[f"ft_{s}"] = 0.5 * (P(f"foot_MainPoint_{s}") + P(f"ball_MainPoint_{s}"))
        m = [(n, w) for n, w in MASS if not upper_only or n in UPPER]
        return sum(w * seg[n] for n, w in m) / sum(w for _, w in m)

    def bend(self, s, f, limb):
        """Bend (deg, 0 = straight) from the root-end distance and the bone lengths; reach error (cm, > 0 = cannot reach);
        and the bend measured on the middle point itself."""
        r, m, e, l1, l2 = ((f"thigh_MainPoint_{s}", f"calf_MainPoint_{s}", f"foot_MainPoint_{s}", self.L[s]["thigh"], self.L[s]["shin"]) if limb == "leg"
                           else (f"upperarm_MainPoint_{s}", f"lowerarm_MainPoint_{s}", f"hand_MainPoint_{s}", self.L[s]["ua"], self.L[s]["fa"]))
        R, M, E = self.p(r, f), self.p(m, f), self.p(e, f)
        d = float(np.linalg.norm(E - R))
        c = np.clip((l1 * l1 + l2 * l2 - d * d) / (2 * l1 * l2), -1, 1)
        b_len = 180.0 - math.degrees(math.acos(c))
        b_pt = 180.0 - ang(R - M, E - M)
        seg_err = max(abs(np.linalg.norm(M - R) - l1), abs(np.linalg.norm(E - M) - l2))
        return round(b_len, 1), round(d - (l1 + l2), 1), round(b_pt, 1), round(float(seg_err), 1)

    def planted(self, s, f):
        b0, a0 = self.floor[s]
        # on the floor = the ball or the toe tip (ball_DirectionPoint) within PLANT_TOL of the idle's ball height
        on = min(self.P[f"ball_MainPoint_{s}"][f][1], self.P[f"ball_DirectionPoint_{s}"][f][1]) <= b0 + PLANT_TOL
        return on, on and self.P[f"foot_MainPoint_{s}"][f][1] <= a0 + PLANT_TOL

    def foot_quad(self, s, f, toes_only):
        an, ba = g2(self.p(f"foot_MainPoint_{s}", f)), g2(self.p(f"ball_MainPoint_{s}", f))
        d = ba - an; L_ = np.linalg.norm(d); a = unit(d); n = np.array([-a[1], a[0]])
        heel = (ba - a * 0.2 * L_) if toes_only else (an - a * 0.45 * L_); toe = ba + a * 0.35 * L_
        return [heel + n * FOOT_HALF_W, heel - n * FOOT_HALF_W, toe - n * FOOT_HALF_W, toe + n * FOOT_HALF_W]

    def along(self, n, f):
        return float(np.dot(g2(self.p(n, f)), self.a))

    def anticipation_key(self, n):
        return min(range(0, self.c0), key=lambda f: self.along(n, f)) if self.c0 > 0 else 0

    def speed_peak(self, n, a, b):
        X = self.P[n][a:b + 1]
        v = np.linalg.norm(np.diff(X, axis=0), axis=1)
        i = int(np.argmax(v)); off = 0.0
        if i in (0, len(v) - 1):
            return None                                  # the peak lies outside the window: no peak inside it
        y0, y1, y2 = v[i - 1], v[i], v[i + 1]; den = y0 - 2 * y1 + y2
        off = 0.5 * (y0 - y2) / den if abs(den) > 1e-9 else 0.0
        return round(a + i + 0.5 + off, 2)

    # ---------------------------------------------------------------- 1. limbs
    def limbs(self):
        TH = self.TH; rows = {}; reach, pts = [], []
        for s in "lr":
            for limb in ("leg", "arm"):
                R = [self.bend(s, f, limb) for f in range(self.N)]
                rows[f"{limb}_{s}"] = [r[0] for r in R]
                for f, (b, re, bp, se) in enumerate(R):
                    if re > 0.5:
                        reach.append((f"{limb}_{s}", f, re))
                    if se > 1.5:
                        pts.append((f"{limb}_{s}", f, se, b, bp))
        if reach:
            self.flag(1, "limb cannot reach (IK will stretch or pull the root)", worst=max(reach, key=lambda x: x[2]), frames=sorted({x[1] for x in reach}))
        if pts:
            w = max(pts, key=lambda x: x[2])
            self.flag(1, "knee/elbow point off its bone lengths (the rig will move it)", worst={"limb": w[0], "frame": w[1], "cm": w[2], "bend_from_lengths": w[3], "bend_at_point": w[4]},
                      count=len(pts))
        # elbow direction (arm_check rules), torso frame from the shoulders and the pelvis -> neck line
        bad = []
        win = set(range(self.c0, self.c1 + 1 + TH.get("joint_action_window_frames", 3)))
        for f in range(self.N):
            up_t = unit(self.p("neck_01_MainPoint", f) - self.p("pelvis_MainPoint", f))
            left = self.p("upperarm_MainPoint_l", f) - self.p("upperarm_MainPoint_r", f); left = unit(left - np.dot(left, up_t) * up_t)
            for s in "lr":
                sh, el, wr = (self.p(f"{j}_MainPoint_{s}", f) for j in ("upperarm", "lowerarm", "hand"))
                line = wr - sh; t = float(np.dot(el - sh, line) / max(np.dot(line, line), 1e-9))
                pole = unit(el - (sh + t * line)); flex = 180.0 - ang(sh - el, wr - el)
                elev = ang(el - sh, np.array([0, -1.0, 0])); out_ = float(np.dot(pole, left if s == "l" else -left))
                if flex <= TH["elbow_min_flex_for_pole_deg"]:
                    continue
                iss = []
                ua = unit(el - sh); fwd = np.cross(left, up_t)
                swept = float(-np.dot(ua, fwd)) > max(0.0, abs(float(np.dot(ua, left))))   # swept back: own limit (arm_check)
                if not swept and pole[1] > TH["elbow_pole_up_max"] and elev >= TH.get("elbow_pole_min_elevation_deg", 40):
                    iss.append("elbow pointing up/sideways")
                if out_ > TH["elbow_pole_out_max"]:
                    iss.append("elbow flared out")
                if not swept and elev > TH["elbow_upperarm_elev_max_deg"]:
                    iss.append("upper arm raised (chicken wing)")
                if swept and elev > TH.get("elbow_ext_elev_max_deg", 65):
                    iss.append("upper arm swept back past the shoulder's range")
                striking_arm = not self.kick and s == self.side
                if iss and not (striking_arm and f in win and elev > 60 and flex < 60):
                    bad.append((s, f, iss, round(out_, 2), round(float(pole[1]), 2), round(elev)))
        if bad:
            for s in "lr":
                b = [x for x in bad if x[0] == s]
                if b:
                    kinds = sorted({i for x in b for i in x[2]})
                    self.flag(1, f"elbow direction ({s}): " + ", ".join(kinds), frames=[x[1] for x in b],
                              example={"frame": b[len(b) // 2][1], "pole_out": b[len(b) // 2][3], "pole_up": b[len(b) // 2][4], "elevation": b[len(b) // 2][5]})
        self.rows_bend = rows
        self.info["knee_bend_range"] = {k: [min(v), max(v)] for k, v in rows.items() if k.startswith("leg")}

    # ---------------------------------------------------------------- 2. balance
    def balance(self):
        TH = self.TH; exempt = set(self.sheet.get("balance_exempt_frames", [])); tol = TH.get("balance_outside_max_cm", 2.0)
        rows = []
        for f in range(self.N):
            pl = {s: self.planted(s, f) for s in "lr"}
            feet = [s for s in "lr" if pl[s][0]]
            c = g2(self.com(f))
            if not feet:
                rows.append({"frame": f, "planted": [], "outside_cm": None}); continue
            poly = hull([q for s in feet for q in self.foot_quad(s, f, toes_only=not pl[s][1])])
            rows.append({"frame": f, "planted": feet, "on_toes": [s for s in feet if not pl[s][1]], "outside_cm": round(outside(poly, c), 1)})
        self.brows = rows
        bad = [r for r in rows if r["outside_cm"] is not None and r["outside_cm"] > tol and r["frame"] not in exempt]
        if bad:
            w = max(bad, key=lambda r: r["outside_cm"])
            self.flag(2, "off balance (centre of mass outside the feet)", frames=[r["frame"] for r in bad], worst_cm=w["outside_cm"], worst_frame=w["frame"])
        air = [r["frame"] for r in rows if not r["planted"] and r["frame"] not in exempt]
        if air:
            self.flag(2, "no foot on the ground (undeclared airborne frames)", frames=air)
        # lift-off: the first single-support frame must already be over the support foot
        single = [r for r in rows if len(r["planted"]) == 1]
        if single:
            fs = single[0]; s = fs["planted"][0]
            over = next((r["frame"] for r in rows if r["outside_cm"] is not None and
                         outside(hull(self.foot_quad(s, r["frame"], toes_only=False)), g2(self.com(r["frame"]))) <= tol), None)
            self.info["liftoff"] = {"frame": fs["frame"], "support": s, "outside_cm": fs["outside_cm"], "weight_over_support_from": over}
            if fs["outside_cm"] > tol and fs["frame"] not in exempt:
                self.flag(2, "weight not shifted before lift-off", frame=fs["frame"], outside_cm=fs["outside_cm"], weight_over_support_from=over)
        # support knee on one foot
        lo, hi = TH.get("support_knee_min_deg", 10), TH.get("joint_support_knee_max_flex_deg", 45)
        if not self.sheet.get("crouch"):
            bk = [(r["frame"], self.rows_bend["leg_" + r["planted"][0]][r["frame"]]) for r in single]
            bk = [(f, k) for f, k in bk if not lo <= k <= hi]
            if bk:
                self.flag(2, "support knee out of range on one foot", range_deg=[lo, hi], frames=[f for f, _ in bk],
                          min_max=[min(k for _, k in bk), max(k for _, k in bk)])
        # kicks: hip drive and counterbalance from the anticipation key (pelvis furthest back) to contact
        if self.kick:
            k0 = self.anticipation_key("pelvis_MainPoint")
            drive = self.along("pelvis_MainPoint", self.c0) - self.along("pelvis_MainPoint", k0)
            u = lambda f: float(np.dot(g2(self.com(f, upper_only=True) - self.p("pelvis_MainPoint", f)), self.a))
            cb = u(self.c0) - u(k0)
            self.info["kick_weight"] = {"anticipation_key": k0, "hip_drive_cm": round(drive, 1), "upper_vs_hips_cm": round(cb, 1)}
            if drive < TH.get("com_shift_min_cm", 8):
                self.flag(2, "hips do not drive into the kick", hip_drive_cm=round(drive, 1), min_cm=TH.get("com_shift_min_cm", 8))
            if cb > -TH.get("counterbalance_min_cm", 2.0):
                self.flag(2, "no counterbalance (upper body does not lean back against the kick)", upper_vs_hips_cm=round(cb, 1))

    # ---------------------------------------------------------------- 3. key plan
    def keyplan(self):
        kp = self.kp; keys = sorted(kp.get("keys") or []); N = self.N
        if not keys:
            self.info["keyplan"] = "none in the targets (older format): not checked"; return
        layers = kp.get("layers") or {}
        plan = {}
        for nm, v in layers.items():
            chains = next((c for pat, c in CHAIN_OF if pat in nm.lower()), None)
            plan[nm] = {"keys": sorted(v.get("keys", keys)), "lag": v.get("lag", 0), "chains": chains}
            if chains is None:
                self.flag(3, "layer not mapped to an IK chain (cannot check its partners)", layer=nm)
        for ch in ("leg_l", "leg_r", "arm_l", "arm_r"):
            mem = {nm: x for nm, x in plan.items() if x["chains"] and ch in x["chains"]}
            sets = {tuple(x["keys"]) for x in mem.values()} | ({tuple(keys)} if len(mem) < 3 else set())   # partners not listed use the global keys
            lags = {x["lag"] for x in mem.values()} | ({0} if len(mem) < 3 else set())
            if len(sets) > 1:
                diff = sorted(set().union(*map(set, sets)) - set.intersection(*map(set, sets)))
                self.flag(3, f"IK chain {ch}: layers keyed on different frames (the joint between them zig-zags)", layers=sorted(mem), frames_not_shared=diff)
            if len(lags) > 1:
                self.flag(3, f"IK chain {ch}: layers with different lags (the joint between them drifts)", lags={nm: x["lag"] for nm, x in mem.items()})
        need = {self.c0 - 1: "snap start", self.c0: "contact", self.faf: "FAF"}
        miss = {f: w for f, w in need.items() if 0 <= f < N and f not in keys}
        if miss:
            self.flag(3, "beat frames not keyed", missing=miss)
        if keys[0] != 0 or keys[-1] != N - 1:
            self.flag(3, "first/last frame not keyed", first=keys[0], last=keys[-1], n=N)
        lin = {tuple(x) for x in kp.get("linear", [])}; bez = {tuple(x) for x in kp.get("bezier", [])}
        intervals = list(zip(keys[:-1], keys[1:]))
        clamped = [iv for iv in intervals if iv not in lin and iv not in bez]
        self.info["keyplan"] = {"keys": len(keys), "linear": len(lin), "bezier": len(bez), "clamped": len(clamped),
                                "dense": len(keys) == N}
        names = [n for n in self.P if n.endswith(("MainPoint", "MainPoint_l", "MainPoint_r"))]
        # Bezier: Catmull-Rom tangents through the keys (non-uniform), evaluated on the integer frames between them
        over, drift = [], []
        for n in names:
            X = self.P[n]
            for i, (a, b) in enumerate(intervals):
                if (a, b) not in bez or b - a < 2:
                    continue
                ka = keys[i - 1] if i > 0 else a; kb = keys[i + 2] if i + 2 < len(keys) else b
                ma = (X[b] - X[ka]) / max(b - ka, 1); mb = (X[kb] - X[a]) / max(kb - a, 1)
                for f in range(a + 1, b):
                    t = (f - a) / (b - a); h = b - a
                    y = ((2 * t ** 3 - 3 * t ** 2 + 1) * X[a] + (t ** 3 - 2 * t ** 2 + t) * h * ma + (-2 * t ** 3 + 3 * t ** 2) * X[b] + (t ** 3 - t ** 2) * h * mb)
                    lo_, hi_ = np.minimum(X[a], X[b]), np.maximum(X[a], X[b])
                    # overshoot only where the key values run monotone through the neighbours (no extremum intended)
                    mono = np.sign(X[a] - X[ka]) * np.sign(X[b] - X[a]) >= 0
                    mono &= np.sign(X[b] - X[a]) * np.sign(X[kb] - X[b]) >= 0
                    o = np.where(mono, np.maximum(y - hi_, lo_ - y), 0).max()
                    if o > 0.5:
                        over.append((n, f, round(float(o), 1)))
                    dv = float(np.linalg.norm(y - X[f]))
                    if dv > 2.0:
                        drift.append((n, f, round(dv, 1)))
        if over:
            self.flag(3, "Bezier overshoot past the key values", worst=max(over, key=lambda x: x[2]), count=len(over))
        if drift:
            self.flag(3, "Bezier curve drifts from the model's curve (> 2 cm)", worst=max(drift, key=lambda x: x[2]), count=len(drift))
        # clamped: Cascadeur flattens every axis at the key, so a key in mid-motion stops the path there
        stops = []
        cl_keys = {k for iv in clamped for k in iv} - {0, N - 1}
        for k in sorted(cl_keys):
            if (k - 1, k) in lin and (k, k + 1) in lin:
                continue
            for n in names:
                v_in = self.P[n][k] - self.P[n][k - 1]; v_out = self.P[n][k + 1] - self.P[n][k]
                sp = min(np.linalg.norm(v_in), np.linalg.norm(v_out))
                if sp > 1.0 and np.dot(v_in, v_out) > 0.5 * np.linalg.norm(v_in) * np.linalg.norm(v_out):
                    stops.append((n, k, round(float(sp), 1)))
        if stops:
            self.flag(3, "clamped key in mid-motion (the path stops at the key)", keys=sorted({x[1] for x in stops}), worst=max(stops, key=lambda x: x[2]))

    # ---------------------------------------------------------------- 4. spacing
    def spacing(self):
        e = self.eff; k0 = self.anticipation_key(e); c0 = self.c0
        X = self.P[e]
        d = [float(np.linalg.norm(X[t + 1] - X[t])) for t in range(k0, c0)]
        tot = sum(d) or 1e-9; cum = np.cumsum(d) / tot
        last2 = sum(d[-2:]) / tot; peak = k0 + 1 + int(np.argmax(d)); even = max(d) / (tot / len(d)) if d else 0
        self.info["spacing"] = {"effector": e, "anticipation_key": k0, "share_last2": round(last2, 2), "peak_into_frame": peak,
                                "peak_over_mean": round(even, 2),
                                "table": [{"frame": k0 + 1 + i, "step_cm": round(x, 1), "cum_pct": round(100 * c)} for i, (x, c) in enumerate(zip(d, cum))]}
        if last2 < self.TH.get("impact_share_last2_min", 0.4):
            self.flag(4, "impact spacing: too little of the path in the last 2 frames", share_last2=round(last2, 2))
        if peak != c0:
            self.flag(4, "speed peak not into contact", peak_into_frame=peak, contact=c0)
        if len(d) >= 3 and even < 1.5:
            self.flag(4, "spacing too even (no snap)", peak_over_mean=round(even, 2))
        # never linear (owner's principles list, 31/10: "use animation curves, never linear for character motion"): a part
        # moving >= 1.5 cm/frame at constant speed (steps within 10% of each other) for 4 frames running, outside the snap
        lin = []
        snap = set(range(c0 - 1, self.c1 + 1))
        for n in ["pelvis_MainPoint", "spine_04_MainPoint", "head_MainPoint"] + [f"{j}_MainPoint_{s}" for s in "lr" for j in ("hand", "lowerarm", "foot", "ball", "calf")]:
            st = np.linalg.norm(np.diff(self.P[n], axis=0), axis=1)
            for a in range(0, self.N - 4):
                w = st[a:a + 4]
                if not (set(range(a, a + 5)) & snap) and w.mean() >= 1.5 and (w.max() - w.min()) / w.mean() < 0.10:
                    lin.append(f"{n}@{a}-{a + 4} ({w.mean():.1f} cm/f)"); break
        if lin:
            self.flag(4, "linear motion (constant speed over 4 frames)", parts=lin)
        # landing recovery (kicks/jumps: "landing recovery extends past the impact"): once the lifted foot is back down, the
        # pelvis absorbs the weight (dips >= 1 cm within 4 frames) before it rises
        if self.kick and getattr(self, "brows", None):
            land = next((r["frame"] for r in self.brows[self.c1:] if len(r["planted"]) == 2), None)
            if land is not None:
                y = self.P["pelvis_MainPoint"][:, 1]
                dip = float(y[land] - y[land:min(land + 5, self.N)].min())
                self.info["landing"] = {"frame": land, "pelvis_dip_cm": round(dip, 1)}
                if dip < 1.0:
                    self.flag(4, "no landing absorption (pelvis does not dip after the foot lands)", landing_frame=land, pelvis_dip_cm=round(dip, 1))
        # recovery: fast out of the hold, slow in to the guard/idle (spacing_rules.py, shared with motion_check)
        import spacing_rules
        rec, fl = spacing_rules.recovery_spacing(X, self.hw[1], self.faf)
        self.info["recovery_spacing"] = rec
        for f in fl:
            self.flag(4, f.pop("check"), **f)

    # ---------------------------------------------------------------- 5. overlap
    def overlap(self):
        TH = self.TH; c0 = self.c0; N = self.N
        res = {}
        if self.kick:
            s = self.side
            ch = ["pelvis_MainPoint", f"thigh_MainPoint_{s}", f"calf_MainPoint_{s}", f"foot_MainPoint_{s}"]
            pk = [self.speed_peak(n, max(c0 - 2, 0), min(c0 + 1, N - 1)) for n in ch]
            res["kick_chain"] = dict(zip(ch, pk))
            if any(pk[i] is not None and pk[i + 1] is not None and pk[i + 1] < pk[i] - 0.25 for i in range(len(pk) - 1)):
                self.flag(5, "kinetic chain out of order (pelvis -> hip -> knee -> ankle)", peaks=res["kick_chain"])
        body = ["pelvis_MainPoint", "spine_04_MainPoint", "head_MainPoint"]
        k0 = self.anticipation_key(self.eff)
        phases = {"drive": (max(k0 - 1, 0), min(c0 + 2, N - 1)), "recovery": (self.hw[1], self.faf)}
        for ph, (a, b) in phases.items():
            if b - a < TH.get("overlap_min_phase_frames", 3):
                continue
            pk = [self.speed_peak(n, a, b) for n in body]
            res[f"body_{ph}"] = dict(zip(body, pk))
            if None in pk:
                continue
            if pk[1] < pk[0] - 0.25 or pk[2] < pk[1] - 0.25:
                self.flag(5, f"{ph}: body peaks out of order (pelvis -> chest -> head)", peaks=res[f"body_{ph}"])
            elif max(pk) - min(pk) < TH.get("overlap_min_spread_frames", 1.0) * 0.5:
                self.flag(5, f"{ph}: pelvis, chest and head peak together (no overlap)", peaks=res[f"body_{ph}"])
        # arrivals into the settle: first frame from which each part stays within 1 cm of its FAF pose
        parts = body + ["hand_MainPoint_l", "hand_MainPoint_r", f"foot_MainPoint_{self.side}"]
        arr = {}
        for n in parts:
            X = self.P[n]; dist = np.linalg.norm(X[self.hw[1]:self.faf + 1] - X[self.faf], axis=1)
            if dist.max() < 3.0:
                continue                                    # part barely moves in the recovery
            far = np.nonzero(dist >= 1.0)[0]
            arr[n] = self.hw[1] + (int(far[-1]) + 1 if len(far) else 0)
        res["settle_arrivals"] = arr
        if len(arr) >= 3:
            from collections import Counter
            same = [(f, c) for f, c in Counter(arr.values()).items() if c >= 4]     # 4 of the 6 parts on one frame
            if same:
                self.flag(5, "simultaneous arrivals into the settle", frame=same[0][0], parts=[n for n, f in arr.items() if f == same[0][0]])
        self.info["overlap"] = res

    # ---------------------------------------------------------------- 6. arcs
    def arcs(self):
        TH = self.TH; N = self.N; c0, c1 = self.c0, self.c1
        k0 = self.anticipation_key(self.eff)
        lift = (self.info.get("liftoff") or {}).get("frame")
        land = next((r["frame"] for r in self.brows[c1:] if len(r["planted"]) == 2), None)
        snap = set(range(c0 - 1, c1 + 2))
        # beats as motion_check: contact, the declared hold, the measured hold (effector >= 60% extended) + 2, FAF, the
        # wind-up reversal when the move has one; plus the snap, lift-off and landing frames here
        ext = lambda f: self.along(self.eff, f) - self.along(self.eff, k0)
        hold_end = next((f for f in range(c0, N) if ext(f) < TH.get("hold_min_extension", 0.6) * ext(c0)), N) - 1
        beats = set(range(c0 - 1, c1 + 2)) | set(range(self.hw[0], self.hw[1] + 2)) | set(range(c0, hold_end + 3)) | {self.faf, self.faf + 1}
        if 0 < k0 < c0 - 1:
            beats |= set(range(max(k0 - 2, 0), min(k0 + 3, c0)))
        leg_beats = set(beats)
        for x in (lift, land):
            if x is not None:
                leg_beats |= set(range(x - 2, x + 3))            # lift-off and landing: beats for the legs only
        body_beats = beats
        pts = ["pelvis_MainPoint", "spine_04_MainPoint", "head_MainPoint"] + [f"{j}_MainPoint_{s}" for s in "lr" for j in ("lowerarm", "hand", "calf", "foot", "ball")]
        rev, zz, straight, jerk, pops = [], [], [], [], []
        ends = ("head_", "hand_", "lowerarm_", "foot_", "ball_", "calf_")     # arcs are judged on the extremities
        vmin = TH.get("jitter_min_speed_cm_f", 0.3); rdeg = TH.get("jitter_reversal_deg", 120)
        for n in pts:
            X = self.P[n]; V = np.diff(X, axis=0)
            beats = leg_beats if n.startswith(("calf_", "foot_", "ball_")) else body_beats
            for t in range(1, N - 1):
                v1, v2 = V[t - 1], V[t]
                if min(np.linalg.norm(v1), np.linalg.norm(v2)) > vmin and ang(v1, v2) > rdeg and t not in beats:
                    rev.append((n, t))
            # turning zig-zag: the turn direction flips back and forth inside a 7-frame window at speed
            w = TH.get("arc_window", 7)
            for a in range(0, N - w):
                if set(range(a, a + w)) & beats:
                    continue
                vs = V[a:a + w - 1]
                if np.linalg.norm(vs, axis=1).min() < 1.0:
                    continue
                cr = [np.cross(vs[i], vs[i + 1]) for i in range(len(vs) - 1)]
                nrm = unit(sum(cr))
                sg = [np.sign(np.dot(c_, nrm)) for c_ in cr if np.linalg.norm(c_) > 0.05 * np.linalg.norm(vs).max() ** 2 / len(vs)]
                flips = sum(1 for i in range(len(sg) - 1) if sg[i] != sg[i + 1])
                if flips >= TH.get("zigzag_min_flips", 3):
                    zz.append((n, a)); break
            # jerk spike (motion_check rule): third difference over 3 cm and 4 x the part's median
            j = np.linalg.norm(X[3:] - 3 * X[2:-1] + 3 * X[1:-2] - X[:-3], axis=1); med = float(np.median(j))
            for i, val in enumerate(j):
                t = i + 1
                if t not in beats and t + 1 not in beats and val > TH.get("jerk_abs_cm", 3.0) and val > TH.get("jerk_rel", 4.0) * max(med, 0.1):
                    jerk.append((n, t, round(float(val), 1)))
            # one-frame pop (outside the snap): a single step > 15 cm and > 3 x both neighbouring steps
            st = np.linalg.norm(V, axis=1)
            for t in range(1, N - 2):
                if t + 1 not in snap and st[t] > 15 and st[t] > 3 * max(st[t - 1], st[t + 1], 0.5):
                    pops.append((n, f"{t}->{t + 1}", round(float(st[t]), 1)))
            # straight path: a 5-frame window travelling >= 15 cm that bows less than half of a rotation about the root (outside the snap), measured
            # RELATIVE TO THE LIMB'S ROOT (hand/elbow from the shoulder, knee/foot/ball from the hip, head from the pelvis):
            # arcs come from joint rotation; in world space a planting foot or a hand carried by a body shifting its weight
            # travels straight even when the joint swings
            side_ = n[-1] if n.endswith(("_l", "_r")) else None
            root = ("pelvis_MainPoint" if n.startswith("head_") else f"upperarm_MainPoint_{side_}" if n.startswith(("hand_", "lowerarm_"))
                    else f"thigh_MainPoint_{side_}" if side_ else None)
            XR = X - self.P[root] if root else X
            for a in range(0, N - 4 if n.startswith(ends) else 0):
                if set(range(a, a + 5)) & set(range(c0 - 1, c0 + 1)):
                    continue
                if n.startswith(("foot_", "ball_", "calf_")) and any(self.planted(side_, f)[0] for f in range(a, a + 5)):
                    continue                                     # a planted foot only mirrors the hips' path
                seg = XR[a:a + 5]; chord = seg[-1] - seg[0]; L_ = np.linalg.norm(chord)
                if L_ < 15:
                    continue
                u = chord / L_; dev = max(np.linalg.norm((q - seg[0]) - np.dot(q - seg[0], u) * u) for q in seg)
                # a pure rotation about the root at this radius bows by tan(theta/4)/2 of the chord: flag a path flatter
                # than half of that (a fixed 3% would flag even a perfect arc on a small swing)
                r_ = max(float(np.mean(np.linalg.norm(seg, axis=1))), L_ / 2 + 1e-6) if root else 1e9
                th_ = 2 * math.asin(min(L_ / (2 * r_), 1.0))
                if root and dev / L_ < 0.5 * math.tan(th_ / 4) / 2:
                    straight.append((n, a, round(float(L_), 1))); break
        # as motion_check: one reversal is a beat (a spring), two on the same part less than 3 frames apart are a zig-zag
        cl = [(n, t) for n, t in rev if any(m_ == n and 0 < t2 - t < 3 for m_, t2 in rev)]
        if cl:
            self.flag(6, "velocity reversals back and forth (two within 2 frames)", parts=[f"{n}@{t}" for n, t in cl])
        if rev:
            self.notes.append(("single velocity reversals (beats, not flagged)", [f"{n}@{t}" for n, t in rev]))
        if zz:
            self.flag(6, "turning zig-zag (path turns back and forth)", parts=[f"{n}@{a}" for n, a in zz])
        if pops:
            self.flag(6, "one-frame pop outside the snap", parts=pops)
        if jerk:
            self.notes.append(("jerk spikes outside the beats (info, as motion_check)", {"count": len(jerk), "worst": max(jerk, key=lambda x: x[2]),
                                                                                          "frames": sorted({x[1] for x in jerk})}))
        if straight:
            self.flag(6, "straight paths (no arc, relative to the limb's root)", parts=[f"{n}@{a}-{a + 4} ({L_} cm)" for n, a, L_ in straight])
        # twinning at contact and mid-hold
        for f in sorted({c0, (self.hw[0] + self.hw[1]) // 2}):
            ua = lambda s: ang(self.p(f"lowerarm_MainPoint_{s}", f) - self.p(f"upperarm_MainPoint_{s}", f), [0, -1, 0])
            pairs = [(self.bend("l", f, "arm")[2], self.bend("r", f, "arm")[2]), (self.bend("l", f, "leg")[2], self.bend("r", f, "leg")[2]), (ua("l"), ua("r"))]
            if all(abs(x - y) < TH.get("twinning_max_deg", 6) for x, y in pairs):
                self.flag(6, "twinning (left and right limbs mirrored)", frame=f, pairs_deg=[[round(x), round(y)] for x, y in pairs])
        # head toward the target (rotation since frame 0, as pose_rules)
        def yp(f):
            v = self.p("head_AdditionalPoint", f) - self.p("head_MainPoint", f)
            return math.degrees(math.atan2(v[0], v[2])), math.degrees(math.asin(float(np.clip(unit(v)[1], -1, 1))))
        y0, p0 = yp(0); hb = []
        for f in range(c0, self.hw[1] + 1):
            y, p = yp(f); dy = (y - y0 + 180) % 360 - 180; dp = p - p0
            if abs(dy) > TH.get("head_yaw_max_deg", 35) or abs(dp) > TH.get("head_pitch_max_deg", 30):
                hb.append((f, round(dy), round(dp)))
        if hb:
            self.flag(6, "head not toward the target", frames=[x[0] for x in hb], example_yaw_pitch=hb[0][1:])
        # planted-foot slide (ball as pivot, within 1 cm of its frame-0 height)
        sl = []
        for s in "lr":
            z0 = self.floor[s][0]; run = []
            for f in range(N + 1):
                ok = f < N and self.planted(s, f)[0] and self.P[f"ball_MainPoint_{s}"][f][1] <= z0 + 1.0
                if ok:
                    run.append(g2(self.p(f"ball_MainPoint_{s}", f)))
                else:
                    if len(run) > 1:
                        d = max(float(np.linalg.norm(q - run[0])) for q in run)
                        if d > TH.get("foot_slide_max_cm", 0.5):
                            sl.append((s, round(d, 2)))
                    run = []
        if sl:
            self.flag(6, "planted foot slides", feet_cm=sl)

    def run(self):
        self.limbs(); self.balance(); self.keyplan(); self.spacing(); self.overlap(); self.arcs()
        return self


SECTIONS = {1: "limbs", 2: "balance", 3: "key plan", 4: "spacing", 5: "overlap", 6: "arcs"}

def report(m, quiet=False):
    out = []
    for k, nm in SECTIONS.items():
        fl = [f for f in m.flags if f["section"] == k]
        out.append(f"{k} {nm:9s} {'PASS' if not fl else 'FLAG ' + str(len(fl))}")
        for f in fl:
            extra = {x: y for x, y in f.items() if x not in ("section", "check")}
            s = json.dumps(extra, default=str)
            out.append(f"    - {f['check']}: {s[:220]}")
    for k, v in m.notes:
        out.append(f"  note: {k}: {json.dumps(v, default=str)[:200]}")
    if not quiet:
        i = m.info
        out.append(f"  knee bend (from lengths) {i.get('knee_bend_range')}")
        out.append(f"  lift-off {i.get('liftoff')}  kick weight {i.get('kick_weight')}")
        sp = i["spacing"]
        out.append(f"  spacing {sp['effector']} from key {sp['anticipation_key']}: last2 {round(100 * sp['share_last2'])}%, peak into {sp['peak_into_frame']}, "
                   f"peak/mean {sp['peak_over_mean']}")
        out.append("    " + "  ".join(f"f{r['frame']}:{r['step_cm']}cm/{r['cum_pct']}%" for r in sp["table"]))
        rc = i.get("recovery_spacing") or {}
        out.append(f"  recovery {rc.get('from')} -> arrival {rc.get('arrival')}: peak {rc.get('peak_frame')} ({rc.get('peak_share')} of the way), "
                   f"approach {rc.get('approach_steps')} (last/max {rc.get('ease_last_over_max')})")
        out.append("    " + "  ".join(f"f{r['frame']}:{r['step_cm']}" for r in rc.get("steps", [])))
        out.append(f"  overlap {json.dumps(i.get('overlap'))[:400]}")
        out.append(f"  key plan {i.get('keyplan')}")
    return "\n".join(out)

if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    m = Move(args[0], args[1] if len(args) > 1 else None).run()
    dt = time.time() - T0
    print(f"pre-flight {m.take} ({m.N} frames, contact {m.c0}-{m.c1}, FAF {m.faf}): {len(m.flags)} flag(s), {dt:.2f} s")
    print(report(m, "--quiet" in sys.argv))
    json.dump({"take": m.take, "flags": m.flags, "notes": m.notes, "info": m.info, "seconds": round(dt, 2)},
              open(os.path.join(HERE, "sheets", f"{m.take}_preflight.json"), "w"), indent=1, default=str)
    sys.exit(1 if m.flags else 0)
