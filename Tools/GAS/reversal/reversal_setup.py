"""Writes the run reversal values into DA_Locomotion_Default (skid-reversal-1). Idempotent; saves only that asset.

Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this> -nullrhi. Values from env REV_* (defaults = the values picked with
reversal_model.py and checked in game by movement_scenarios.py move_reversal). REV_ENABLED=0 turns the reversal off (0 $ way back).
Writes Saved/Reversal/reversal_setup.txt, prints max 5 lines.
"""
import os
import unreal

PROFILE = os.environ.get("REV_PROFILE", "/Game/GASNPPTest/Combat/DA_Locomotion_Default")
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Reversal", "reversal_setup.txt")
VALUES = {
    "enabled": os.environ.get("REV_ENABLED", "1") == "1",
    "stick_cos": float(os.environ.get("REV_STICK_COS", "-0.5")),
    "min_stick_magnitude": float(os.environ.get("REV_MIN_STICK", "0.5")),
    "min_speed": float(os.environ.get("REV_MIN_SPEED", "250")),
    "turning_strength": float(os.environ.get("REV_TURN", "16")),
    "acceleration_multiplier": float(os.environ.get("REV_ACCEL_MULT", "1.3")),
    "end_speed_fraction": float(os.environ.get("REV_END_FRACTION", "0.9")),
    "max_frames": int(os.environ.get("REV_MAX_FRAMES", "40")),
}

prof = unreal.load_asset(PROFILE)
rs = prof.get_editor_property("run_stages")
rev = rs.get_editor_property("reversal")
before = str(rev)
stages_before = str(rs)
for k, v in VALUES.items():
    rev.set_editor_property(k, v)
rs.set_editor_property("reversal", rev)
prof.set_editor_property("run_stages", rs)
ok = unreal.EditorAssetLibrary.save_loaded_asset(prof, only_if_is_dirty=False)
after = str(prof.get_editor_property("run_stages").get_editor_property("reversal"))
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as f:
    f.write("before %s\nafter  %s\nsaved %s\n" % (before, after, ok))
    f.write("run_stages before %s\nrun_stages after  %s\n" % (stages_before, str(prof.get_editor_property("run_stages"))))
print("REVSETUP: saved=%s %s" % (ok, after[:300]))
