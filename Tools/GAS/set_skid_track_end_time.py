"""Set DA_Locomotion_Default's skid RootTrack.EndTime (skid attempt 2: hand over to the run before the owner clip's slow tail).
Env GSTN_SKID_END_TIME (seconds, default 0.7). Summary: Saved/Logs/ci/set_skid_track_end_time.txt"""
import os
import unreal

PROFILE = "/Game/GASNPPTest/Combat/DA_Locomotion_Default"
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "set_skid_track_end_time.txt")

try:
    end_time = float(os.environ.get("GSTN_SKID_END_TIME", "0.7"))
    profile = unreal.load_asset(PROFILE)
    stages = profile.get_editor_property("run_stages")
    skid = stages.get_editor_property("skid")
    track = skid.get_editor_property("root_track")
    track.set_editor_property("end_time", end_time)
    skid.set_editor_property("root_track", track)
    stages.set_editor_property("skid", skid)
    profile.set_editor_property("run_stages", stages)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False)
    check = profile.get_editor_property("run_stages").get_editor_property("skid").get_editor_property("root_track")
    result = "ESITO: %s end_time=%.3f samples=%d saved=%s" % ("OK" if saved else "FAIL", check.get_editor_property("end_time"),
                                                            len(check.get_editor_property("speed")), saved)
except Exception as exc:
    result = "ESITO: FAIL %r" % exc
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as handle:
    handle.write(result + "\n")
