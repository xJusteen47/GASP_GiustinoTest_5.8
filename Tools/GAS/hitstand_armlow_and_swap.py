"""Owner decisions 2026-11-18 (2A, 3A). Headless, idempotent:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<project>/Tools/GAS/hitstand_armlow_and_swap.py" -unattended -nullrhi
3A: variant of the hitstun pose with the arm that points at the attacker lowered (default 25 deg, env HITSTAND_ARM_DEG):
    /Game/GSTN/Animation/Reaction/AS_GSTN_React_HitStand_ArmLow, a copy of /Game/GASNPPTest/Combat/Reaction/AS_React_HitStand (the original
    is not touched). Only the upper arm track changes: a rotation about the shoulder, about the horizontal axis across the arm, every frame.
2A: DA_ReactionAnims_HitAdd (the variant set) gets the ArmLow clip on its HitStand rule and keeps only the 4 reviewed directional clips
    (ground front/left/right, air front; the other 4 go back to the placeholder bend); the character Blueprint SandboxCharacter_Mover then
    uses that set. DA_ReactionAnims itself is not touched (the owner edits it).
Log: Saved/Reaction/hitstand_armlow_and_swap.log (ends with DONE_MARK)."""
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

SRC = '/Game/GASNPPTest/Combat/Reaction/AS_React_HitStand'
DST = '/Game/GSTN/Animation/Reaction/AS_GSTN_React_HitStand_ArmLow'
VSET = '/Game/GSTN/Animation/Reaction/DA_ReactionAnims_HitAdd'
DEG = float(os.environ.get('HITSTAND_ARM_DEG') or 25.0)
SWAP = os.environ.get('HITSTAND_SWAP', '1') == '1'
UNREVIEWED = {('ground', 'back'), ('air', 'back'), ('air', 'left'), ('air', 'right')}
LOG = os.path.join(unreal.Paths.project_saved_dir(), 'Reaction', 'hitstand_armlow_and_swap.log')
L = []


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz)


def qinv(q):
    return (-q[0], -q[1], -q[2], q[3])


def qrot(q, v):
    r = qmul(qmul(q, (v[0], v[1], v[2], 0.0)), qinv(q))
    return r[:3]


def qaxis(axis, deg):
    a = math.radians(deg) * 0.5
    return (axis[0] * math.sin(a), axis[1] * math.sin(a), axis[2] * math.sin(a), math.cos(a))


def qt(u):
    return (u.x, u.y, u.z, u.w)


def vt(u):
    return (u.x, u.y, u.z)


def sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def norm(a):
    n = math.sqrt(sum(x * x for x in a)) or 1.0
    return tuple(x / n for x in a)


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def comp_pose(seq, frame):
    """Component-space rotation/position of the arm chain from the stored local tracks (root -> pelvis -> spine -> clavicle -> arm)."""
    chain = {'l': ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'clavicle_l', 'upperarm_l', 'lowerarm_l', 'hand_l'],
             'r': ['root', 'pelvis', 'spine_01', 'spine_02', 'spine_03', 'spine_04', 'spine_05', 'clavicle_r', 'upperarm_r', 'lowerarm_r', 'hand_r']}
    out = {}
    for s, bones in chain.items():
        rot, pos = (0.0, 0.0, 0.0, 1.0), (0.0, 0.0, 0.0)
        for b in bones:
            lt = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, b, frame, False)
            tr = qrot(rot, vt(lt.translation))
            pos = tuple(pos[i] + tr[i] for i in range(3))
            rot = qmul(rot, qt(lt.rotation))
            out[b] = (rot, pos)
    return out


def armlow():
    EAL = unreal.EditorAssetLibrary
    if EAL.does_asset_exist(DST):
        EAL.delete_asset(DST)
    EAL.duplicate_asset(SRC, DST)
    seq = unreal.load_asset(DST)
    n = seq.get_editor_property('number_of_sampled_keys')
    p0 = comp_pose(seq, 0)
    # forward = from the heel to the ball of the feet (character facing), up = +Z
    up = (0.0, 0.0, 1.0)
    # the arm that points at the attacker: the hand furthest along the chest's facing, nearest to horizontal
    pick, best = None, -1e9
    for s in ('l', 'r'):
        d = sub(p0[f'hand_{s}'][1], p0[f'upperarm_{s}'][1])
        elev = math.degrees(math.asin(max(-1.0, min(1.0, norm(d)[2]))))
        L.append(f'arm {s}: shoulder->hand {tuple(round(x, 1) for x in d)} elevation {elev:.1f} deg')
        score = -abs(elev)
        if score > best:
            pick, best = s, score
    L.append(f'lowering arm {pick} by {DEG} deg')
    ua = f'upperarm_{pick}'
    rots, poss, scls = [], [], []
    for f in range(n):
        pf = comp_pose(seq, f)
        d = norm(sub(pf[f'hand_{pick}'][1], pf[ua][1]))
        axis = norm(cross(d, up))
        w = qaxis(axis, DEG)
        if qrot(w, d)[2] > d[2]:       # must move the hand down
            w = qaxis(axis, -DEG)
        prot = pf[f'clavicle_{pick}'][0]
        q = qmul(qinv(prot), qmul(w, prot))          # world rotation about the shoulder -> local pre-multiplied delta
        lt = unreal.AnimationLibrary.get_bone_pose_for_frame(seq, ua, f, False)
        rots.append(unreal.Quat(*qmul(q, qt(lt.rotation))))
        poss.append(lt.translation)
        scls.append(unreal.Vector(1, 1, 1))
    ctrl = seq.get_editor_property('controller')
    ctrl.open_bracket(unreal.Text('hitstand arm low'), False)
    ctrl.set_bone_track_keys(ua, poss, rots, scls, False)
    ctrl.close_bracket(False)
    EAL.save_asset(DST)
    seq = unreal.load_asset(DST)
    p1 = comp_pose(seq, 0)
    d0 = sub(p0[f'hand_{pick}'][1], p0[ua][1])
    d1 = sub(p1[f'hand_{pick}'][1], p1[ua][1])
    ang = math.degrees(math.acos(max(-1.0, min(1.0, sum(norm(d0)[i] * norm(d1)[i] for i in range(3))))))
    L.append(f'{DST}: {n} keys; hand {pick} moved {tuple(round(d1[i] - d0[i], 1) for i in range(3))} cm, arm turned {ang:.1f} deg, '
             f'elevation {math.degrees(math.asin(norm(d1)[2])):.1f} deg')
    return seq


def variant_set(clip):
    rset = unreal.load_asset(VSET)
    rules = list(rset.get_editor_property('rules'))
    hit = 0
    for r in rules:
        if str(r.get_editor_property('name')) == 'HitStand':
            r.set_editor_property('animation', clip)
            hit += 1
    rset.set_editor_property('rules', rules)
    adds = rset.get_editor_property('directional_hits')
    for key in ('ground', 'air'):
        # setting None on a soft pointer is ignored by the binding: rebuild the struct with only the reviewed clips
        old = adds.get_editor_property(key)
        a = unreal.GSTN_DirectionalHitAdditives()
        a.set_editor_property('placeholder_angle_deg', old.get_editor_property('placeholder_angle_deg'))
        for side in ('front', 'back', 'left', 'right'):
            if (key, side) not in UNREVIEWED:
                a.set_editor_property(side, old.get_editor_property(side))
        adds.set_editor_property(key, a)
    rset.set_editor_property('directional_hits', adds)
    unreal.EditorAssetLibrary.save_loaded_asset(rset, False)
    rset = unreal.load_asset(VSET)
    adds = rset.get_editor_property('directional_hits')
    slots = [f'{k}.{s}={"clip" if adds.get_editor_property(k).get_editor_property(s) else "placeholder"}'
             for k in ('ground', 'air') for s in ('front', 'back', 'left', 'right')]
    L.append(f'{VSET}: HitStand rules set {hit}; ' + ', '.join(slots))
    return rset


try:
    clip = armlow()
    rset = variant_set(clip)
    if SWAP:
        import setup_reaction_anims as sra
        sra.character_component(rset)
        L.extend(sra._lines)
except Exception as ex:
    import traceback
    L.append('ERROR ' + repr(ex))
    L.append(traceback.format_exc())
os.makedirs(os.path.dirname(LOG), exist_ok=True)
open(LOG, 'w').write('\n'.join(L) + '\nDONE_MARK\n')
