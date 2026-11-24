# Playtest fixes 1117b (owner playtest 2026-11-17 00:44). Data only, idempotent, saves .uasset only (never a map). Run AFTER
# setup_playtest_fixes_1117.py (and after any rerun of setup_overlord_jab_kit.py). Writes Saved/PlaytestFixes/fixes_1117b.txt, prints <= 5 lines.
#  1. "After crouching, jab no longer works": WPN_Test_Overlord bound Command.Attack.CrouchNeutral (grounded) to GSTN_GA_TestFeetProbe, a
#     ~150 frame test probe with no animation copied in from the test weapons. Crouch + X started it and every jab was refused until it
#     ended (PIE diag, Saved/PF1117b/diag/crouch.jsonl: timeline 5 from frame 81 to 234, taps at 140 and 200 refused). The test-only
#     probe binding leaves WPN_Test_Overlord and WPN_Test_OverlordDropkick (KIT_Test_Playtest's weapon): crouch + X on the ground now does nothing until a down tilt exists.
#  2. "Back + attack in the air must flip at once": the dropkick (the Overlord's directional aerial) aims SnapToStick, so its first frame
#     turns straight to the stick (FGSTN_StartupSteer::ResolveOriginAim) instead of keeping the facing the air turn was easing.
#  3. (NOT RUN, see run()) Playtest camera farther back: a Follow definition (CMD_Playtest_Follow, arm FOLLOW_DISTANCE) and a rule set using it as the default,
#     set as RuleSetOverride on the test character's GSTN camera component (Blueprint backed up to Saved/Backups/PF1117b first).
import os
import shutil
import time

import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJ, 'Saved', 'PlaytestFixes', 'fixes_1117b.txt')
WEAPONS = ['/Game/GSTN/Characters/Overlord/Kits/WPN_Test_Overlord', '/Game/GSTN/Characters/Overlord/Kits/WPN_Test_OverlordDropkick']   # the 2nd is KIT_Test_Playtest's
PROBE = 'GSTN_GA_TestFeetProbe'
DROPKICK_TL = '/Game/GSTN/Characters/Overlord/Moves/Dropkick/TL_Overlord_Dropkick'
CAM_DIR = '/Game/GSTN/Data/Camera'
CAM_DEF = CAM_DIR + '/CMD_Playtest_Follow'
CAM_RULES = CAM_DIR + '/CRS_Playtest'
CHARACTER = '/Game/GSTN/Core/BP_GASNPP_TestCharacter'
FOLLOW_DISTANCE = 1100.0   # was 650 (plugin default)
FOLLOW_PITCH = -22.0
FOLLOW_FOV = 60.0
_lines = []


def log(m):
    _lines.append(m)


def _weapon():
    for path in WEAPONS:
        w = unreal.load_asset(path)
        keep, dropped = [], []
        for b in w.get_editor_property('bindings'):
            a = b.get_editor_property('ability')
            if a and a.get_name().startswith(PROBE):
                dropped.append(f"{b.get_editor_property('command').get_editor_property('tag_name')}->{a.get_name()}")
            else:
                keep.append(b)
        if dropped:
            w.set_editor_property('bindings', keep)
            unreal.EditorAssetLibrary.save_loaded_asset(w, only_if_is_dirty=False)
        log(f'{path.rsplit("/", 1)[1]}: removed {dropped or "nothing"}; {len(keep)} bindings')


def _dropkick():
    tl = unreal.load_asset(DROPKICK_TL)
    data = tl.get_editor_property('data')
    before = data.get_editor_property('aim_policy')
    data.set_editor_property('aim_policy', unreal.GSTN_AimPolicy.SNAP_TO_STICK)
    data.set_editor_property('max_turn_degrees', 180.0)
    tl.set_editor_property('data', data)
    unreal.EditorAssetLibrary.save_loaded_asset(tl, only_if_is_dirty=False)
    log(f'TL_Overlord_Dropkick: aim {before} -> SnapToStick (ignore move input {data.get_editor_property("ignore_move_input")})')


def _asset(path, cls):
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    folder, name = path.rsplit('/', 1)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, unreal.DataAssetFactory())


def _camera():
    d = _asset(CAM_DEF, unreal.GSTN_CameraModeDefinition)
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="Camera.Mode.Follow")')
    d.set_editor_property('mode_tag', tag)
    mode = d.get_editor_property('mode')
    if not mode or not isinstance(mode, unreal.GSTN_CameraMode_Follow):
        mode = unreal.new_object(unreal.GSTN_CameraMode_Follow, d)
    tuning = mode.get_editor_property('tuning')
    tuning.set_editor_property('distance_cm', FOLLOW_DISTANCE)
    tuning.set_editor_property('pitch_deg', FOLLOW_PITCH)
    tuning.set_editor_property('fov_deg', FOLLOW_FOV)
    mode.set_editor_property('tuning', tuning)
    d.set_editor_property('mode', mode)
    unreal.EditorAssetLibrary.save_loaded_asset(d, only_if_is_dirty=False)
    rs = _asset(CAM_RULES, unreal.GSTN_CameraRuleSet)
    rule = unreal.GSTN_CameraModeRule()
    rule.set_editor_property('mode', d)
    rule.set_editor_property('priority', 0)
    rs.set_editor_property('rules', [rule])
    unreal.EditorAssetLibrary.save_loaded_asset(rs, only_if_is_dirty=False)
    # The test character's GSTN camera component: RuleSetOverride (soft pointer)
    src = os.path.join(PROJ, 'Content', 'GASNPPTest', 'BP_GASNPP_TestCharacter.uasset')
    bak = os.path.join(PROJ, 'Saved', 'Backups', 'PF1117b', time.strftime('%Y%m%d-%H%M%S'))
    os.makedirs(bak, exist_ok=True)
    shutil.copy2(src, bak)
    bp = unreal.load_asset(CHARACTER)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    done = 0
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        o = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
        if o and isinstance(o, unreal.GSTN_CameraComponent):
            o.set_editor_property('rule_set_override', rs)
            done += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False)
    log(f'camera: {CAM_DEF.rsplit("/", 1)[1]} arm {FOLLOW_DISTANCE:.0f} pitch {FOLLOW_PITCH:.0f} fov {FOLLOW_FOV:.0f}; rule set on {done} component(s); BP backup {bak}')


def run():
    for step in (_weapon, _dropkick):   # _camera kept for reference, NOT run: the play view is GASP's CameraAsset_SandboxCharacter, not the GSTN camera
        try:
            step()
        except Exception as e:
            log(f'ERROR in {step.__name__}: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in _lines[-5:]:
        print(line[:300])


run()
