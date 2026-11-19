# Frantic run start and L3 boost dash, sim side (owner 2026-11-18, choice B, Toad reference): turns the run start stamp on in the shared locomotion
# profile DA_Locomotion_Default (all kits use it). Values = the Toad measure: ~0.33 s almost in place = 21 frames at 62.5 Hz.
# Idempotent: a profile whose stamp is already on (Frames > 0) is left alone, so the owner's tuning survives. Saves .uasset only.
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_run_start_stamp.py"
# Writes Saved/RunStages/setup_run_start_stamp.txt and prints one summary line.
import os

import unreal

NAME = 'DA_Locomotion_Default'
VALUES = {'frames': 21, 'accel_fraction': 0.05, 'stick_min': 0.5, 'max_start_speed': 60.0}
# L3 boost dash (owner 2026-11-19, Smash's dash): skips the stamp, stage 2 top speed in ~4 frames
DASH = {'frames': 6, 'accel_multiplier': 8.0}


def run():
    lines = []
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    done = 0
    for data in registry.get_assets_by_path('/Game', recursive=True):
        if str(data.asset_name) != NAME:
            continue
        asset = data.get_asset()
        if not isinstance(asset, unreal.GSTN_LocomotionProfile):
            continue
        stages = asset.get_editor_property('run_stages')
        stamp = stages.get_editor_property('start_stamp')
        if stamp.get_editor_property('frames') > 0:
            lines.append('%s: stamp already on (%d frames), left alone' % (data.package_name, stamp.get_editor_property('frames')))
            continue
        for key, value in VALUES.items():
            stamp.set_editor_property(key, value)
        stages.set_editor_property('start_stamp', stamp)
        dash = stages.get_editor_property('boost_dash')
        if dash.get_editor_property('frames') <= 0:
            for key, value in DASH.items():
                dash.set_editor_property(key, value)
            stages.set_editor_property('boost_dash', dash)
        asset.set_editor_property('run_stages', stages)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        lines.append('%s: stamp on %s' % (data.package_name, VALUES))
        done += 1
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'RunStages')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_run_start_stamp.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    summary = 'setup_run_start_stamp: %d profiles set; details in Saved/RunStages/setup_run_start_stamp.txt' % done
    unreal.log(summary)
    print(summary)


if __name__ == '__main__':
    run()
