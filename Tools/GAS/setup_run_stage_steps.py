# Readable run stages S1 (owner 2026-11-20, decisions 1A 2A 3B; docs/notes/run-stages-leggibili-piano.md). Data only:
#  - DA_Locomotion_Default (all kits) > Run Stages > Stages[i] > Readable Step: entry burst on stage 2 and 3 (a step instead of a ramp,
#    decision 1A), the sprint 2 tell (~0.32 s) and charge (~0.1 s) on stage 2 (decision 2A). The stage 1 ramp from rest is not touched.
#  - DA_AnimSet_Locomotion_Default > Lean > Run Stages: forward lean per stage (3B: the stock sprint must read bent) and the tell's deepening.
# Starting values to judge on video, every one tunable in the assets; gstn.RunStage.Readable 0 shows the old run without touching the data.
# Idempotent: values already at the target are left alone. STEPS_MODE=dump only writes the current values.
# Headless (editor closed, GSTNCombatCore/GSTNCombatNP built with the new fields):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_run_stage_steps.py" -unattended -nullrhi
# Writes Saved/RunStages/setup_run_stage_steps.txt and prints one summary line.
import os

import unreal

PROFILE = 'DA_Locomotion_Default'
ANIM_SET = '/Game/GSTN/Animation/Layers/DA_AnimSet_Locomotion_Default'
# stage index (0 = run) -> readable step fields. Frames at 62.5 Hz. Burst multipliers sized so ~90% -> 100% of the next top speed takes the
# burst's frames with the default accelerations (stage 2: 405 -> 650 cm/s in 3 f needs ~5100 cm/s^2 = 1400 x ~4; x6 leaves margin, Mover stops at MaxSpeed)
STEPS = {
    1: {'entry_burst_frames': 3, 'entry_accel_multiplier': 6.0, 'tell_frames': 20, 'charge_frames': 6},
    2: {'entry_burst_frames': 2, 'entry_accel_multiplier': 8.0},
}
STAGE_FORWARD_LEAN = [0.0, 6.0, 8.0]
TELL_FORWARD_LEAN = 6.0
FIELDS = ('max_speed', 'acceleration', 'frames_to_next_stage', 'accel_ramp_frames', 'entry_burst_frames', 'entry_accel_multiplier',
          'tell_frames', 'charge_frames')


def _dump_stages(stages, lines, tag):
    for i, st in enumerate(stages):
        lines.append('  %s stage %d %s' % (tag, i + 1, ' '.join('%s=%s' % (k, round(st.get_editor_property(k), 3)) for k in FIELDS)))


def _profile(lines, dump_only):
    done = 0
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path('/Game', recursive=True):
        if str(data.asset_name) != PROFILE:
            continue
        asset = data.get_asset()
        if not isinstance(asset, unreal.GSTN_LocomotionProfile):
            continue
        rs = asset.get_editor_property('run_stages')
        stages = list(rs.get_editor_property('stages'))
        lines.append(str(data.package_name))
        _dump_stages(stages, lines, 'before')
        if dump_only:
            continue
        changed = False
        for i, values in STEPS.items():
            if i >= len(stages):
                lines.append('  stage %d missing, skipped' % (i + 1))
                continue
            for key, value in values.items():
                if abs(float(stages[i].get_editor_property(key)) - float(value)) > 1e-4:
                    stages[i].set_editor_property(key, value)
                    changed = True
        if changed:
            rs.set_editor_property('stages', stages)
            asset.set_editor_property('run_stages', rs)
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
            _dump_stages(stages, lines, 'after')
            done += 1
        else:
            lines.append('  readable steps already set, left alone')
    return done


def _anim_set(lines, dump_only):
    asset = unreal.EditorAssetLibrary.load_asset(ANIM_SET)
    if not asset:
        lines.append('%s not found (setup_locomotion_profile.py makes it); lean left at the C++ defaults (0 = none)' % ANIM_SET)
        return 0
    lean = asset.get_editor_property('lean')
    before = (list(lean.get_editor_property('stage_forward_lean_deg')), lean.get_editor_property('tell_forward_lean_deg'))
    lines.append('%s lean before stage_forward=%s tell=%s' % (ANIM_SET, before[0], before[1]))
    if dump_only:
        return 0
    if [round(x, 3) for x in before[0]] == STAGE_FORWARD_LEAN and abs(before[1] - TELL_FORWARD_LEAN) < 1e-4:
        lines.append('  lean already set, left alone')
        return 0
    lean.set_editor_property('stage_forward_lean_deg', STAGE_FORWARD_LEAN)
    lean.set_editor_property('tell_forward_lean_deg', TELL_FORWARD_LEAN)
    asset.set_editor_property('lean', lean)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    lines.append('  lean after stage_forward=%s tell=%s' % (STAGE_FORWARD_LEAN, TELL_FORWARD_LEAN))
    return 1


def run():
    dump_only = os.environ.get('STEPS_MODE', '') == 'dump'
    lines = []
    profiles = _profile(lines, dump_only)
    sets = _anim_set(lines, dump_only)
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'RunStages')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_run_stage_steps.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    summary = 'setup_run_stage_steps: %d profiles, %d anim sets set%s; details in Saved/RunStages/setup_run_stage_steps.txt' % (
        profiles, sets, ' (dump)' if dump_only else '')
    unreal.log(summary)
    print(summary)


if __name__ == '__main__':
    run()
