# Overlord dropkick and dair: a small hop at 0 % (owner 2026-11-24, decision 2A). Data only, idempotent, saves .uasset only (never a map).
#   Before: dropkick K0 475 and DAir V9 K0 450 sat under the push back threshold (540): at 0-20 % the victim only staggered in place.
#   After: K0 = HOP_K0 (above the launch threshold 750 of knockback_smash_data; the rules must have it: setup_knockback_smash.py --rules-only) and the tumble percent of each hitbox kept where it was (KBG lowered to match),
#   so only the low-percent end changes. With the 1A flight (knocked gravity x4, launch fade 2300) the hops are ~0.4 m (dropkick, 40 deg,
#   ~1.6 m away) and ~0.7 m (dair, 75 deg). Hitboxes already stronger at 0 % and set knockback hitboxes are left alone.
#   Needs the Smash model on the rules (setup_knockback_smash.py, F2/F4). Rerunning setup_knockback_smash.py undoes this: rerun this after it.
# Kits searched: every KIT_* under /Game/GASNPPTest/Combat (the playtest kit and the Overlord kits); moves whose class starts with OVERLORD_PREFIXES.
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<proj>/Tools/GAS/setup_overlord_hop0.py [--revert]" -unattended -nullrhi
# --revert writes back the values of Saved/OverlordHop0/before.json.
# Out: Saved/OverlordHop0/setup.log (every hitbox before -> after); prints at most 5 lines.
import json
import os
import re
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import knockback_smash_data as D  # noqa: E402

DIR = '/Game/GASNPPTest/Combat'
HOP_K0 = {'GA_Overlord_Dropkick': 1100.0, 'GA_Overlord_DAir': 950.0}
OVERLORD_PREFIXES = tuple(HOP_K0)
TUMBLE = 1800.0
SAVED = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'OverlordHop0')
HIT_RE = re.compile(r'\[([\d.]+)% b([\d.]+) g([\d.]+) f(-?[\d.]+) a(-?[\d.]+)\]')
L = unreal.GSTN_TestKitContentLibrary
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[overlord_hop0] {msg}')


def _target(name):
    base = name[:-2] if name.endswith('_C') else name
    for prefix, k0 in HOP_K0.items():
        if base.startswith(prefix):
            return k0
    return None


def solve(damage, bkb, kbg, k0):
    """(bkb, kbg) with K = k0 at 0 % and the tumble percent of (bkb, kbg) kept as close as a whole KBG allows; None when already >= k0"""
    if D.k_new(bkb, kbg, damage, 0) >= k0:
        return None
    keep = D.first_percent_reaching(TUMBLE, bkb, kbg, damage)
    best = None
    for new_kbg in range(0, int(max(kbg, 1)) + 1):
        new_bkb = D.bkb_for(k0, 0, damage, new_kbg)
        tumble = D.first_percent_reaching(TUMBLE, new_bkb, new_kbg, damage)
        score = abs((tumble if tumble is not None else 300) - (keep if keep is not None else 300))
        if best is None or score < best[0]:
            best = (score, round(new_bkb), float(new_kbg))
    return best[1], best[2]


def _kits():
    out = []
    for path in unreal.EditorAssetLibrary.list_assets(DIR, recursive=False, include_folder=False):
        if path.rsplit('/', 1)[-1].split('.')[0].startswith('KIT_'):
            asset = unreal.load_asset(path)
            if asset and hasattr(asset, 'get_editor_property'):
                out.append(asset)
    return out


def _classes(kit):
    owners = [(kit, 'kit_bindings')]
    for prop in ('primary_weapon', 'secondary_weapon'):
        try:
            weapon = kit.get_editor_property(prop)
        except Exception:
            weapon = None
        if weapon:
            owners.append((weapon, 'bindings'))
    for owner, prop in owners:
        try:
            bindings = owner.get_editor_property(prop)
        except Exception:
            continue
        for b in bindings:
            cls = b.get_editor_property('ability')
            if cls and _target(cls.get_name()) is not None:
                yield cls


def _rules_launch_threshold():
    rules = unreal.load_asset('/GSTNCombat/Rules/GSTN_CombatRules_Standard')
    model = rules.get_editor_property('knockback_model') if rules else None
    return model.get_editor_property('tuning').get_editor_property('launch_threshold') if model else None


def run(revert=False):
    del _lines[:]
    launch = _rules_launch_threshold()
    if launch is not None and launch > min(HOP_K0.values()):
        print(f'WARNING: the rules launch at {launch:.0f}, above the hop K0 {min(HOP_K0.values()):.0f}: the hop is only a push back. '
              f'Run setup_knockback_smash.py --rules-only first (launch {D.LAUNCH:.0f})')
    os.makedirs(SAVED, exist_ok=True)
    before_path = os.path.join(SAVED, 'before.json')
    before = json.load(open(before_path)) if os.path.exists(before_path) else {}
    saves, done, written = [], set(), 0
    for kit in _kits():
        for cls in _classes(kit):
            name = cls.get_name()
            if name in done:
                continue
            done.add(name)
            desc = L.describe_move_knockback(cls)
            changed = False
            for i, (dmg, bkb, kbg, fixed, _angle) in enumerate(HIT_RE.findall(desc)):
                dmg, bkb, kbg, fixed = float(dmg), float(bkb), float(kbg), float(fixed)
                key = f'{name}#{i}'
                if fixed >= 0:
                    continue
                if revert:
                    if key in before:
                        changed |= L.write_move_hit_knockback(cls, i, before[key]['base'], before[key]['growth'], True)
                        written += 1
                    continue
                result = solve(dmg, bkb, kbg, _target(name))
                if result is None:
                    log(f'{kit.get_name()} {name} #{i}: K0 {D.k_new(bkb, kbg, dmg, 0):.0f} already at or above the hop: kept')
                    continue
                before.setdefault(key, {'base': bkb, 'growth': kbg})
                changed |= L.write_move_hit_knockback(cls, i, result[0], result[1], True)
                written += 1
                log(f'{kit.get_name()} {name} #{i}: b{bkb:.0f} g{kbg:.0f} (K0 {D.k_new(bkb, kbg, dmg, 0):.0f}, tumble '
                    f'{D.first_percent_reaching(TUMBLE, bkb, kbg, dmg)} %) -> b{result[0]} g{result[1]:.0f} (K0 {D.k_new(result[0], result[1], dmg, 0):.0f}, '
                    f'launch {D.first_percent_reaching(D.LAUNCH, result[0], result[1], dmg)} %, tumble {D.first_percent_reaching(TUMBLE, result[0], result[1], dmg)} %)')
            if changed:
                asset = L.get_move_timeline_asset(cls)
                saves.append(asset if asset else unreal.load_asset(cls.get_path_name().rsplit('.', 1)[0]))
    for asset in {a.get_path_name(): a for a in saves if a}.values():
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    if not revert:
        json.dump(before, open(before_path, 'w'), indent=1, sort_keys=True)
    with open(os.path.join(SAVED, 'setup.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    if not done:
        print('WARNING: no Overlord dropkick or dair bound on any kit')
    print(f'{"reverted" if revert else "written"} {written} hitboxes on {len(done)} moves, {len(saves)} assets saved -> Saved/OverlordHop0/setup.log')


if __name__ == '__main__':
    run(revert='--revert' in sys.argv)
