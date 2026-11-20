# Gradual run start (owner 2026-11-20, Toad reference: "takes a while to get going" = gradual acceleration, not a standstill).
# Data only, shared profile DA_Locomotion_Default (all kits). Keeps the 0.33 s stamp (identity trait) and replaces the two
# acceleration steps after it (stamp -> stage 1 top in ~0.23 s, stage 1 -> 2 in ~0.2 s) with linear acceleration ramps
# (FGSTN_RunStage AccelRampFrames / RampStartFraction, existing fields; the stage 1 ramp starts at the end of the stamp).
# Measure + target: docs/notes/runstart-accel/report.md. Hand tuning: DA_Locomotion_Default > Run Stages > Stages[i] >
# Acceleration Ramp (frames at 62.5 Hz). Idempotent: a stage whose ramp is already on (AccelRampFrames > 0) is left alone.
# RAMP_MODE=dump only writes the current values. Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_run_start_ramp.py"
# Writes Saved/RunStages/setup_run_start_ramp.txt and prints one summary line.
import os

import unreal

NAME = 'DA_Locomotion_Default'
# stage index -> (AccelRampFrames, RampStartFraction). Toad: ~0.33 s in place, then ~0.6 s of growing speed, no step.
RAMPS = {0: (31, 0.15), 1: (30, 0.25)}
FIELDS = ('max_speed', 'acceleration', 'frames_to_next_stage', 'next_stage_speed_fraction', 'accel_ramp_frames', 'ramp_start_fraction')


def _dump(stages, lines, tag):
    stamp = stages.get_editor_property('start_stamp')
    lines.append('  %s stamp frames=%d accel_fraction=%.3f max_start_speed=%.0f' % (
        tag, stamp.get_editor_property('frames'), stamp.get_editor_property('accel_fraction'), stamp.get_editor_property('max_start_speed')))
    for i, st in enumerate(stages.get_editor_property('stages')):
        lines.append('  %s stage %d %s' % (tag, i + 1, ' '.join('%s=%s' % (k, round(st.get_editor_property(k), 3)) for k in FIELDS)))


def run():
    dump_only = os.environ.get('RAMP_MODE', '') == 'dump'
    lines, done = [], 0
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path('/Game', recursive=True):
        if str(data.asset_name) != NAME:
            continue
        asset = data.get_asset()
        if not isinstance(asset, unreal.GSTN_LocomotionProfile):
            continue
        rs = asset.get_editor_property('run_stages')
        lines.append(str(data.package_name))
        _dump(rs, lines, 'before')
        if dump_only:
            continue
        stages, changed = list(rs.get_editor_property('stages')), False
        for i, (frames, start) in RAMPS.items():
            if i >= len(stages) or stages[i].get_editor_property('accel_ramp_frames') > 0:
                continue
            stages[i].set_editor_property('accel_ramp_frames', frames)
            stages[i].set_editor_property('ramp_start_fraction', start)
            changed = True
        if changed:
            rs.set_editor_property('stages', stages)
            asset.set_editor_property('run_stages', rs)
            unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
            _dump(rs, lines, 'after')
            done += 1
        else:
            lines.append('  ramps already on, left alone')
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'RunStages')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_run_start_ramp.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    summary = 'setup_run_start_ramp: %d profiles set%s; details in Saved/RunStages/setup_run_start_ramp.txt' % (done, ' (dump)' if dump_only else '')
    unreal.log(summary)
    print(summary)


if __name__ == '__main__':
    run()
