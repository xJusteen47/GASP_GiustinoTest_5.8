# DROPKICK-BOUNCE-WINDOW (owner 2026-11-25, option G/E: "the dropkick goes into the tech bounce too early, even when it seems it should not"):
# every combat timeline under /Game with a Tags window holding Combat.Move.KnockdownOnLanding gets that window opened at its first attack hitbox
# frame instead of frame 0 (setup_playtest_dropkick_knockdown.py / setup_dropkick_tech_bounce.py wrote [0, total)). A landing during the startup
# is then an ordinary landing. The window's end is kept. Data only, rerun-safe (a window already there is left alone), saves .uasset only.
# Uses UGSTN_DefenceContentLibrary.open_self_knockdown_window_at_first_hitbox (C++, branch claude/project-thread-pkioys): build first.
# Covers TL_Overlord_Dropkick, TL_Overlord_DropkickTB and every TB variant (R*, K5, K6...) found, nothing hand listed.
# Env: BOUNCE_WINDOW_ROOT (default /Game), BOUNCE_WINDOW_DRY=1 (log only, save nothing).
# Editor:  import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_dropkick_bounce_window as s; s.run()
# Output:  Saved/DropkickBounce/setup_dropkick_bounce_window.txt, at most 5 printed lines.
import os

import unreal

TAG = 'Combat.Move.KnockdownOnLanding'
ROOT = os.environ.get('BOUNCE_WINDOW_ROOT', '/Game')
DRY = os.environ.get('BOUNCE_WINDOW_DRY') == '1'
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'DropkickBounce', 'setup_dropkick_bounce_window.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[bounce_window] {msg}')


def _timelines():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    cls = unreal.GSTN_CombatTimeline
    path = unreal.TopLevelAssetPath(cls.static_class().get_outer().get_path_name(), cls.static_class().get_name())
    return [d for d in reg.get_assets_by_class(path, True) if str(d.package_name).startswith(ROOT)]


def run():
    del _lines[:]
    ok = False
    changed = 0
    try:
        found = _timelines()
        log(f'{len(found)} combat timelines under {ROOT}')
        for d in found:
            asset = d.get_asset()
            data = asset.get_editor_property('data') if asset else None
            if data is None or TAG not in data.export_text():
                continue
            new_data, start = unreal.GSTN_DefenceContentLibrary.open_self_knockdown_window_at_first_hitbox(data)
            name = str(d.asset_name)
            if start < 0:
                log(f'{name}: unchanged (no attack hitbox, or the window already opens with it)')
                continue
            log(f'{name}: {TAG} window now opens at frame {start} (first attack hitbox)' + (' [dry run]' if DRY else ''))
            if not DRY:
                asset.set_editor_property('data', new_data)
                unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
            changed += 1
        log(f'done: {changed} timeline(s) {"would change" if DRY else "saved"}')
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'ERROR' in l][:4] + [_lines[-1][:300] if _lines else '']:
        print(line)
    return ok


if __name__ == '__main__':
    run()
