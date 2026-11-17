# F2 (owner 2026-11-17, decision 2B): the knockback of the standard rules moves from the weight driven model (one percent growth, 1.8, for every
# move) to the full Smash formula, where every move has its own base (BKB) and growth (KBG). Pure Python, no `unreal`: the formulas, the targets
# of the retune and the move list, shared by the conversion table (run this file) and Tools/GAS/setup_knockback_smash.py (the editor script).
#
#   python3 Tools/GAS/knockback_smash_data.py      -> docs/notes/reviews/feel-f2/knockback_table.csv (+ at most 5 printed lines)
#
# THE FORMULA (UGSTN_KnockbackModel_PercentScaling with bBaseKnockbackInCmPerSecond, GSTN_KnockbackModel.cpp):
#   P = percent BEFORE the hit + this hit's damage   (the percent AFTER the hit, as in Smash)
#   K = ((P/10 + P*d/20) * 200/(w+100) * 1.4 + 18) * KBG/100 * UnitScale + BKB        [cm/s; BKB is authored in cm/s]
# Same K as Smash's ((...)*KBG/100 + BKB_smash) * UnitScale with BKB = BKB_smash * UnitScale. UnitScale 22.5: Smash tumbles at 80 units, the
# rules tumble at 1800 cm/s (80 * 22.5 = 1800), so the thresholds (push back 540, launch 750, tumble 1800) stay where they are.
# THE OLD MODEL (UGSTN_KnockbackModel_WeightDriven as the standard rules asset has it today):
#   K = base * (1 + 1.8 * p/100) * 100 / (w * curve),   p = percent BEFORE the hit, curve = 1 -> 0.6 over p/MaxHealth (MaxHealth assumed 100).
import csv
import math
import os

UNIT_SCALE = 22.5
PUSH_BACK, LAUNCH, TUMBLE = 540.0, 750.0, 1800.0
OLD_PERCENT_GROWTH = 1.8
OLD_CURVE = (1.0, 0.6)
OLD_MAX_HEALTH = 100.0          # the rules asset's MaxHealth is not readable from the cloud: 100 is the class default (assumption)
PERCENTS = (0, 40, 100, 150)
WEIGHTS = (80, 100, 140)        # the archetype kits' weights (light, medium, heavy); 100 = the dummy and the reference
TILT_PRESETS = ('hop0', 'smash')


def smash_term(percent_before, damage, weight=100.0):
    """The bracket of the Smash formula before KBG: ((P/10 + P*d/20) * 200/(w+100) * 1.4 + 18), P = percent after the hit"""
    p = max(percent_before, 0.0) + damage
    return (p / 10.0 + p * damage / 20.0) * 200.0 / (max(weight, 1.0) + 100.0) * 1.4 + 18.0


def k_new(bkb, kbg, damage, percent_before, weight=100.0, unit_scale=UNIT_SCALE):
    return smash_term(percent_before, damage, weight) * kbg / 100.0 * unit_scale + bkb


def k_old(base, percent_before, weight=100.0, growth=OLD_PERCENT_GROWTH, curve=OLD_CURVE, max_health=OLD_MAX_HEALTH):
    progress = min(max(percent_before / max_health, 0.0), 1.0)
    scale = curve[0] + (curve[1] - curve[0]) * progress
    return base * (1.0 + growth * max(percent_before, 0.0) / 100.0) * 100.0 / max(weight * scale, 1.0)


def bkb_for(k_target, percent_target, damage, kbg):
    """The BKB (cm/s) that gives K = k_target at percent_target (before the hit) on a weight-100 victim; never below 0"""
    return max(k_target - smash_term(percent_target, damage) * kbg / 100.0 * UNIT_SCALE, 0.0)


def first_percent_reaching(k_target, bkb, kbg, damage, weight=100.0, limit=300):
    """The first whole percent at which K >= k_target, None if not before limit"""
    for p in range(0, limit + 1):
        if k_new(bkb, kbg, damage, p, weight) >= k_target:
            return p
    return None


def first_percent_reaching_old(k_target, base, weight=100.0, limit=300):
    for p in range(0, limit + 1):
        if k_old(base, p, weight) >= k_target:
            return p
    return None


# ---- Roles: how a move's BKB is chosen. Every role names its KBG (combo <= 60, KO >= 100). An anchor fixes K at one percent:
#   ('at', K, percent)  K reaches K at that percent (before the hit) on weight 100: jab 'launch at 80 %', strong 'tumble at 40 %'
#   ('keep0',)          K at 0 % stays what it is today (the old base): the move feels the same at 0 % and gains its own growth after
def role_spec(role, kit, tilt_preset='hop0'):
    """(kbg, anchor) of a role in a kit"""
    if role == 'jab':        # owner 2B targets: the jab does not launch before ~80 %. Heavier hits need less growth to get there
        return {'Light': 40, 'Medium': 30, 'Heavy': 20}.get(kit, 30), ('at', LAUNCH, 80)
    if role == 'tilt':       # hop0 = the playtest of 2026-11-17 (owner: 30 deg, launches at 0 % as a low hop); smash = launch from 19 / 11 / 4 %
        kbg = {'Light': 90, 'Medium': 70, 'Heavy': 70}.get(kit, 70)   # low damage needs more growth to reach tumble
        if tilt_preset == 'smash':
            return kbg, ('at', LAUNCH, {'Light': 19, 'Medium': 11, 'Heavy': 4}.get(kit, 11))
        return kbg, ('keep0',)
    if role == 'special':    # light pop-up = combo starter, heavy slam = KO
        return {'Light': 60, 'Medium': 80, 'Heavy': 100}.get(kit, 80), ('keep0',)
    if role == 'strong':     # owner 2B targets: tumble from ~40 %
        return 100, ('at', TUMBLE, 40)
    if role == 'ground_hit':  # playtest X on the ground: launches at 0 % at 30 deg as a hop (owner 2026-11-17), combo growth
        return 60, ('keep0',)
    if role.startswith('throw_'):
        d = role[6:]
        kbg = {'Forward': 60, 'Back': 80, 'Up': 70, 'Down': 40}[d]
        if kit == 'Heavy' and d == 'Back':
            kbg = 100
        return kbg, ('keep0',)
    if role == 'overlord':   # NOT approved knockback (owner scope: list them). Same K at 0 %, growth per move in OVERLORD_KBG
        return None, ('keep0',)
    return 70, ('keep0',)    # anything found on a kit and not listed: same K at 0 %, middle growth


OVERLORD_KBG = {'GA_Overlord_Jab1': 40, 'GA_Overlord_Jab2': 50, 'GA_Overlord_Dropkick': 80, 'GA_Overlord_DAir': 70}


def overlord_kbg(name):
    for prefix, kbg in OVERLORD_KBG.items():
        if name.startswith(prefix):
            return kbg
    return 70


def convert(role, kit, damage, old_base, tilt_preset='hop0', name=''):
    """(bkb, kbg) of one hitbox: the role's KBG, the BKB from its anchor"""
    kbg, anchor = role_spec(role, kit, tilt_preset)
    if kbg is None:
        kbg = overlord_kbg(name)
    # Rounded up to the whole cm/s, so the anchor is met at its percent (a round down would move 'tumble at 40 %' to 41 %)
    if anchor[0] == 'at':
        return math.ceil(bkb_for(anchor[1], anchor[2], damage, kbg) - 1e-9), kbg
    return math.ceil(bkb_for(old_base, 0, damage, kbg) - 1e-9), kbg


# ---- The moves of the kits, as the setup scripts leave them today (old base = after setup_playtest_launch_1117.py):
# (group, kit, ability class name, role, damage, old base, angle, source)
MOVES = [
    ('Light', 'Light', 'GA_Light_Jab', 'jab', 3.0, 300.0, 30, 'setup_test_kit_archetypes.py'),
    ('Light', 'Light', 'GA_Light_Tilt', 'tilt', 5.0, 780.0, 30, 'setup_playtest_launch_1117.py'),
    ('Light', 'Light', 'GA_Light_Special', 'special', 5.0, 780.0, 80, 'setup_playtest_launch_1117.py'),
    ('Light', 'Light', 'GA_Playtest_Strong', 'strong', 14.0, 1100.0, 30, 'setup_playtest_launch_1117.py (shared by L/M/H)'),
    ('Light', 'Light', 'GA_Light_ThrowForward', 'throw_Forward', 5.0, 1300.0, 20, 'setup_test_kit_archetypes.py'),
    ('Light', 'Light', 'GA_Light_ThrowBack', 'throw_Back', 6.0, 1500.0, 160, 'setup_test_kit_archetypes.py'),
    ('Light', 'Light', 'GA_Light_ThrowUp', 'throw_Up', 4.0, 1500.0, 90, 'setup_test_kit_archetypes.py'),
    ('Light', 'Light', 'GA_Light_ThrowDown', 'throw_Down', 4.0, 700.0, 70, 'setup_test_kit_archetypes.py'),
    ('Medium', 'Medium', 'GA_Medium_Jab', 'jab', 7.0, 550.0, 35, 'setup_test_kit_archetypes.py'),
    ('Medium', 'Medium', 'GA_Medium_Tilt', 'tilt', 8.0, 780.0, 30, 'setup_playtest_launch_1117.py'),
    ('Medium', 'Medium', 'GA_Medium_Special', 'special', 10.0, 850.0, 30, 'setup_playtest_launch_1117.py'),
    ('Medium', 'Medium', 'GA_Playtest_Strong', 'strong', 14.0, 1100.0, 30, 'shared'),
    ('Medium', 'Medium', 'GA_Medium_ThrowForward', 'throw_Forward', 8.0, 1700.0, 45, 'setup_test_kit_archetypes.py'),
    ('Medium', 'Medium', 'GA_Medium_ThrowBack', 'throw_Back', 10.0, 1900.0, 135, 'setup_test_kit_archetypes.py'),
    ('Medium', 'Medium', 'GA_Medium_ThrowUp', 'throw_Up', 7.0, 1800.0, 85, 'setup_test_kit_archetypes.py'),
    ('Medium', 'Medium', 'GA_Medium_ThrowDown', 'throw_Down', 6.0, 1000.0, 75, 'setup_test_kit_archetypes.py'),
    ('Heavy', 'Heavy', 'GA_Heavy_Jab', 'jab', 14.0, 900.0, 40, 'setup_test_kit_archetypes.py'),
    ('Heavy', 'Heavy', 'GA_Heavy_Tilt', 'tilt', 12.0, 780.0, 30, 'setup_playtest_launch_1117.py'),
    ('Heavy', 'Heavy', 'GA_Heavy_Special', 'special', 18.0, 1200.0, 60, 'setup_playtest_launch_1117.py'),
    ('Heavy', 'Heavy', 'GA_Playtest_Strong', 'strong', 14.0, 1100.0, 30, 'shared'),
    ('Heavy', 'Heavy', 'GA_Heavy_ThrowForward', 'throw_Forward', 13.0, 2300.0, 55, 'setup_test_kit_archetypes.py'),
    ('Heavy', 'Heavy', 'GA_Heavy_ThrowBack', 'throw_Back', 15.0, 2500.0, 120, 'setup_test_kit_archetypes.py'),
    ('Heavy', 'Heavy', 'GA_Heavy_ThrowUp', 'throw_Up', 11.0, 2600.0, 89, 'setup_test_kit_archetypes.py'),
    ('Heavy', 'Heavy', 'GA_Heavy_ThrowDown', 'throw_Down', 10.0, 1300.0, 85, 'setup_test_kit_archetypes.py'),
    ('Playtest', 'Playtest', 'GA_Playtest_GroundHit', 'ground_hit', 8.0, 780.0, 30, 'setup_playtest_launch_1117.py (root hitbox; other hitboxes keep K at 0 % too)'),
    # Overlord moves on the playtest kit / their test kits: NOT approved knockback, listed for the owner (scope of 2B). Same K at 0 %
    ('Overlord', 'Overlord', 'GA_Overlord_Jab1', 'overlord', 3.0, 150.0, 30, 'setup_overlord_jab_kit.py'),
    ('Overlord', 'Overlord', 'GA_Overlord_Jab2', 'overlord', 7.0, 550.0, 35, 'setup_overlord_jab_kit.py'),
    ('Overlord', 'Overlord', 'GA_Overlord_Dropkick', 'overlord', 10.0, 400.0, 40, 'setup_overlord_dropkick_kit.py'),
    ('Overlord', 'Overlord', 'GA_Overlord_DAirV9', 'overlord', 11.0, 450.0, 75, 'setup_overlord_dair_kit.py'),
    # Example impact moves (KIT_Example_Impacts), not on the four kits: converted too so they do not turn flat
    ('Examples', 'Examples', 'GA_Example_GroundBounce', 'other', 12.0, 1900.0, -45, 'setup_launch_impacts.py'),
    ('Examples', 'Examples', 'GA_Example_WallSplat', 'other', 12.0, 2000.0, 15, 'setup_launch_impacts.py'),
]


def tier(k):
    return 'tumble' if k >= TUMBLE else 'launch' if k >= LAUNCH else 'push' if k >= PUSH_BACK else 'stagger'


def table_rows(tilt_preset='hop0'):
    rows = []
    for group, kit, name, role, damage, base, angle, source in MOVES:
        bkb, kbg = convert(role, kit, damage, base, tilt_preset, name)
        row = {'group': group, 'move': name, 'role': role, 'damage': damage, 'angle': angle, 'old_base': base, 'new_bkb_cms': bkb,
               'new_kbg': kbg, 'class': 'KO' if kbg >= 100 else 'combo' if kbg <= 60 else 'mid'}
        for p in PERCENTS:
            row[f'old_K{p}'] = round(k_old(base, p))
            row[f'new_K{p}'] = round(k_new(bkb, kbg, damage, p))
            row[f'new_tier{p}'] = tier(row[f'new_K{p}'])
        row['old_launch_from'] = first_percent_reaching_old(LAUNCH, base)
        row['new_launch_from'] = first_percent_reaching(LAUNCH, bkb, kbg, damage)
        row['old_tumble_from'] = first_percent_reaching_old(TUMBLE, base)
        row['new_tumble_from'] = first_percent_reaching(TUMBLE, bkb, kbg, damage)
        for w in WEIGHTS:
            row[f'new_K100_w{w}'] = round(k_new(bkb, kbg, damage, 100, w))
            row[f'old_K100_w{w}'] = round(k_old(base, 100, w))
        row['source'] = source
        rows.append(row)
    return rows


def write_table(path, tilt_preset='hop0'):
    rows = table_rows(tilt_preset)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        w.writeheader()
        w.writerows(rows)
    return rows


if __name__ == '__main__':
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    out_dir = os.path.join(root, 'docs', 'notes', 'reviews', 'feel-f2')
    rows = write_table(os.path.join(out_dir, 'knockback_table.csv'), 'hop0')
    alt = write_table(os.path.join(out_dir, 'knockback_table_tilts_smash.csv'), 'smash')
    print(f'{len(rows)} moves -> docs/notes/reviews/feel-f2/knockback_table.csv (tilts hop0) and knockback_table_tilts_smash.csv')
    jabs = [r for r in rows if r['role'] == 'jab']
    print('jab launch from: ' + ', '.join(f"{r['move']} {r['new_launch_from']}%" for r in jabs))
    print('strong tumble from: %s%%; tilts (smash preset) launch from: %s' % (
        next(r['new_tumble_from'] for r in rows if r['role'] == 'strong'),
        ', '.join(f"{r['move']} {r['new_launch_from']}%" for r in alt if r['role'] == 'tilt')))
