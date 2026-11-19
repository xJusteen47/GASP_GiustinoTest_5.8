"""GASP-CURVES (owner 2026-11-19): give our locomotion clips the curves and notifies GASP's equivalent clips carry.
What reads them (ABP_GSTN_Locomotion name table + the editor's AI assistant, docs/notes/gasp-curves.md):
  MoveData_Speed (dynamic play rate), Enable_Warping (stride/orientation warping alpha: ABSENT = 0 = warping off),
  contact_l/r (foot contact history), phase; Foley notifies (footstep sounds).
GASP parity per kind (curves_audit.json): stop = contact_l/r + movedata_speed; start = + enable_warping 1 + phase; pivot (our skid) =
the start set without enable_warping (a skid slides on purpose; assistant's advice). Notifies: Foley Run_L/Run_R on each contact onset.
Everything is derived from the clip's own bones (ball height/speed, root speed), nothing is copied across clips. Idempotent: our
curves and our Foley notifies are removed and rewritten. Env CURVES_CLIPS = "path:kind;path:kind" (kind stop|start|pivot),
CURVES_DRY=1 = report only. Report Saved/Routing/curves_author.txt. Also calibrates the contact rule on GASP clips (CAL list)."""
import math, os
import unreal

OUT = unreal.Paths.project_saved_dir() + "Routing/curves_author.txt"
AL = unreal.AnimationLibrary
DRY = os.environ.get("CURVES_DRY", "") == "1"
FOLEY = {"l": "/Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Run_L.BP_AnimNotify_FoleyEvent_Run_L_C",
         "r": "/Game/Blueprints/AnimNotifies/BP_AnimNotify_FoleyEvent_Run_R.BP_AnimNotify_FoleyEvent_Run_R_C"}
TRACK = "GSTN_Foley"
CAL = ["/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Stop_F_Lfoot",
       "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Start_F_Lfoot"]
H_UP, V_MAX = 4.0, 40.0          # contact: ball within H_UP cm of its lowest point in the clip and slower than V_MAX cm/s
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

def sample(seq):
    opts = unreal.AnimPoseEvaluationOptions()
    n = int(round(seq.get_play_length() * 60)) + 1
    rows = []
    for i in range(n):
        t = min(i / 60.0, seq.get_play_length())
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, opts)
        g = lambda b: unreal.AnimPoseExtensions.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
        rows.append({"t": t, "ball_l": g("ball_l"), "ball_r": g("ball_r"), "root": g("root")})
    return rows

def contacts(rows, s):
    zs = [r["ball_" + s].z for r in rows]
    lo = min(zs)
    out = []
    for i, r in enumerate(rows):
        a, b = rows[max(i - 1, 0)]["ball_" + s], rows[min(i + 1, len(rows) - 1)]["ball_" + s]
        dt = rows[min(i + 1, len(rows) - 1)]["t"] - rows[max(i - 1, 0)]["t"] or 1.0
        v = math.hypot(b.x - a.x, b.y - a.y) / dt
        out.append(1.0 if (r["ball_" + s].z - lo) <= H_UP and v <= V_MAX else 0.0)
    return out

def root_speed(rows):
    sp = []
    for i in range(len(rows)):
        a, b = rows[max(i - 1, 0)]["root"], rows[min(i + 1, len(rows) - 1)]["root"]
        dt = rows[min(i + 1, len(rows) - 1)]["t"] - rows[max(i - 1, 0)]["t"] or 1.0
        sp.append(math.hypot(b.x - a.x, b.y - a.y) / dt)
    return sp

def onsets(c):
    return [i for i in range(len(c)) if c[i] > 0.5 and (i == 0 or c[i - 1] < 0.5)]

def phase(cl, cr, n):
    ev = sorted([(i, 0.0) for i in onsets(cl)] + [(i, 0.5) for i in onsets(cr)])
    if len(ev) < 2:
        return None
    out = []
    for f in range(n):
        if f <= ev[0][0]:
            out.append(ev[0][1]); continue
        if f >= ev[-1][0]:
            out.append(ev[-1][1]); continue
        for (fa, pa), (fb, pb) in zip(ev, ev[1:]):
            if fa <= f <= fb:
                d = (pb - pa) % 1.0 or 0.5
                out.append((pa + d * (f - fa) / max(fb - fa, 1)) % 1.0); break
    return out

def gasp_curve(seq, name, rows):
    try:
        return [AL.get_float_value_at_time(seq, name, r["t"]) for r in rows]
    except Exception as e:
        return None

for path in CAL:
    seq = unreal.load_asset(path)
    if not seq:
        p("cal missing", path); continue
    rows = sample(seq)
    for s in "lr":
        mine, theirs = contacts(rows, s), gasp_curve(seq, "contact_" + s, rows)
        if theirs:
            agree = sum(1 for a, b in zip(mine, theirs) if (a > 0.5) == (b > 0.5)) / len(rows)
            p("cal", path.rsplit("/", 1)[-1], s, "agreement %.0f%%" % (agree * 100))
    gs = gasp_curve(seq, "movedata_speed", rows)
    if gs:
        rs = root_speed(rows)
        p("cal movedata_speed mid: gasp %.0f ours %.0f" % (gs[len(gs) // 2], rs[len(rs) // 2]))

spec = [x.rsplit(":", 1) for x in os.environ.get("CURVES_CLIPS", "").split(";") if x.strip()]
for path, kind in spec:
    seq = unreal.load_asset(path)
    if not seq:
        p("MISSING", path); continue
    rows = sample(seq)
    n = len(rows); times = [r["t"] for r in rows]
    cl, cr, sp = contacts(rows, "l"), contacts(rows, "r"), root_speed(rows)
    curves = {"contact_l": cl, "contact_r": cr, "movedata_speed": sp}
    if kind in ("start", "pivot"):
        ph = phase(cl, cr, n)
        if ph:
            curves["phase"] = ph
    # An in-place clip (no root motion: our frantic run start) gets neither: warping scales the stride by capsule speed / root speed,
    # and a 0 root speed is what an absent movedata_speed reads anyway
    if max(sp) < 50.0:
        del curves["movedata_speed"]
    elif kind == "start":
        curves["enable_warping"] = [1.0] * n
    p(path.rsplit("/", 1)[-1], kind, "frames", n, "onsets L", onsets(cl), "R", onsets(cr), "speed max %.0f" % max(sp), "curves", sorted(curves))
    if DRY:
        continue
    for name, vals in curves.items():
        if AL.does_curve_exist(seq, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
            AL.remove_curve(seq, name, False)
        AL.add_curve(seq, name, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
        AL.add_float_curve_keys(seq, name, times, vals)
    if AL.is_valid_anim_notify_track_name(seq, TRACK):
        AL.remove_anim_notify_track(seq, TRACK)
    AL.add_animation_notify_track(seq, TRACK)
    for s, c in (("l", cl), ("r", cr)):
        cls = unreal.load_class(None, FOLEY[s])
        for i in onsets(c):
            AL.add_animation_notify_event(seq, TRACK, times[i], cls)
    p("  saved", unreal.EditorAssetLibrary.save_asset(path.split(".")[0], only_if_is_dirty=False))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
