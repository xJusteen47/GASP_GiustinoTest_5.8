# getup-momentum-1 (2026-11-21): where each knockdown get-up clip is "up enough to act" (its FAF), read-only, headless.
# Per frame of the clip behind each get-up montage: pelvis height and both feet heights (component space, cm, chained local poses as
# runstart_turn_bones_ue.py). FAF candidate = first frame from which the pelvis stays >= RATIO of its last-frame height AND both feet stay
# within FOOT_UP cm of their lowest height in the clip's last third (planted). Writes Saved/Getup/faf_measure.json and prints <= 5 lines.
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<copy of this file in a path without spaces> -unattended -nullrhi
import json
import math
import os

import unreal

REACT = '/Game/GASNPPTest/Combat/Reaction'
MONTAGES = ['AM_React_GetUp', 'AM_React_Roll', 'AM_React_RollBack']
RATIO = 0.85
FOOT_UP = 6.0
SIM_HZ = 62.5
AL = unreal.AnimationLibrary
CHAINS = {'pelvis': ['root', 'pelvis'], 'foot_l': ['root', 'pelvis', 'thigh_l', 'calf_l', 'foot_l'],
          'foot_r': ['root', 'pelvis', 'thigh_r', 'calf_r', 'foot_r']}


def _sequence(montage):
    for track in montage.get_editor_property('slot_anim_tracks'):
        for seg in track.get_editor_property('anim_track').get_editor_property('anim_segments'):
            ref = seg.get_editor_property('anim_reference')
            if ref:
                return ref
    return None


def _heights(seq):
    n = int(AL.get_num_frames(seq)) + 1
    out = {k: [] for k in CHAINS}
    for f in range(n):
        cache = {}
        for k, chain in CHAINS.items():
            t = None
            for i, b in enumerate(chain):
                key = tuple(chain[:i + 1])
                if key not in cache:
                    loc = AL.get_bone_pose_for_frame(seq, b, f, False)
                    cache[key] = loc if t is None else unreal.MathLibrary.compose_transforms(loc, t)
                t = cache[key]
            out[k].append(round(t.translation.z, 2))
    return n, out


def _faf(n, h):
    pel = h['pelvis']
    tail = range(int(n * 2 / 3), n)
    low = {k: min(h[k][i] for i in tail) for k in ('foot_l', 'foot_r')}
    for f in range(n):
        if all(pel[i] >= RATIO * pel[-1] for i in range(f, n)) and all(h[k][i] <= low[k] + FOOT_UP for k in low for i in range(f, n)):
            return f
    return n - 1


def run():
    res = {}
    lines = []
    for name in MONTAGES:
        m = unreal.load_asset(f'{REACT}/{name}')
        seq = _sequence(m) if m else None
        if not seq:
            lines.append(f'{name}: missing')
            continue
        n, h = _heights(seq)
        fps = round((n - 1) / max(seq.get_play_length(), 1e-3), 2)
        f = _faf(n, h)
        sim_total = math.ceil(m.get_play_length() * SIM_HZ)
        sim_faf = math.ceil(f / fps * SIM_HZ)
        res[name] = {'sequence': seq.get_name(), 'frames': n, 'fps': fps, 'faf_clip': f, 'sim_total': sim_total, 'sim_faf': sim_faf,
                     'pelvis': h['pelvis'], 'foot_l': h['foot_l'], 'foot_r': h['foot_r']}
        lines.append(f'{name} ({seq.get_name()}): clip {n} f @{fps:g}, FAF clip f{f} -> sim {sim_faf}/{sim_total}, tail cut sim [{sim_faf},{sim_total})')
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Getup')
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'faf_measure.json'), 'w') as fh:
        json.dump(res, fh, indent=1)
    with open(os.path.join(out, 'faf_measure.txt'), 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
    return lines


for _line in run()[:5]:
    print(_line)
