# Downed state with an end (playtest 2026-11-17: dummies lay forever after a knockdown, no get-up was ever seen). The defence rules the test
# modes use get GroundBounce.MaxDownedFrames = 90 (1.5 s, Smash-like): with nothing pressed the neutral get-up plays on the last downed frame
# (C++ FGSTN_TumbleLanding::AfterClocks). A roll (stick) or the neutral get-up (jump) before that works as before. Rerun-safe.
# Out: Saved/PlaytestFixes/downed_1117.txt
import os
import unreal

FRAMES = 90
RULES = ['/GSTNDefence/Test/DA_DefenceRules_Test', '/GSTNDefence/Test/DA_DefenceRules_TestPvE']
_PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(_PROJ, 'Saved', 'PlaytestFixes', 'downed_1117.txt')


def run():
    lines = []
    combat = unreal.load_asset('/GSTNCombat/Rules/GSTN_CombatRules_Standard')
    try:
        lines.append('combat rules extensions: ' + combat.get_editor_property('rules_extensions').__str__()[:300])
    except Exception as e:
        lines.append('combat rules extensions n/a ' + repr(e)[:80])
    for path in RULES:
        r = unreal.load_asset(path)
        if not r:
            lines.append(f'{path}: missing')
            continue
        gb = r.get_editor_property('ground_bounce')
        before = gb.get_editor_property('max_downed_frames')
        gb.set_editor_property('max_downed_frames', FRAMES)
        r.set_editor_property('ground_bounce', gb)
        unreal.EditorAssetLibrary.save_loaded_asset(r, False)
        lines.append(f'{path}: MaxDownedFrames {before} -> {FRAMES}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w').write('\n'.join(lines) + '\n')
    print(lines[-1])


if __name__ == '__main__':
    run()
