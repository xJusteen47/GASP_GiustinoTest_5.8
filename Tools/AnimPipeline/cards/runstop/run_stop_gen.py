"""Run stop v2 (owner 2026-11-19, answer "A": a pose-based clip like Ganondorf's run stop in Smash). Brief: project file
handoffs/run-start-stop-skid-brief.md section 2. Identity traits: 1) front foot planted, facing forward, no turn; 2) the torso rises
AFTER the feet stop, arms open low for balance; 3) soft parts last (no cape: head and arms land 3-4 frames after the torso).

Cascadeur was not available (access declined 2026-11-19), so the poses are authored here as numbers and solved by the pipeline's
IK poser (animpipe/author.py Poser): pelvis, trunk, head, hand targets in the chest frame, ankle targets in component space.

    python run_stop_gen.py [L|R] [--plot]     (from Tools/AnimPipeline)

Writes run_stop_<side>_bones.json (component space, root at the origin = the character's frame, as the game shows it with root
lock) and run_stop_<side>_root.json (root track: forward Y per frame, matching the sim's brake speed so the speed-matched clip
time runs at real time). Prints at most 5 lines.

Axes: Unreal component space, X = character's left, Y = forward, Z = up. Rotation vectors in degrees; -rx = lean forward.
"""
import json, math, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, TOOLS)
from animpipe.author import Poser, rv, m2q   # noqa: E402

FPS = 60
N = 61                       # frames 0..60 (an even number of intervals: the 30 fps resample needs it)
# Root speed per frame, cm/s: the sim's brake measured in run-stop-1 (Saved/RunStop/clip/run.log.ue.log, player, entry 470 cm/s,
# one value per presented frame from the first stop frame). Frames 0-2 = the run speed (the track's run speed and brake start).
# The tail below 10 cm/s is cut to 0: the game starts the settle at StoppedSpeed (10 cm/s) and jumps the clip to its stop, so the
# clip's stop must sit on the same frame (no skipped frames at the start of the rise).
SIM = [460, 451, 440, 426, 410, 393, 374, 353, 333, 312, 290, 267, 244, 221, 198, 174, 151, 128, 106, 86, 70, 56, 44, 35, 27, 21,
       16, 13, 10]
SPEED = [470.0] * 3 + SIM + [0.0] * (N - 3 - len(SIM))
STOP = 31                    # feet stopped (the root's last move, sim speed 10, ends on frame 31): the torso starts rising here, never earlier (trait 2)


# --- curves: keys (frame, value, ease of the segment that ENDS on this key) ---
def ease(t, kind):
    if kind == "lin":
        return t
    if kind == "out":                       # fast start, soft landing
        return 1 - (1 - t) ** 3
    if kind == "in":
        return t ** 3
    return t * t * (3 - 2 * t)              # "io"


def curve(keys, f):
    keys = sorted(keys, key=lambda k: k[0])
    if f <= keys[0][0]:
        return np.array(keys[0][1], float)
    for (fa, va, _), (fb, vb, e) in zip(keys, keys[1:]):
        if fa <= f <= fb:
            t = ease((f - fa) / (fb - fa), e)
            return np.array(va, float) + (np.array(vb, float) - np.array(va, float)) * t
    return np.array(keys[-1][1], float)


# --- the idle (GASP M_Neutral_Stand_Idle_Loop frame 0 = the poser's rest) ---
POSER = Poser()
P0, R0 = POSER.P0, POSER.R0
IDLE_ANKLE = {s: P0[f"foot_{s}"].copy() for s in "lr"}
IDLE_HAND = {s: P0[f"hand_{s}"] - P0["spine_05"] for s in "lr"}          # chest frame = component axes at idle
BALL_OFF = {s: P0[f"foot_{s}"] - P0[f"ball_{s}"] for s in "lr"}          # ankle relative to the ball, idle orientation

# ---------------- the action, LEFT foot planted in front (component space, root at the origin) ----------------
# Pelvis offset from the idle pelvis (2.1, -2.5, 90.6): 7 cm down on the plant (brief: 6-8), deeper while it absorbs, over the foot
PELVIS_LOC = [(0, [0, 2.5, -7.0], "lin"), (2, [0, 2.5, -8.0], "lin"), (12, [0, 8.0, -10.5], "io"), (22, [0, 12.0, -9.0], "io"),
              (STOP, [0, 13.0, -7.5], "io"), (STOP + 8, [0, 10.0, -6.5], "io"), (STOP + 18, [0, 4.0, -2.5], "io"),
              (STOP + 27, [0, 0, 0], "io")]   # V2 (Fable r3 FAIL): stays low after the lunge, rises over ~19 frames, no pelvis bump
# Pelvis rotation: forward tilt and the left hip forward (yaw), unwinding with the rise
PELVIS_ROT = [(0, [-8, 0, 12], "lin"), (2, [-8, 0, 12], "lin"), (22, [-8, 0, 10], "io"), (STOP, [-7, 0, 8], "io"),
              (STOP + 18, [1, 0, 3], "io"), (STOP + 27, [0, 0, 0], "io")]   # V2: follows the slower rise
# Total trunk pitch (pelvis + spine), the line the eye reads: run lean on the plant, a follow-through forward while the feet brake,
# still leaning when the feet stop, rise with an ease-out to 4 deg past vertical (backward), settle (single overshoot)
TRUNK_PITCH = [(0, -32, "lin"), (2, -32, "lin"), (8, -37, "out"), (20, -31, "io"), (STOP, -28, "io"), (STOP + 8, -22, "io"),
               (STOP + 18, -4, "io"), (STOP + 25, 2, "out"), (STOP + 29, 0, "io")]   # V2: rise delayed and spread (V1: -28 -> +4 in 9 f)
TRUNK_YAW = [(0, -8, "lin"), (2, -8, "lin"), (STOP, -5, "io"), (STOP + 10, -1, "out"), (STOP + 18, 0, "io")]   # shoulders counter the hips
TRUNK_ROLL = [(0, 0, "lin"), (10, -3, "io"), (STOP, -2, "io"), (STOP + 12, 0, "io")]                         # weight toward the front leg
# Head WORLD pitch: eyes forward through the brake, then it follows the trunk 3 frames late (trait 3)
# v1 capture: -10 read as the chin on the chest in game; eyes level now
HEAD_PITCH = [(0, 2, "lin"), (2, 2, "lin"), (STOP, 3, "io"), (STOP + 3, 3, "io"), (STOP + 20, 8, "out"), (STOP + 28, 0, "io")]

# Front (left) ankle: lands 50 cm ahead, heel first (toes up 14 deg), flat by frame 7, skids while the pelvis comes over it; after the
# stop it steps back under the body (the end pose is the idle, root at rest)
LF_POS = [(0, [16, 42, 11.5], "lin"), (2, [16, 42, 11.5], "lin"), (7, [16.5, 40, 9.4], "io"), (22, [17, 33, 9.4], "io"),
          (STOP, [17, 31, 9.4], "io"), (STOP + 6, [17, 31, 9.4], "lin"), (STOP + 10, [18.5, 26, 15.0], "io"),
          (STOP + 15, [20.0, 12.5, 9.4], "io")]
LF_ROT = [(0, [14, 0, 6], "lin"), (2, [14, 0, 6], "lin"), (7, [0, 0, 6], "io"), (STOP + 6, [0, 0, 6], "lin"),
          (STOP + 10, [-8, 0, 3], "io"), (STOP + 15, [0, 0, 0], "io")]
LF_POLE = [(0, [18, 110, 45], "lin")]
# Back (right) foot: on its toes, dragged behind, then a quick step up beside the body
RB_BALL = [(0, [-12, -50, 3.2], "lin"), (2, [-12, -50, 3.2], "lin"), (10, [-13, -45, 3.2], "io")]
RF_POS = [(14, [-14, -34, 17.0], "io"), (20, [-14.5, -20.4, 9.4], "io")]                 # ankle, after the toe drag
RF_ROT = [(0, [-48, 0, -4], "lin"), (2, [-48, 0, -4], "lin"), (10, [-42, 0, -4], "io"), (14, [-18, 0, -2], "io"), (20, [0, 0, 0], "io")]
RF_POLE = [(0, [-16, 40, 15], "lin"), (20, [-16, 60, 45], "io")]

# Arms (hand targets in the chest frame): run swing on the plant (right arm forward, left back), open LOW to the sides for balance
# (brief frames 4-10), hold through the brake, then a small forward swing and close to the idle 3-4 frames after the trunk (trait 3)
HL = [(0, [24, -24, -42], "lin"), (2, [24, -24, -42], "lin"), (11, [46, 4, -40], "out"), (STOP, [44, 8, -42], "io"),
      (STOP + 20, [31, 20, -44], "out"), (STOP + 29, None, "io")]
HR = [(0, [-20, 28, -28], "lin"), (2, [-20, 28, -28], "lin"), (12, [-43, 13, -37], "out"), (STOP, [-41, 10, -39], "io"),
      (STOP + 19, [-29, 13, -45], "out"), (STOP + 28, None, "io")]
POLE_L = [(0, [36, -45, -12], "lin"), (11, [55, -25, -20], "io"), (STOP + 22, [40, -30, -25], "io")]
POLE_R = [(0, [-34, -15, -45], "lin"), (12, [-55, -25, -20], "io"), (STOP + 22, [-40, -30, -25], "io")]
FINGERS = [(0, 0.45, "lin"), (12, 0.7, "io"), (STOP + 20, 0.45, "io")]


def _fill_idle(keys, idle):
    return [(f, idle if v is None else v, e) for f, v, e in keys]


def foot_R(rot, s):
    """Wanted world orientation of a foot: the idle foot turned by toe pitch (about X) then yaw (about Z)."""
    return rv([0, 0, rot[2]]) @ rv([rot[0], 0, 0]) @ R0[f"foot_{s}"]


def action(f):
    """Pose numbers of frame f for the LEFT-foot clip: dict of plain arrays."""
    tp, ty, tr = curve(TRUNK_PITCH, f), curve(TRUNK_YAW, f), curve(TRUNK_ROLL, f)
    prot = curve(PELVIS_ROT, f)
    trunk = np.array([tp, tr, ty], float)
    d = {"pelvis_loc": curve(PELVIS_LOC, f), "pelvis_rot": prot, "spine": trunk - prot,
         "head": np.array([curve(HEAD_PITCH, f) - tp, -0.5 * tr, -0.6 * ty]),
         "hand_l": curve(_fill_idle(HL, IDLE_HAND["l"]), f), "hand_r": curve(_fill_idle(HR, IDLE_HAND["r"]), f),
         "pole_hl": curve(POLE_L, f), "pole_hr": curve(POLE_R, f), "fingers": float(curve(FINGERS, f)),
         "ankle_l": curve(LF_POS, f), "rot_l": curve(LF_ROT, f), "pole_l": curve(LF_POLE, f),
         "rot_r": curve(RF_ROT, f), "pole_r": curve(RF_POLE, f)}
    if f <= 10:
        ball = curve(RB_BALL, f)
        d["ankle_r"] = ball + foot_R(d["rot_r"], "r") @ R0["foot_r"].T @ BALL_OFF["r"]
    else:
        start = curve(RB_BALL, 10) + foot_R(curve(RF_ROT, 10), "r") @ R0["foot_r"].T @ BALL_OFF["r"]
        d["ankle_r"] = curve([(10, start, "lin")] + RF_POS, f)
    return d


MIRROR_V = np.array([-1.0, 1, 1])        # positions: x -> -x
MIRROR_A = np.array([1.0, -1, -1])       # rotation vectors under the same reflection


def mirrored(d):
    """The same action with the RIGHT foot in front (x -> -x, left <-> right). Feet targets are absolute, so the idle stance is
    not mirrored here: the end of the clip blends to the true idle in solve()."""
    m = {"pelvis_loc": d["pelvis_loc"] * MIRROR_V, "pelvis_rot": d["pelvis_rot"] * MIRROR_A, "spine": d["spine"] * MIRROR_A,
         "head": d["head"] * MIRROR_A, "fingers": d["fingers"]}
    for a, b in (("l", "r"), ("r", "l")):
        m[f"hand_{a}"] = d[f"hand_{b}"] * MIRROR_V; m[f"pole_h{a}"] = d[f"pole_h{b}"] * MIRROR_V
        m[f"ankle_{a}"] = d[f"ankle_{b}"] * MIRROR_V; m[f"pole_{a}"] = d[f"pole_{b}"] * MIRROR_V
        m[f"rot_{a}"] = d[f"rot_{b}"] * MIRROR_A
    return m


def idle_numbers():
    d = {"pelvis_loc": np.zeros(3), "pelvis_rot": np.zeros(3), "spine": np.zeros(3), "head": np.zeros(3), "fingers": 0.45}
    for s, sx in (("l", 1), ("r", -1)):
        d[f"hand_{s}"] = IDLE_HAND[s].copy(); d[f"pole_h{s}"] = np.array([40.0 * sx, -30, -25])
        d[f"ankle_{s}"] = IDLE_ANKLE[s].copy(); d[f"pole_{s}"] = np.array([16.0 * sx, 60, 45]); d[f"rot_{s}"] = np.zeros(3)
    return d


def numbers(f, side):
    d = action(f)
    if side == "R":
        d = mirrored(d)
        # the mirrored stance ends on the mirrored idle: hand over to the true idle once each part is home
        w = ease(min(max((f - (STOP + 8)) / 14.0, 0.0), 1.0), "io")
        idle = idle_numbers()
        d = {k: (v + (idle[k] - v) * w) for k, v in d.items()}
        # The idle stance is staggered (left foot 12.5 cm forward, right 20 cm back), so the mirrored stop cannot end on it with the
        # same steps. As in GASP's own Rfoot stop: the back (left) foot steps up beside and ahead, and after the stop the planted right
        # foot steps back under the body.
        if f > 10:
            back10 = mirrored(action(10))["ankle_l"]
            d["ankle_l"] = curve([(10, back10, "lin"), (14, [14.0, -10.0, 17.0], "io"), (20, IDLE_ANKLE["l"], "io")], f)
            front = mirrored(action(STOP))["ankle_r"]
            d["ankle_r"] = curve([(STOP, front, "lin"), (STOP + 6, front, "lin"), (STOP + 11, [-16.0, 8.0, 17.0], "io"),
                                  (STOP + 17, IDLE_ANKLE["r"], "io")], f) if f >= STOP else mirrored(action(f))["ankle_r"]
    return d


def solve(f, side):
    d = numbers(f, side)
    pv_loc, pv_rot = d["pelvis_loc"], d["pelvis_rot"]
    pel = P0["pelvis"] + pv_loc; Dp = rv(pv_rot)
    pose = {"pelvis": {"loc": pv_loc.tolist(), "rot": pv_rot.tolist()}, "spine": d["spine"].tolist(), "head": d["head"].tolist(),
            "fingers": [d["fingers"], 0, 0]}
    for s in "lr":
        pose[f"arm_{s}"] = {"hand": d[f"hand_{s}"].tolist(), "pole": d[f"pole_h{s}"].tolist()}
        pose[f"leg_{s}"] = {"foot": (Dp.T @ (d[f"ankle_{s}"] - pel)).tolist(), "pole": (Dp.T @ (d[f"pole_{s}"] - pel)).tolist()}
    P, R = POSER.solve(pose)
    for s in "lr":                       # second pass: the foot's world orientation (flat when planted, on the toes behind)
        want = foot_R(d[f"rot_{s}"], s)
        pose[f"leg_{s}"]["foot_rot"] = _rotvec(want @ R[f"foot_{s}"].T)
    return POSER.solve(pose)


def _rotvec(M):
    a = math.acos(max(-1.0, min(1.0, (np.trace(M) - 1) / 2)))
    if a < 1e-9:
        return [0.0, 0.0, 0.0]
    v = np.array([M[2, 1] - M[1, 2], M[0, 2] - M[2, 0], M[1, 0] - M[0, 1]]) / (2 * math.sin(a))
    return (v * math.degrees(a)).tolist()


def root_track():
    p = np.concatenate([[0.0], np.cumsum(np.array(SPEED[:N - 1]) / FPS)])
    return (p - p[-1]).tolist()          # ends at 0 like the GASP stop clips


def main():
    side = (sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] in "LR" else "L")
    frames = {}
    for f in range(N):
        P, R = solve(f, side)
        frames[str(f)] = {b: [P[b].tolist(), m2q(R[b]).tolist()] for b in P}
    name = f"run_stop_{side}"
    json.dump({"source": __file__, "space": "ue_component", "frames": frames}, open(os.path.join(TOOLS, f"{name}_bones.json"), "w"))
    json.dump({"fps": FPS, "root_y": root_track(), "speed": SPEED[:N], "stop": STOP}, open(os.path.join(HERE, f"{name}_root.json"), "w"))
    F = frames
    lf = [F[str(f)]["foot_l" if side == "L" else "foot_r"][0] for f in range(N)]
    print(f"{name}: {N} frames, root travel {-root_track()[0]:.1f} cm, front ankle Y {lf[2][1]:.1f} -> {lf[STOP][1]:.1f} -> {lf[-1][1]:.1f}")


if __name__ == "__main__":
    main()
