"""greybox-capsula 3A (owner 2026-11-24, 3A 4A): blocking clips for the greybox capsule, made from numbers (no rig bake).

Every clip is a mannequin clip (same Skeleton as the GASP driver), keyed with 2-4 key poses on the body chain (pelvis height and lean,
spine, neck, head) plus a light pose of arms and legs so the capsule's hand and foot markers read the action. Unkeyed bones stay in the
reference pose. Poses are authored about the character's axes (forward / right / up measured from the reference pose) and turned into
each bone's local delta, like Tools/GAS/build_hit_additives.py, so the clip does not depend on the bones' local axes.

Two kinds:
  LOCOMOTION  AS_GSTN_Greybox_<State>: replace the placeholder rows of CHT_GSTN_SimpleLoco (the row whose LocoState cell is the state);
              the driver plays them under gstn.Loco.Base 2 and the capsule copies the pose.
  MOVES       AS_GSTN_Greybox_Move_<montage>: one per reference montage found (Overlord moves, hit reactions, roll / dodge), same length
              as the montage, written into ABP_GSTN_GreyboxCapsule's MoveOverrides (montage -> clip). The driver keeps playing the
              reference montage (hitboxes unchanged); the capsule shows the clip at the montage's time.
              Move timing is a guess from the montage length (startup / active / recovery shares); the owner retouches (3A = blocking).
4A: these are placeholders, exempt from the level-2 animation review; the owner looks at them in game.

Dry run unless GSTN_SAVE=1 (Tools/CI/Save-Assets.ps1). Existing clips are rebuilt (they are generated). Report:
Saved/Greybox/make_capsule_clips.txt. MOVE_FILTER="Overlord" (default; owner 2026-11-24: bones only in attacks, hit reactions and rolls are
the rigid capsule swaying) picks montages whose name contains one of them.
Run 2: SequenceMoveOverrides (sequence -> clip) for moves played as dynamic montages: every montage's own sequence, the AnimSequences
matching SEQUENCE_FILTER (AS_GSTN_Greybox_Seq_<name>) and the exact paths in SEQUENCES.
"""
import math
import os
import time

import unreal

DIR = '/Game/GSTN/Greybox/Clips'
CHOOSER = '/Game/GSTN/Greybox/CHT_GSTN_SimpleLoco'
CAPSULE_ABP = '/Game/GSTN/Greybox/ABP_GSTN_GreyboxCapsule'
MANNEQUIN = '/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin'
FPS = 30
OUT = unreal.Paths.project_saved_dir() + 'Greybox/make_capsule_clips.txt'
SAVE = os.environ.get('GSTN_SAVE') == '1'
MOVE_FILTER = [s.strip().lower() for s in os.environ.get('MOVE_FILTER', 'Overlord').split(',') if s.strip()]
# Run 2: clips played as dynamic montages (the Playtest jab) are keyed by the sequence. SEQUENCE_FILTER picks AnimSequences by name under
# the move roots; SEQUENCES adds exact paths (the capsule logs 'GSTN Greybox: no capsule override for montage ... playing <path>').
SEQUENCE_FILTER = [s.strip().lower() for s in os.environ.get('SEQUENCE_FILTER', 'Jab,Punch,Kick,Attack').split(',') if s.strip()]
EXTRA_SEQUENCES = [s.strip() for s in os.environ.get('SEQUENCES', '').split(',') if s.strip()]
# Run 2: share of the 'lean' the pelvis takes (the capsule hangs from it: without it the lower body never tilts)
PELVIS_LEAN_SHARE = float(os.environ.get('PELVIS_LEAN_SHARE', '1.0'))  # run 3: rigid body on the pelvis (setup_greybox_assets RIGID_BODY)

lines = []
errors = []


def p(*a):
    lines.append(' '.join(str(x) for x in a))


def fail(msg):
    errors.append(msg)
    p('ERROR', msg)


# ------------------------------------------------------------------------------------------------------------------- pose language
# A pose is a dict; missing keys are 0. Angles in degrees about the character's axes, cumulative where noted.
#   drop      pelvis down (cm)          fwd     pelvis forward (cm)
#   lean      torso forward (+) / back (-): PELVIS_LEAN_SHARE on the pelvis, the rest spread over spine_01..05 (cumulative at spine_05)
#   tilt      whole body forward (+) / back (-) on the pelvis only (run 2: a dropkick lies flat, a roll tucks over)
#   side      torso bend to its right (+)        twist   chest turns to its right (+)
#   head      extra head pitch forward (+)       head_twist   extra head turn right (+)
#   arm_l/r   upper arm swing forward (+, hand forward)       out_l/r  arm away from the body (+) from hanging
#   elbow_l/r forearm bend (+)
#   leg_l/r   thigh swing forward (+, knee forward)           knee_l/r calf bend (+, foot back)
IDLE = {}
GATHER = {'drop': 8, 'lean': 22, 'head': -8, 'arm_l': -30, 'arm_r': 20, 'elbow_l': 40, 'elbow_r': 60, 'leg_l': 25, 'knee_l': 40, 'leg_r': -15, 'knee_r': 30}
PUSH = {'drop': 3, 'fwd': 6, 'lean': 28, 'head': -10, 'arm_l': 45, 'arm_r': -40, 'elbow_l': 70, 'elbow_r': 50, 'leg_l': -30, 'knee_l': 10, 'leg_r': 50, 'knee_r': 70}
RUN_A = {'drop': 2, 'lean': 16, 'head': -6, 'arm_l': 40, 'arm_r': -35, 'elbow_l': 80, 'elbow_r': 70, 'leg_l': -30, 'knee_l': 25, 'leg_r': 40, 'knee_r': 80, 'twist': -6}
RUN_B = {'drop': 5, 'lean': 16, 'head': -6, 'arm_l': -35, 'arm_r': 40, 'elbow_l': 70, 'elbow_r': 80, 'leg_l': 40, 'knee_l': 80, 'leg_r': -30, 'knee_r': 25, 'twist': 6}
BRAKE = {'drop': 12, 'fwd': -4, 'lean': -18, 'head': 8, 'arm_l': 30, 'arm_r': 35, 'out_l': 20, 'out_r': 20, 'elbow_l': 30, 'elbow_r': 30, 'leg_l': 35, 'knee_l': 20, 'leg_r': -10, 'knee_r': 45}
SKID = {'drop': 18, 'lean': -25, 'side': 10, 'twist': 25, 'head': 10, 'arm_l': 50, 'arm_r': -20, 'out_l': 45, 'out_r': 30, 'elbow_l': 20, 'elbow_r': 40, 'leg_l': 45, 'knee_l': 30, 'leg_r': -20, 'knee_r': 60}
AIR_A = {'drop': -4, 'lean': 5, 'arm_l': 20, 'arm_r': 20, 'out_l': 35, 'out_r': 35, 'elbow_l': 30, 'elbow_r': 30, 'leg_l': 45, 'knee_l': 70, 'leg_r': 30, 'knee_r': 60}
AIR_B = dict(AIR_A, out_l=45, out_r=45, leg_l=35, leg_r=40)
CROUCH = {'drop': 42, 'lean': 25, 'head': -15, 'arm_l': 35, 'arm_r': 35, 'elbow_l': 60, 'elbow_r': 60, 'leg_l': 80, 'knee_l': 120, 'leg_r': 80, 'knee_r': 120}
CROUCH_B = dict(CROUCH, drop=44, lean=27)

# State -> (frames at 30 fps, loop, keys [(frame, pose)])
LOCOMOTION = {
    'Idle': (60, True, [(0, IDLE), (30, {'drop': 1, 'lean': 2, 'head': -2}), (60, IDLE)]),
    'RunStart': (12, False, [(0, IDLE), (3, GATHER), (8, PUSH), (12, RUN_A)]),
    'Run': (20, True, [(0, RUN_A), (10, RUN_B), (20, RUN_A)]),
    'RunStop': (16, False, [(0, RUN_A), (4, BRAKE), (11, dict(BRAKE, drop=6, lean=-6)), (16, IDLE)]),
    'Skid': (24, False, [(0, RUN_A), (4, SKID), (16, dict(SKID, twist=10, lean=-15)), (24, PUSH)]),
    'Air': (20, True, [(0, AIR_A), (10, AIR_B), (20, AIR_A)]),
    'Crouch': (40, True, [(0, CROUCH), (20, CROUCH_B), (40, CROUCH)]),
}

# Moves: name pattern -> three poses (anticipation, action, follow-through); shares of the montage length for each key
JAB_A = {'lean': -6, 'twist': -15, 'arm_r': -10, 'elbow_r': 110, 'arm_l': 40, 'elbow_l': 90, 'drop': 4, 'leg_l': 20, 'knee_l': 15}
JAB_B = {'lean': 14, 'twist': 25, 'arm_r': 85, 'elbow_r': 5, 'arm_l': 20, 'elbow_l': 100, 'drop': 6, 'fwd': 6, 'leg_l': 30, 'knee_l': 20, 'leg_r': -15}
KICK_A = {'lean': -10, 'leg_r': 60, 'knee_r': 100, 'arm_l': 30, 'arm_r': -20, 'out_l': 30, 'out_r': 30}
KICK_B = {'lean': -35, 'leg_r': 95, 'knee_r': 5, 'leg_l': 10, 'arm_l': -30, 'arm_r': -30, 'out_l': 50, 'out_r': 50}
DROPKICK_A = {'lean': -20, 'leg_l': 70, 'knee_l': 110, 'leg_r': 70, 'knee_r': 110, 'out_l': 40, 'out_r': 40}
# Run 3: legs in line with the lying body (95 forward on a body tilted back ~97 left the feet 60 cm above it)
DROPKICK_B = {'tilt': -72, 'lean': -25, 'leg_l': 12, 'knee_l': 0, 'leg_r': 8, 'knee_r': 5, 'arm_l': -40, 'arm_r': -40, 'out_l': 60, 'out_r': 60}
DAIR_A = {'lean': 25, 'leg_l': 80, 'knee_l': 120, 'leg_r': 80, 'knee_r': 120, 'arm_l': 60, 'arm_r': 60, 'out_l': 50, 'out_r': 50}
DAIR_B = {'lean': -5, 'drop': -10, 'leg_l': -5, 'knee_l': 0, 'leg_r': -5, 'knee_r': 0, 'arm_l': -20, 'arm_r': -20, 'out_l': 70, 'out_r': 70}
HIT_A = {'lean': -20, 'twist': -10, 'head': -15, 'arm_l': 40, 'arm_r': 30, 'out_l': 40, 'out_r': 40, 'elbow_l': 30, 'elbow_r': 30, 'drop': 6}
ROLL_A = {'drop': 40, 'tilt': 50, 'lean': 40, 'head': 30, 'leg_l': 100, 'knee_l': 130, 'leg_r': 100, 'knee_r': 130, 'arm_l': 60, 'arm_r': 60, 'elbow_l': 90, 'elbow_r': 90}
GENERIC_A = {'lean': -8, 'drop': 6, 'arm_l': -20, 'arm_r': -20}
GENERIC_B = {'lean': 20, 'fwd': 5, 'arm_l': 60, 'arm_r': 60, 'elbow_l': 20, 'elbow_r': 20}
# Owner 2026-11-24 14:43 (the rest of the Overlord moves on the capsule): second jab with the other hand, the dair landings, the dropkick's
# tech bounce. More specific patterns first (the first row whose pattern the name contains wins)
JAB2_A = {'lean': -4, 'twist': 15, 'arm_l': -10, 'elbow_l': 110, 'arm_r': 40, 'elbow_r': 90, 'drop': 4, 'leg_r': 20, 'knee_r': 15}
JAB2_B = {'lean': 16, 'twist': -28, 'arm_l': 88, 'elbow_l': 5, 'arm_r': 15, 'elbow_r': 100, 'drop': 7, 'fwd': 8, 'leg_r': 30, 'knee_r': 20, 'leg_l': -15}
DAIR_LAND_A = {'drop': 26, 'lean': 28, 'head': -10, 'arm_l': 30, 'arm_r': 30, 'out_l': 50, 'out_r': 50, 'elbow_l': 40, 'elbow_r': 40,
               'leg_l': 60, 'knee_l': 100, 'leg_r': 60, 'knee_r': 100}
DAIR_LAND_B = dict(DAIR_LAND_A, drop=14, lean=12)
TECH_A = dict(DROPKICK_B, tilt=-80, lean=-15)
TECH_B = {'drop': 30, 'tilt': 40, 'lean': 35, 'head': 25, 'leg_l': 100, 'knee_l': 130, 'leg_r': 100, 'knee_r': 130, 'arm_l': 50,
          'arm_r': 50, 'elbow_l': 90, 'elbow_r': 90}
MOVES = [
    # (name contains, startup share, active share, anticipation, action)
    ('jab2', 0.25, 0.2, JAB2_A, JAB2_B),
    ('dairland', 0.05, 0.35, DAIR_LAND_A, DAIR_LAND_B),
    ('techbounce', 0.1, 0.45, TECH_A, TECH_B),
    ('jab', 0.25, 0.2, JAB_A, JAB_B),
    ('ftilt', 0.3, 0.2, KICK_A, KICK_B),
    ('dropkick', 0.25, 0.3, DROPKICK_A, DROPKICK_B),
    ('dair', 0.3, 0.2, DAIR_A, DAIR_B),
    ('react', 0.08, 0.3, HIT_A, dict(HIT_A, lean=-8, head=-5)),
    ('hit', 0.08, 0.3, HIT_A, dict(HIT_A, lean=-8, head=-5)),
    ('roll', 0.15, 0.6, ROLL_A, dict(ROLL_A, tilt=70)),
    ('dodge', 0.15, 0.6, ROLL_A, dict(ROLL_A, tilt=70)),
    ('', 0.3, 0.2, GENERIC_A, GENERIC_B),
]


# ---------------------------------------------------------------------------------------------------------------- quaternion math
# Plain Python (x, y, z, w), FQuat semantics: qmul(a, b) applies b first, then a (see build_hit_additives.py for why not unreal.Quat *).
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    r = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qinv(q))
    return (r[0], r[1], r[2])


def qt(uq):
    return (uq.x, uq.y, uq.z, uq.w)


def quat_axis(axis, deg):
    a = math.radians(deg) * 0.5
    s = math.sin(a)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(a))


def smooth(t):
    return t * t * (3.0 - 2.0 * t)


SPINE = [('spine_01', 0.12), ('spine_02', 0.3), ('spine_03', 0.55), ('spine_04', 0.78), ('spine_05', 1.0)]


class Rig:
    """Character axes and per-bone helpers from the mannequin reference pose."""

    def __init__(self, skeleton):
        self.skeleton = skeleton
        ref = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
        self.ref = ref
        self.names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(ref)]
        ws, ls = unreal.AnimPoseSpaces.WORLD, unreal.AnimPoseSpaces.LOCAL
        self.cs = {b: unreal.AnimPoseExtensions.get_bone_pose(ref, b, ws) for b in self.names}
        self.loc = {b: unreal.AnimPoseExtensions.get_bone_pose(ref, b, ls) for b in self.names}
        self.parent = {}
        for b in self.names:
            try:
                self.parent[b] = str(unreal.AnimPoseExtensions.get_parent_bone_name(ref, b))
            except Exception:
                self.parent[b] = None
        hier = {'pelvis': 'root', 'spine_01': 'pelvis', 'spine_02': 'spine_01', 'spine_03': 'spine_02', 'spine_04': 'spine_03',
                'spine_05': 'spine_04', 'neck_01': 'spine_05', 'neck_02': 'neck_01', 'head': 'neck_02', 'clavicle_l': 'spine_05',
                'clavicle_r': 'spine_05', 'upperarm_l': 'clavicle_l', 'upperarm_r': 'clavicle_r', 'lowerarm_l': 'upperarm_l',
                'lowerarm_r': 'upperarm_r', 'hand_l': 'lowerarm_l', 'hand_r': 'lowerarm_r', 'thigh_l': 'pelvis', 'thigh_r': 'pelvis',
                'calf_l': 'thigh_l', 'calf_r': 'thigh_r', 'foot_l': 'calf_l', 'foot_r': 'calf_r'}
        for b, par in hier.items():
            if b in self.names and (not self.parent.get(b) or self.parent[b] == 'None'):
                self.parent[b] = par
        t = lambda b: self.cs[b].translation
        fwd = (t('ball_l') - t('foot_l')) + (t('ball_r') - t('foot_r')) if 'ball_l' in self.names else unreal.Vector(0, 1, 0)
        fwd = unreal.Vector(fwd.x, fwd.y, 0).normal()
        up = unreal.Vector(0, 0, 1)
        right = up.cross(fwd).normal()
        if (t('thigh_l') - t('thigh_r')).dot(right) > 0:
            right = right * -1.0
        self.up, self.fwd, self.right = (0.0, 0.0, 1.0), (fwd.x, fwd.y, 0.0), (right.x, right.y, right.z)
        # Signs: a positive angle must move the probe the named way
        self.s_lean = self.calib(self.right, self.up, self.fwd)          # top goes forward
        self.s_side = self.calib(self.fwd, self.up, self.right)          # top goes right
        self.s_twist = self.calib(self.up, self.fwd, self.right)         # front turns right
        self.s_swing = self.calib(self.right, (0.0, 0.0, -1.0), self.fwd)  # a hanging limb swings forward
        # Arms hang: angle about the forward axis that brings each upper arm from the reference pose to ~15 degrees off vertical
        self.hang = {}
        self.toward = {}
        for s in ('l', 'r'):
            v = tuple(getattr(t('hand_' + s), a) - getattr(t('upperarm_' + s), a) for a in 'xyz') if 'hand_' + s in self.names else (0, 0, -1)
            side_comp = sum(v[i] * self.right[i] for i in range(3))
            down_comp = -v[2]
            current = math.degrees(math.atan2(abs(side_comp), down_comp))  # 0 = straight down, 90 = T pose
            want = 15.0
            # rotate about forward so the hand moves toward the body (sign probed)
            probe = v
            toward = tuple(-math.copysign(1.0, side_comp) * self.right[i] for i in range(3))
            sign = self.calib(self.fwd, probe, toward)
            self.toward[s] = sign
            self.hang[s] = sign * (current - want)
        p('rig: fwd', self.fwd, 'right', self.right, 'signs lean/side/twist/swing', self.s_lean, self.s_side, self.s_twist, self.s_swing,
          'arm hang', {k: round(v, 1) for k, v in self.hang.items()})

    @staticmethod
    def calib(axis, probe, want):
        moved = qrot(quat_axis(axis, 10.0), probe)
        return 1.0 if sum((moved[i] - probe[i]) * want[i] for i in range(3)) > 0 else -1.0

    def own_world(self, pose):
        """Own world-axis rotation (about the character axes) of each animated bone for a pose."""
        g = lambda k: float(pose.get(k, 0.0))
        out = {}
        prev = (0.0, 0.0, 0.0)
        hip = (g('tilt') + g('lean') * PELVIS_LEAN_SHARE, g('side') * PELVIS_LEAN_SHARE)
        out['pelvis'] = qmul(quat_axis(self.fwd, self.s_side * hip[1]), quat_axis(self.right, self.s_lean * hip[0]))
        rest = 1.0 - PELVIS_LEAN_SHARE
        for bone, share in SPINE:
            cum = (g('lean') * rest * share, g('side') * rest * share, g('twist') * share)
            own = tuple(cum[i] - prev[i] for i in range(3))
            prev = cum
            out[bone] = qmul(quat_axis(self.up, self.s_twist * own[2]),
                             qmul(quat_axis(self.fwd, self.s_side * own[1]), quat_axis(self.right, self.s_lean * own[0])))
        out['head'] = qmul(quat_axis(self.up, self.s_twist * g('head_twist')), quat_axis(self.right, self.s_lean * g('head')))
        for s in ('l', 'r'):
            # hang first (toward the body), then away from it by 'out', then the forward swing
            arm = qmul(quat_axis(self.right, self.s_swing * g('arm_' + s)),
                       quat_axis(self.fwd, self.hang[s] - self.toward[s] * g('out_' + s)))
            out['upperarm_' + s] = arm
            out['lowerarm_' + s] = quat_axis(self.right, self.s_swing * g('elbow_' + s))
            out['thigh_' + s] = quat_axis(self.right, self.s_swing * g('leg_' + s))
            out['calf_' + s] = quat_axis(self.right, -self.s_swing * g('knee_' + s))
        return out

    def local_rotation(self, bone, world_own):
        parent = self.parent.get(bone) or 'root'
        pr = qt(self.cs[parent].rotation) if parent in self.cs else (0.0, 0.0, 0.0, 1.0)
        delta = qmul(qinv(pr), qmul(world_own, pr))
        return qmul(delta, qt(self.loc[bone].rotation))

    def pelvis_translation(self, pose):
        off = (self.fwd[0] * pose.get('fwd', 0.0), self.fwd[1] * pose.get('fwd', 0.0), -float(pose.get('drop', 0.0)))
        parent = self.parent.get('pelvis') or 'root'
        pr = qt(self.cs[parent].rotation) if parent in self.cs else (0.0, 0.0, 0.0, 1.0)
        local_off = qrot(qinv(pr), off)
        base = self.loc['pelvis'].translation
        return unreal.Vector(base.x + local_off[0], base.y + local_off[1], base.z + local_off[2])


def pose_at(keys, frame):
    """Pose at a frame: smoothstep between the surrounding keys."""
    for i in range(len(keys) - 1):
        f0, a = keys[i]
        f1, b = keys[i + 1]
        if f0 <= frame <= f1:
            t = 0.0 if f1 == f0 else smooth((frame - f0) / float(f1 - f0))
            names = set(a) | set(b)
            return {k: a.get(k, 0.0) + (b.get(k, 0.0) - a.get(k, 0.0)) * t for k in names}
    return dict(keys[-1][1])


def write_clip(rig, name, frames, loop, keys):
    path = '%s/%s' % (DIR, name)
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    factory = unreal.AnimSequenceFactory()
    factory.set_editor_property('target_skeleton', rig.skeleton)
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DIR, unreal.AnimSequence, factory)
    ctrl = seq.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('greybox capsule clip'), False)
    ctrl.set_frame_rate(unreal.FrameRate(FPS, 1), False)
    ctrl.set_number_of_frames(unreal.FrameNumber(frames), False)
    samples = frames + 1
    poses = [rig.own_world(pose_at(keys, f)) for f in range(samples)]
    pelvis = [rig.pelvis_translation(pose_at(keys, f)) for f in range(samples)]
    for bone in rig.names:
        lt = rig.loc[bone]
        if bone in poses[0]:
            rots = [unreal.Quat(*rig.local_rotation(bone, poses[f][bone])) for f in range(samples)]
        else:
            rots = [lt.rotation] * samples
        trans = pelvis if bone == 'pelvis' else [lt.translation] * samples
        if hasattr(ctrl, 'add_bone_curve'):
            ctrl.add_bone_curve(bone, False)
        ctrl.set_bone_track_keys(bone, trans, rots, [unreal.Vector(1, 1, 1)] * samples, False)
    ctrl.close_bracket(False)
    seq.set_editor_property('enable_root_motion', False)
    try:
        seq.set_editor_property('loop', bool(loop))
    except Exception:
        pass
    if SAVE:
        eal.save_asset(path, only_if_is_dirty=False)
    p('clip', path, 'frames', frames, 'loop', loop)
    return unreal.load_asset(path) if SAVE else seq


def move_keys(name, frames):
    low = name.lower()
    for pattern, startup, active, a, b in MOVES:
        if pattern in low:
            f1 = max(1, int(round(frames * startup)))
            f2 = min(frames - 1, f1 + max(1, int(round(frames * active))))
            return pattern or 'generic', [(0, IDLE), (f1, a), (f2, b), (frames, IDLE)]
    return 'generic', [(0, IDLE), (frames, IDLE)]


def move_poses(name):
    """(kind, anticipation, action) of the first MOVES row whose pattern the name contains."""
    low = name.lower()
    for pattern, _startup, _active, a, b in MOVES:
        if pattern in low:
            return pattern or 'generic', a, b
    return 'generic', GENERIC_A, GENERIC_B


def segment_role(name, index, count):
    """Role of one sequence of a multi-sequence move: by its name (Startup / Loop / Land), else by its place in the montage."""
    low = name.lower()
    for role in ('startup', 'loop', 'land'):
        if role in low:
            return role
    if count > 1 and index == 0:
        return 'startup'
    if count > 1 and index == count - 1:
        return 'land'
    return 'loop' if count > 1 else 'whole'


def role_keys(name, role, frames):
    """Run 3 (dropkick stayed upright): a move split in sequences holds its action pose across the cut. Startup ends ON the action pose,
    the loop holds it (it repeats in the air for as long as the fall lasts), the land leaves it for the rest pose."""
    kind, a, b = move_poses(name)
    if kind == 'techbounce':
        # The tech bounce is the dropkick until it lands, then the bounce: its startup and loop are the dropkick's
        if role in ('startup', 'loop'):
            a, b = DROPKICK_A, DROPKICK_B
        elif role == 'land':
            f1 = max(1, int(round(frames * 0.4)))
            return kind, [(0, TECH_A), (min(f1, frames - 1), TECH_B), (frames, IDLE)]
    if role == 'startup':
        f1 = max(1, int(round(frames * 0.55)))
        return kind, [(0, IDLE), (min(f1, frames - 1), a), (frames, b)]
    if role == 'loop':
        return kind, [(0, b), (frames // 2, b), (frames, b)]
    if role == 'land':
        f1 = max(1, int(round(frames * 0.35)))
        return kind, [(0, b), (min(f1, frames - 1), b), (frames, IDLE)]
    return move_keys(name, frames)


def segment_sequences(montage):
    """The sequences of the montage's first slot track, in order (duplicates once)."""
    out = []
    try:
        track = montage.get_editor_property('slot_anim_tracks')[0].get_editor_property('anim_track')
        for seg in track.get_editor_property('anim_segments'):
            ref = seg.get_editor_property('anim_reference')
            if ref and ref not in out:
                out.append(ref)
    except Exception as exc:
        p('segments of', montage.get_name(), 'unreadable:', exc)
    return out


def find_montages():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    out = []
    for root in ('/Game/GSTN', '/Game/GAS', '/Game/Blueprints'):
        for data in reg.get_assets_by_path(root, recursive=True):
            name = str(data.asset_name)
            if str(data.asset_class_path.asset_name) != 'AnimMontage' or not any(f in name.lower() for f in MOVE_FILTER):
                continue
            out.append(str(data.package_name))
    return sorted(set(out))


def update_chooser(made):
    cht = unreal.EditorAssetLibrary.load_asset(CHOOSER)
    if not cht:
        fail('chooser %s missing: run setup_greybox_assets.py first' % CHOOSER)
        return
    # Toolset-only UFUNCTIONs (not Python exposed): call them by name on the CDO
    cdo = unreal.GSTN_ChooserTableToolset.get_default_object()

    class tool:
        get_chooser_table = staticmethod(lambda *a: cdo.call_method('GetChooserTable', a))
        set_chooser_row_result = staticmethod(lambda *a: cdo.call_method('SetChooserRowResult', a))
    info = tool.get_chooser_table(cht)
    state_col = 0
    import re
    matched = 0
    for row_index, row in enumerate(info.rows):
        cell = str(row.cells[state_col]) if row.cells else ''
        p('chooser cell', row_index, repr(cell)[:200])
        # Whole word: 'Run' must not match 'RunStart' / 'RunStop' (the cell text format differs between toolset versions)
        hits = [s for s in made if re.search(r'(?<![A-Za-z0-9_])%s(?![A-Za-z0-9_])' % s, cell)]
        if len(hits) == 1:
            state, clip = hits[0], made[hits[0]]
            tool.set_chooser_row_result(cht, row_index, '', {'Asset': clip.get_path_name()})
            p('chooser row', row_index, state, '->', clip.get_path_name())
            matched += 1
    if matched < len(made):
        fail('chooser: only %d of %d locomotion rows matched a LocoState cell (cells logged above)' % (matched, len(made)))
    if SAVE:
        unreal.EditorAssetLibrary.save_asset(CHOOSER, only_if_is_dirty=False)


def find_sequences(covered):
    """AnimSequences under the move roots whose name matches SEQUENCE_FILTER, plus SEQUENCES, minus the ones a montage already covers."""
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    out = set(EXTRA_SEQUENCES)
    for root in ('/Game/GSTN', '/Game/GAS', '/Game/Blueprints'):
        for data in reg.get_assets_by_path(root, recursive=True):
            name = str(data.asset_name)
            if str(data.asset_class_path.asset_name) == 'AnimSequence' and any(f in name.lower() for f in SEQUENCE_FILTER) \
                    and not name.startswith('AS_GSTN_Greybox'):
                out.add(str(data.package_name))
    return sorted(path for path in out if path.split('.')[0] not in covered)


def first_segment_sequence(montage):
    try:
        track = montage.get_editor_property('slot_anim_tracks')[0].get_editor_property('anim_track')
        return track.get_editor_property('anim_segments')[0].get_editor_property('anim_reference')
    except Exception as exc:
        p('first segment of', montage.get_name(), 'unreadable:', exc)
        return None


def update_move_overrides(overrides, sequence_overrides=None):
    bp = unreal.EditorAssetLibrary.load_asset(CAPSULE_ABP)
    if not bp:
        fail('capsule ABP %s missing' % CAPSULE_ABP)
        return
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    # Replaced, not merged: owner 2026-11-24 bones only in attacks, so the run 1 entries for hit reactions and rolls must go (those are
    # the rigid capsule swaying now)
    current = dict(overrides)
    cdo.set_editor_property('move_overrides', current)
    if sequence_overrides is not None:
        by_sequence = dict(sequence_overrides)
        cdo.set_editor_property('sequence_move_overrides', by_sequence)
        p('sequence move overrides on the capsule ABP:', len(by_sequence))
    # Run 3 (owner 14:32): the run start sway default went 1.4 -> 0.7 in C++; a value saved on the ABP before that would keep 1.4
    shape = cdo.get_editor_property('shape_settings')
    frantic = shape.get_editor_property('frantic_amplitude')
    p('ShapeSettings.FranticAmplitude on the capsule ABP:', frantic)
    # Run 4: still not clearly smaller on video: 0.45. Only the old defaults are replaced, never a value the owner tuned
    if any(abs(frantic - old) < 1e-3 for old in (1.4, 0.7)):
        shape.set_editor_property('frantic_amplitude', 0.45)
        cdo.set_editor_property('shape_settings', shape)
        p('ShapeSettings.FranticAmplitude set to 0.45')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    p('move overrides on the capsule ABP:', len(current))
    if SAVE:
        unreal.EditorAssetLibrary.save_asset(CAPSULE_ABP, only_if_is_dirty=False)


def run():
    p('make_capsule_clips', time.strftime('%Y-%m-%d %H:%M:%S'), 'SAVE' if SAVE else 'DRY RUN')
    mannequin = unreal.EditorAssetLibrary.load_asset(MANNEQUIN)
    rig = Rig(mannequin.get_editor_property('skeleton'))
    made = {}
    for state, (frames, loop, keys) in LOCOMOTION.items():
        made[state] = write_clip(rig, 'AS_GSTN_Greybox_' + state, frames, loop, keys)
    update_chooser(made)
    overrides = {}
    sequence_overrides = {}
    covered = set()
    for path in find_montages():
        montage = unreal.EditorAssetLibrary.load_asset(path)
        try:
            slots = sorted({str(t.get_editor_property('slot_name')) for t in montage.get_editor_property('slot_anim_tracks')})
        except Exception as exc:
            slots = ['? %s' % exc]
        p('slots', montage.get_name(), slots)
        length = montage.get_play_length()
        frames = max(2, int(round(length * FPS)))
        kind, keys = move_keys(montage.get_name(), frames)
        clip = write_clip(rig, 'AS_GSTN_Greybox_Move_' + montage.get_name(), frames, False, keys)
        overrides[montage] = clip
        sequences = segment_sequences(montage)
        if len(sequences) > 1:
            # Run 3: one clip per sequence, asked before the montage's clip (the capsule looks up the segment under the montage's time)
            for index, sequence in enumerate(sequences):
                seq_frames = max(2, int(round(sequence.get_play_length() * FPS)))
                role = segment_role(sequence.get_name(), index, len(sequences))
                seq_kind, seq_keys = role_keys(montage.get_name(), role, seq_frames)
                sequence_overrides[sequence] = write_clip(rig, 'AS_GSTN_Greybox_Seq_' + sequence.get_name(), seq_frames, role == 'loop', seq_keys)
                covered.add(sequence.get_path_name().split('.')[0])
                p('  segment', sequence.get_name(), 'role', role, 'kind', seq_kind, 'frames', seq_frames)
        else:
            # The same clip for a dynamic montage that plays this montage's sequence
            played = first_segment_sequence(montage)
            if played and played not in sequence_overrides:
                sequence_overrides[played] = clip
                covered.add(played.get_path_name().split('.')[0])
        p('move', path, 'length %.3f s' % length, 'kind', kind, 'clip length %.3f s' % (frames / float(FPS)))
    for path in find_sequences(covered):
        sequence = unreal.EditorAssetLibrary.load_asset(path)
        if not sequence:
            fail('sequence %s not found' % path)
            continue
        frames = max(2, int(round(sequence.get_play_length() * FPS)))
        role = segment_role(sequence.get_name(), 0, 1)
        kind, keys = role_keys(sequence.get_name(), role, frames)
        clip = write_clip(rig, 'AS_GSTN_Greybox_Seq_' + sequence.get_name(), frames, role == 'loop', keys)
        sequence_overrides[sequence] = clip
        p('sequence', path, 'length %.3f s' % sequence.get_play_length(), 'kind', kind)
    update_move_overrides(overrides, sequence_overrides)
    p('RESULT:', 'PASS' if not errors else 'FAIL (%d errors)' % len(errors))


try:
    run()
except Exception as exc:
    import traceback
    fail('exception: %s' % exc)
    p(traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')
print('[make_capsule_clips] %s, report %s' % ('PASS' if not errors else 'FAIL %d: %s' % (len(errors), errors[0][:160]), OUT))
