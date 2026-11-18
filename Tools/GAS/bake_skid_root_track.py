"""Bake the skid clip's root motion into the locomotion profile (owner choice A part 2, 2026-11-18).

Sets DA_Locomotion_Default.SkidRootMotionClip to the clip the main-graph Skid state plays, bakes RunStages.Skid.RootTrack
(UGSTN_RunSkidTrackLibrary) and saves the profile. Summary goes to Saved/Logs/ci/bake_skid_root_track.txt.
Headless: UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/GAS/bake_skid_root_track.py
"""
import os
import unreal

PROFILE = "/Game/GASNPPTest/Combat/DA_Locomotion_Default"
CLIP = "/Game/Blueprints/Animation/Ported57/SkidOwner/SS_180_Turn_Root180"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "bake_skid_root_track.txt")


def main():
    lines = []
    profile = unreal.load_asset(PROFILE)
    clip = unreal.load_asset(CLIP)
    if not profile or not clip:
        lines.append("ESITO: FAIL missing profile=%s clip=%s" % (bool(profile), bool(clip)))
        return lines
    profile.set_editor_property("skid_root_motion_clip", clip)
    ok = unreal.GSTN_RunSkidTrackLibrary.bake_profile_skid_root_track(profile)
    stages = profile.get_editor_property("run_stages")
    track = stages.get_editor_property("skid").get_editor_property("root_track")
    speed = list(track.get_editor_property("speed"))
    turn = list(track.get_editor_property("turn"))
    rate = track.get_editor_property("sample_rate")
    lines.append("bake=%s samples=%d rate=%.0f length=%.3f s yaw=%.1f sign=%.0f" % (
        ok, len(speed), rate, clip.get_play_length(), track.get_editor_property("total_yaw_deg"), track.get_editor_property("turn_sign")))
    rest = next((i for i, v in enumerate(speed) if v <= 0.0), len(speed))
    lines.append("rest sample=%d (%.3f s) entry speed=%.0f min=%.0f end=%.0f" % (rest, rest / rate, speed[0], min(speed), speed[-1]))
    for i in range(0, len(speed), 6):
        lines.append("t=%.3f speed=%7.1f turn=%.3f" % (i / rate, speed[i], turn[i]))
    saved = unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
    lines.append("ESITO: %s saved=%s" % ("OK" if ok and saved else "FAIL", saved))
    return lines


try:
    result = main()
except Exception as exc:  # the summary must say why
    result = ["ESITO: FAIL %r" % exc]
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as handle:
    handle.write("\n".join(result) + "\n")
