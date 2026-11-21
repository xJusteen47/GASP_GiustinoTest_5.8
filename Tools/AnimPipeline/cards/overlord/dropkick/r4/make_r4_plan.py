"""Round 4 of the dropkick tech bounce (dropkick-tb-4, laptop, 2026-11-21): writes overlord_dropkick_techbounce_r4.plan.json from the
director's design (docs/notes/reviews/dropkick-tech-bounce-v4/design-r4.md, section 3) and the capsule hop MEASURED in the base capture.

Why a script: the design gives WORLD pelvis heights; the clip is authored in component space, whose root rides the 25 cm capsule hop. So
clip pelvis z = world target - root z of that frame (root z = mean of base2 low_whiff / low_hit, Saved/DropkickTB4/base2/*/frames.jsonl,
actor_z + mesh_rel_z on the tech bounce frames). Arms: chest-frame values sampled along r3's own uncross arc (r3 dense author f4 -> f12),
retimed to the design's percentages; legs: component targets from the design's leg angle, heels on the floor f2-f4.
    python cards/overlord/dropkick/r4/make_r4_plan.py      (from Tools/AnimPipeline; then pose_plan.py on the plan)"""
import json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
AP = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
R3 = json.load(open(os.path.join(AP, 'cards/overlord/dropkick/overlord_dropkick_techbounce_r3.json')))
R3PLAN = json.load(open(os.path.join(AP, 'cards/overlord/dropkick/overlord_dropkick_techbounce_r3.plan.json')))
GH0 = json.load(open(os.path.join(AP, 'cards/reaction/reaction_groundhit.plan.json')))['keys']['0']
PELVIS_IDLE_Z = 90.6          # comp z of the pelvis bone = 90.6 + pelvis.loc z (r3 f0: -80 -> 10.6 solved)
LEG = 85.0                    # hip -> ankle, straight (r3 f0 solved)
ROOT = [3.3, 9.3, 14.5, 18.8, 22.3, 25.0, 26.9, 28.0, 28.2, 27.4, 24.6, 21.6]   # measured capsule hop, f0..f11
# key: world pelvis z, pitch, spine, head, leg angle (deg above the floor, None = heels on the floor), knee, L arc %, R arc %, fingers, legs parted %
KEYS = {0: (15, 82, -8, -22, 15, 0, 0, 0, 0.0, 0), 1: (12, 90, -2, -16, 10, 5, 0, 0, 0.0, 0),
        2: (11, 92, 4, -6, None, 10, 0, 0, 0.0, 0), 3: (12, 91, 6, 8, None, 6, 0, 0, 0.0, 0),
        4: (23, 94, 10, 10, None, 0, 0, 0, 0.0, 0), 6: (40, 93, 4, 2, 9, 0, 25, 0, 0.2, 0),
        8: (45, 90, -4, -8, 16, 0, 60, 20, 0.4, 0), 10: (41, 86, -10, -14, 22, 0, 80, 55, 0.6, 30),
        11: (36.5, 83, -13, -13, 18, 0, 90, 75, 0.7, 65)}
dense = R3['author']['poses']


def arc(side, pct):
    """chest-frame hand/pole along r3's uncross: 0 % = r3 f4 (crossed), 100 % = r3 f12 (GroundHit f2 spread)"""
    t = 4 + 8 * pct / 100.0
    a, b = int(math.floor(t)), min(12, int(math.floor(t)) + 1)
    w = t - a
    pa, pb = dense[str(a)][f'arm_{side}'], dense[str(b)][f'arm_{side}']
    return {k: [round(pa[k][i] * (1 - w) + pb[k][i] * w, 2) for i in range(3)] for k in ('hand', 'pole')}


def legs(zc, py, ang, knee, parted, f):
    floor = -ROOT[f]
    d = LEG * math.cos(math.radians(knee / 2.0))
    if ang is None:     # heels on the floor: the ankle sits 9 cm over it
        ang = math.degrees(math.asin(max(-1.0, min(1.0, (floor + 9.0 - zc) / d))))
    out = {}
    for side, sx in (('l', 1), ('r', -1)):
        a = math.radians(ang)
        foot = [5.0 * sx, py + d * math.cos(a), zc + d * math.sin(a)]
        gh = GH0[f'leg_{side}']['foot_w']
        if parted:       # part toward GroundHit key 0, in x and in the far/near split only (the height stays the design's angle)
            foot[0] = foot[0] + (gh[0] - foot[0]) * parted / 100.0
            foot[1] = foot[1] + ((gh[1] - 50.5) * parted / 100.0)
        n = (0.0, -math.sin(a), math.cos(a))
        mid = [(foot[0]) / 2.0, (py + foot[1]) / 2.0, (zc + foot[2]) / 2.0]
        pole = [12.0 * sx, mid[1] + 40 * n[1], mid[2] + 40 * n[2]]
        out[f'leg_{side}'] = {'foot_w': [round(v, 2) for v in foot], 'pole_w': [round(v, 2) for v in pole]}
    return out, round(ang, 1)


keys, report = {}, []
for f, (wz, pitch, spine, head, ang, knee, lp, rp, fing, parted) in KEYS.items():
    zc = wz - ROOT[f]
    s = f / 11.0
    px, py = round(-2.1 * s, 2), round(-8.0 + 0.5 * s, 2)
    k = {'pelvis': {'loc': [px, py, round(zc - PELVIS_IDLE_Z, 2)], 'rot_e': {'pitch': pitch}},
         'spine': [spine, 0, 0], 'head': [head, 0, 0], 'fingers': [fing, 0, 0],
         'arm_l': arc('l', lp), 'arm_r': arc('r', rp)}
    lg, used = legs(zc, py, ang, knee, parted, f)
    k.update(lg)
    keys[str(f)] = k
    report.append(f'f{f}: world {wz} root {ROOT[f]} comp {zc:.1f} leg {used} deg')
card = {k: v for k, v in R3PLAN['card'].items()}
card.update({'move': 'overlord_dropkick_techbounce_r4',
             'anim': '/Game/AnimPipeline/Rig/Overlord/Dropkick/AS_OverlordDropkickTechBounceR4',
             'source_anim': '/Game/AnimPipeline/Pilot/AS_Pilot_OverlordDropkickTechBounceR4',
             'timing_csv': 'refs/ufd/OverlordDropkick_techbounce_r4_timing.csv',
             'exemptions': [{'check': c, 'frames': list(range(12)), 'why': 'dropkick tech bounce, lying'} for c in ('balance', 'planted')],
             '_design': 'r4 (design-r4.md): back bump of a rigid log, hips f0 -> back f1 -> heels f2 -> head f3, 3 f on the floor while the capsule '
                        'rises (clip counter-lift), hips leave first f4, flat apex f8 (world pelvis 45), shallow V falling into GroundHit f1; '
                        'arms crossed to f4, L from f5, R from f7'})
card.pop('poses', None)
plan = {'card': card, 'card_path': 'cards/overlord/dropkick/overlord_dropkick_techbounce_r4.json',
        'take': 'AS_Pilot_OverlordDropkickTechBounceR4', 'frames': 12, 'thumb': 'along', 'keys': keys}
out = os.path.join(AP, 'cards/overlord/dropkick/overlord_dropkick_techbounce_r4.plan.json')
json.dump(plan, open(out, 'w'), indent=1)
print('r4 plan:', os.path.relpath(out, AP)); print(' | '.join(report))
