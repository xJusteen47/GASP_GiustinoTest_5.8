# Owner playtest 2026-11-25: four reference moves on the Overlord, with Smash Ultimate's numbers, "che mi servono per fare playtest":
#   F.S        (Forward Strong)        = Ganondorf's forward smash      (vertical overhead sword slash)
#   N.S        (Neutral Strong)        = Ike's forward tilt             (horizontal forward sword swing, "Swing Out")
#   N.L Aerial (Neutral Light Aerial)  = Ganondorf's neutral aerial     (two kicks, the first carries the victim into the second)
#   N.S Aerial (Neutral Strong Aerial) = Ike's neutral aerial           (sword swung in a circle around the body, "Round Slash")
# Notation: docs/notes/notazione-mosse.md. Greybox phase: the poses are capsule placeholders, the numbers are the point.
#
# Pure Python (no `unreal`): the data, the conversions and the per-frame hitbox paths, shared by Tools/GAS/setup_overlord_ref_moves.py (the
# editor script) and the table in docs/notes/overlord-ref-moves.md. `python3 Tools/GAS/overlord_ref_moves_data.py` prints the table.
#
# SOURCES (read 2026-11-25; values copied, not guessed):
#   UFD  = ultimateframedata.com/ganondorf, /ike  (startup, total, landing lag, autocancel, active frames, damage)
#   SW   = ssbwiki.com Ganondorf_(SSBU)/Forward_smash, /Neutral_aerial; Ike_(SSBU)/Forward_tilt, /Neutral_aerial (hitbox tables: damage,
#          angle, BKB, KBG, radius, bone offsets, timing "Interruptible" and "Animation length")
#   Where they disagree the SW hitbox table wins for hit numbers (UFD lists Ike's ftilt at 12.5 %, the SSB4 value; SW SSBU says 13.5 %) and
#   UFD wins for landing lag (Ganon nair 10, Ike nair 8). Angles: SW shows them as icons (AngleIcon40 = 40 deg, AngleIcon361 = Sakurai angle).
#
# CONVERSIONS (the project's own, not new):
#   frames    Smash frame f (1-based, 60 fps) = timeline frame f - 1 (0-based, one simulation tick each; montages play at 62.5/60 so timeline
#             frame N shows animation frame N, as the jab and dropkick scripts do). FAF = SW "Interruptible" - 1. Total = SW "Animation length".
#   knockback BKB in cm/s = Smash BKB x 22.5 (UNIT_SCALE of knockback_smash_data.py, the Smash formula of the standard rules); KBG as is.
#   hitstop   Ultimate's hitlag: floor((damage x 0.65 + 6) x hitlag multiplier), at most 30, both fighters (attacker = victim).
#   size      1 Smash unit = UNIT_CM cm. ASSUMPTION (no source gives a unit in cm): the Overlord (~190 cm) is as tall as Ganondorf (~20 units),
#             so 9.5 cm. One number to retune for every reach and radius.
# SIMPLIFICATIONS (said, not hidden): Ganon's fsmash is not chargeable here (Smash: charge hold on frame 14); Ike's ftilt is the
# "angled forward" variant only; Ganon nair hit 1's knee hitbox has a grounded-victim (55 deg, 50/110 set weight) and an aerial-victim version
# (100 deg, 20/30): the timeline has no per-victim-state hitbox, so it uses the knee hitbox shared by both (70 deg, 20/30); Ganon nair's late
# second hit radii are the summary values of SW (6.0 / 5.3 / 4.5), not read verbatim. Every box is a placeholder path around the Overlord's
# root (capsule centre, 90 cm above the floor), shaped after the Smash hitboxes' bones and offsets, not baked from an animation.
# CENTRED (owner 2026-11-25): every path lies on the character's facing axis (vertical arcs and kicks in the X-Z plane, y = 0; the horizontal
# swing symmetric about it), and the capsule poses are symmetric or cross the facing at the middle of the active frames.
#
# PENETRATION (GSTNParts, 0..15 against a part's hardness; owner 11-23: F.S "penetrazione media"). PROPOSALS for the playtest, the owner decides.
import math

UNIT_SCALE = 22.5
UNIT_CM = 9.5
HITLAG_CAP = 30
SIM_HZ, ANIM_FPS = 62.5, 60.0

PENETRATION = {'FS': 6, 'NS': 4, 'NLAir': 2, 'NSAir': 4}


def hitlag(damage, mult=1.0):
    return min(HITLAG_CAP, int(math.floor((damage * 0.65 + 6.0) * mult)))


def tl(smash_frame):
    """Smash frame (1-based) -> timeline frame (0-based)"""
    return smash_frame - 1


# A hit: (name, damage %, angle deg or 'sakurai', BKB smash, KBG, hitlag multiplier, priority)
# A hitbox window: (first smash frame, last smash frame, path kind, [(hit name, radius units, distance along the path's arm units)])
MOVES = {
    'FS': {
        'name': 'F.S', 'ref': 'Ganondorf forward smash (SSBU)', 'command': 'Command.Strong.Directional', 'air': False,
        'total': 90, 'faf': tl(70),
        'sources': 'SW Ganondorf_(SSBU)/Forward_smash: early hit 29, clean 30-31, interruptible 70, animation 90, all IDs 24 %, '
                   'AngleIcon40, BK 61, KS 75, bone haver; UFD: startup 29, total 69, active 29-31, 24.0, "hits targets above him on frame 29"',
        'hits': {'Early': (24.0, 40.0, 61, 75, 1.0, 1), 'Clean': (24.0, 40.0, 61, 75, 1.0, 1)},
        # vertical arc: the arm (sword) pivots at the hand; angle 90 = straight up, 0 = straight ahead, negative = toward the floor
        'windows': [
            (29, 29, ('vertical', (35.0, 40.0), 80.0, 60.0), [('Early', 4.5, 4.0), ('Early', 4.0, 12.0)]),
            (30, 31, ('vertical', (55.0, 15.0), 45.0, -25.0), [('Clean', 5.0, 6.5), ('Clean', 4.5, 14.5), ('Clean', 4.0, 0.0)]),
        ],
        'landing': None,
    },
    'NS': {
        'name': 'N.S', 'ref': 'Ike forward tilt (SSBU), angled forward', 'command': 'Command.Strong.Neutral', 'air': False,
        'total': 60, 'faf': tl(42),
        'sources': 'SW Ike_(SSBU)/Forward_tilt: hitboxes 12-13, interruptible 42, animation 60, ID 0 13.5 %, Sakurai angle, BK 30, KS 97, '
                   'radius 4.0, bone top, offset 10 -> 24 (a stretched box); UFD: startup 12, total 41, on shield -17',
        'hits': {'Swing': (13.5, 'sakurai', 30, 97, 1.0, 0)},
        # horizontal arc at chest height, CENTRED on the facing (owner 2026-11-25: a crooked swing costs precision): yaw +60 (right) -> -60
        # (left), so the middle of the active frames is straight ahead; the box stretched from 10 to 24 units along the arm
        'windows': [
            (12, 13, ('horizontal', (25.0, 10.0), 60.0, -60.0), [('Swing', 4.0, (10.0, 24.0))]),
        ],
        'landing': None,
    },
    'NLAir': {
        'name': 'N.L Aerial', 'ref': 'Ganondorf neutral aerial (SSBU)', 'command': 'Command.Attack.Neutral', 'air': True,
        'total': 55, 'faf': tl(41),
        'sources': 'UFD: startup 7/17, total 40, landing lag 10, autocancel 1-3 and 37+, active (7-8/9-12)(17-18/19-26), damage '
                   '(7.0/5.25)(12.0/9.5); SW Ganondorf_(SSBU)/Neutral_aerial: hit 1 hip r5.5 55 deg 20/30, knee r5.5 70 deg 20/30, '
                   'knee+6 r4.3; hit 2 kneel r7.8 Sakurai 25/106 H x1.2, animation 55, interruptible 41',
        'hits': {'K1': (7.0, 70.0, 20, 30, 1.0, 0), 'K1Late': (5.25, 70.0, 20, 30, 1.0, 0),
                 'K2': (12.0, 'sakurai', 25, 106, 1.2, 0), 'K2Late': (9.0, 'sakurai', 25, 106, 1.2, 0)},
        # kicks: the leg is the arm, pivot at the hip; angle 0 = straight ahead, negative = down. First kick low, second higher and longer
        'windows': [
            (7, 8, ('fixed', (0.0, -20.0), -35.0), [('K1', 5.5, 0.0), ('K1', 5.5, 4.5), ('K1', 4.3, 9.0)]),
            (9, 12, ('fixed', (0.0, -20.0), -35.0), [('K1Late', 5.5, 0.0), ('K1Late', 5.5, 4.5), ('K1Late', 4.3, 9.0)]),
            (17, 18, ('fixed', (0.0, -10.0), -10.0), [('K2', 6.5, 0.0), ('K2', 7.0, 5.0), ('K2', 7.8, 10.0)]),
            (19, 26, ('fixed', (0.0, -10.0), -10.0), [('K2Late', 4.5, 0.0), ('K2Late', 5.3, 5.0), ('K2Late', 6.0, 10.0)]),
        ],
        # (first smash frame, last smash frame or None = to the end, 'auto' | landing lag frames)
        'landing': [(1, 3, 'auto'), (4, 36, 10), (37, None, 'auto')],
    },
    'NSAir': {
        'name': 'N.S Aerial', 'ref': 'Ike neutral aerial (SSBU)', 'command': 'Command.Strong.Neutral', 'air': True,
        'total': 68, 'faf': tl(60),
        'sources': 'SW Ike_(SSBU)/Neutral_aerial: clean 10-14 7.5 %, late 15-22 6.0 %, AngleIcon80, BK 35, KS 130, bone sword offsets '
                   '5 / 7 / 10.2, radius 4.0 clean, 3.0 late, autocancel 1-2 and 50+, interruptible 60, animation 68; UFD: landing lag 8',
        'hits': {'Clean': (7.5, 80.0, 35, 130, 1.0, 0), 'Late': (6.0, 80.0, 35, 130, 1.0, 0)},
        # a full circle around the body in the vertical plane: from up-front (100) forward, down under, behind and back up
        'windows': [
            (10, 14, ('vertical', (10.0, 10.0), 100.0, -80.0), [('Clean', 4.0, 5.0), ('Clean', 4.0, 7.0), ('Clean', 4.0, 10.2)]),
            (15, 22, ('vertical', (10.0, 10.0), -80.0, -260.0), [('Late', 3.0, 5.0), ('Late', 3.0, 7.0), ('Late', 3.0, 10.2)]),
        ],
        'landing': [(1, 2, 'auto'), (3, 49, 8), (50, None, 'auto')],
    },
}

# Asset names (Tools/GAS/gstn_content.py places them: Characters/Overlord/Moves/Ref)
ASSET = {'FS': 'FSGanon', 'NS': 'NSIke', 'NLAir': 'NLAirGanon', 'NSAir': 'NSAirIke'}


def arm_angle(path, i, n):
    """The arm angle at sub-frame i of n (0..n-1) of a window: linear from start to end; a 'fixed' path keeps its one angle"""
    if path[0] == 'fixed':
        return path[2]
    a0, a1 = path[2], path[3]
    return a0 if n <= 1 else a0 + (a1 - a0) * i / (n - 1)


def box_at(path, angle_deg, along):
    """(centre (x, y, z) cm in root space, rotator (pitch, yaw) deg that turns the local Z axis along the arm, half length cm).
    along: units along the arm, or (from, to) for a stretched box (a capsule)"""
    (px, pz) = path[1]
    a = math.radians(angle_deg)
    if path[0] == 'horizontal':
        d = (math.cos(a), math.sin(a), 0.0)
        rot = (-90.0, angle_deg)
    else:   # vertical and fixed: in the X-Z plane
        d = (math.cos(a), 0.0, math.sin(a))
        rot = (angle_deg - 90.0, 0.0)
    if isinstance(along, tuple):
        mid, half = (along[0] + along[1]) / 2.0, (along[1] - along[0]) / 2.0
    else:
        mid, half = along, 0.0
    m = mid * UNIT_CM
    return (px + d[0] * m, d[1] * m, pz + d[2] * m), rot, half * UNIT_CM


def window_keys(window):
    """[(hit name, radius cm, half length cm, [(frame from window start, centre, rot)])] of one window"""
    first, last, path, boxes = window
    n = last - first + 1
    out = []
    for hit, radius, along in boxes:
        keys = []
        for i in range(n):
            centre, rot, half = box_at(path, arm_angle(path, i, n), along)
            keys.append((i, centre, rot))
        out.append((hit, radius * UNIT_CM, half, keys))
    return out


def hit_numbers(move, hit):
    damage, angle, bkb, kbg, mult, priority = MOVES[move]['hits'][hit]
    return {'damage': damage, 'angle': angle, 'bkb_cms': round(bkb * UNIT_SCALE, 1), 'kbg': kbg, 'hitstop': hitlag(damage, mult),
            'priority': priority, 'penetration': PENETRATION[move]}


def table():
    rows = []
    for key, m in MOVES.items():
        for first, last, _path, boxes in m['windows']:
            hit = boxes[0][0]
            h = hit_numbers(key, hit)
            rows.append(f"{m['name']:<11} {hit:<7} f{first}-{last} (tl {tl(first)}-{tl(last)})  {h['damage']:>5}%  "
                        f"{h['angle'] if h['angle'] != 'sakurai' else 'Sakurai':>7}  BKB {h['bkb_cms']:>6} cm/s  KBG {h['kbg']:>3}  "
                        f"hitstop {h['hitstop']:>2}  pen {h['penetration']}")
    return rows


if __name__ == '__main__':
    for r in table():
        print(r)
