"""GASP-CURVES (owner 2026-11-19: do our clips carry the curves GASP needs?). Lists, per clip: float curves, notifies (class/name), notify
states and sync markers, for (a) every animation in our locomotion anim set DA_AnimSet_Locomotion_Default and in /Game/GSTN/Animation,
(b) GASP reference clips (/Game/Characters/UEFN_Mannequin/Animations), aggregated by curve name. Report Saved/Routing/curves_audit.json
+ .txt summary. Headless, read only."""
import json, os
import unreal

OUT = unreal.Paths.project_saved_dir() + "Routing/curves_audit"
reg = unreal.AssetRegistryHelpers.get_asset_registry()
AL = unreal.AnimationLibrary

def seqs_under(path):
    f = unreal.ARFilter(package_paths=[path], recursive_paths=True, class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "AnimSequence")])
    return [a.get_asset() for a in reg.get_assets(f)]

def info(s):
    d = {"curves": sorted(str(n) for n in AL.get_animation_curve_names(s, unreal.RawCurveTrackTypes.RCT_FLOAT)),
         "notifies": [], "markers": sorted(set(str(m.marker_name) for m in AL.get_animation_sync_markers(s))),
         "frames": s.get_editor_property("number_of_sampled_keys") if hasattr(s, "number_of_sampled_keys") else None}
    for ev in AL.get_animation_notify_events(s):
        try:
            n = ev.get_editor_property("notify")
            ns = ev.get_editor_property("notify_state_class")
            d["notifies"].append(type(ns).__name__ if ns else (type(n).__name__ if n else "named"))
        except Exception:
            d["notifies"].append("?")
    d["notifies"] += ["name:" + str(x) for x in AL.get_animation_notify_event_names(s)]
    d["notifies"] = sorted(set(d["notifies"]))
    return d

ours = {s.get_path_name(): info(s) for s in seqs_under("/Game/GSTN/Animation")}
gasp = {s.get_path_name(): info(s) for s in seqs_under("/Game/Characters/UEFN_Mannequin/Animations")}
freq, nfreq = {}, {}
for v in gasp.values():
    for cn in v["curves"]:
        freq[cn] = freq.get(cn, 0) + 1
    for nn in v["notifies"]:
        nfreq[nn] = nfreq.get(nn, 0) + 1
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump({"ours": ours, "gasp": gasp}, open(OUT + ".json", "w"), indent=1)
lines = ["GASP clips %d, ours %d" % (len(gasp), len(ours)), "GASP curve frequency:"]
lines += ["  %4d %s" % (n, k) for k, n in sorted(freq.items(), key=lambda x: -x[1])]
lines += ["GASP notify frequency:"] + ["  %4d %s" % (n, k) for k, n in sorted(nfreq.items(), key=lambda x: -x[1])]
lines += ["OURS:"] + ["  %s curves=%d %s notifies=%s markers=%s" % (k.split(".")[-1], len(v["curves"]), ",".join(v["curves"])[:200], v["notifies"], v["markers"]) for k, v in sorted(ours.items())]
open(OUT + ".txt", "w").write("\n".join(lines))
