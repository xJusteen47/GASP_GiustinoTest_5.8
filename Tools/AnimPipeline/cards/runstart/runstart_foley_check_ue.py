"""Read-only check of the foley notifies on the generic run start clips (runstart-sprinter-s2, 2026-11-21): track, time, class, side,
event tag, threshold. Writes Saved/AnimPipeline/runstart/foley_check.json, prints 2 lines."""
import json, os, unreal
AL = unreal.AnimationLibrary
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
out = {}
for p in ("/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_g3", "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_s2",
          "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Loop_F"):
    s = unreal.load_asset(p); d = {}
    for tr in ("Footstep Left", "Footstep Right"):
        d[tr] = []
        for e in AL.get_animation_notify_events_for_track(s, tr):
            n = e.get_editor_property("notify")
            d[tr].append([round(AL.get_anim_notify_event_trigger_time(e) * 30, 1), n.get_class().get_name(), str(n.get_editor_property("side")),
                          str(n.get_editor_property("event").get_editor_property("tag_name")), e.get_editor_property("trigger_weight_threshold")])
    out[p.split("/")[-1]] = d
json.dump(out, open(os.path.join(PROJ, "Saved", "AnimPipeline", "runstart", "foley_check.json"), "w"), indent=1)
for k, v in out.items():
    print("FOLEYCHK %s L %d R %d" % (k, len(v["Footstep Left"]), len(v["Footstep Right"])))
