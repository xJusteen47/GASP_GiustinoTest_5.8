# GSTN-FALLING-MODE (owner 2026-11-25, dropkick bounce too early; owner: "extend the class", not the Mover fork): GASP's
# /Game/Blueprints/MovementModes/BP_MovementMode_Falling (a child of Mover's UFallingMode, overrides GenerateMove for the turn) is reparented
# onto UGSTN_FallingMode (GSTNCombatNP), which lets Mover's start-of-fall floor snap (a landing up to 40 cm above the floor, with no contact) run
# only on the first airborne frame (walking off a step), never later (owner 2026-11-26). Its own graph and every value set on it stay.
# One Blueprint, shared: backup first (Saved/Backups/GAS), rerun-safe (already reparented = nothing). Listed in docs/notes/gasp-divergenze.md
# (row 28). Back to stock: GSTN_FALLING_UNDO=1.
# Editor:  import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_gstn_falling_mode as s; s.run()
# Output:  Saved/DropkickBounce/setup_gstn_falling_mode.txt, at most 5 printed lines.
import os
import shutil

import unreal

BP = os.environ.get('GSTN_FALLING_BP', '/Game/Blueprints/MovementModes/BP_MovementMode_Falling')
UNDO = os.environ.get('GSTN_FALLING_UNDO') == '1'
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'DropkickBounce', 'setup_gstn_falling_mode.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[gstn_falling] {msg}')


def _backup(asset_path):
    src = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir() + asset_path[len('/Game/'):] + '.uasset')
    dst_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backups', 'GAS')
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, os.path.basename(src))
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        log(f'backup {dst}')


def run():
    del _lines[:]
    ok = False
    try:
        bp = unreal.load_asset(BP)
        if not bp:
            raise RuntimeError(f'{BP} missing')
        target = unreal.FallingMode if UNDO else unreal.GSTN_FallingMode
        current = unreal.get_default_object(bp.generated_class())
        on_gstn = isinstance(current, unreal.GSTN_FallingMode)
        if on_gstn != UNDO:
            log(f'{BP}: parent already {target.__name__}, nothing to do')
            ok = True
        else:
            _backup(BP)
            unreal.BlueprintEditorLibrary.reparent_blueprint(bp, target.static_class())
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
            cdo = unreal.get_default_object(bp.generated_class())
            if not isinstance(cdo, target):
                raise RuntimeError(f'reparent did not take: CDO is {type(cdo).__name__}')
            log(f'{BP}: reparented onto {target.__name__}, compiled, saved'
                + ('' if UNDO else f' (fall start snap: {cdo.get_editor_property("fall_start_snap")})'))
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
