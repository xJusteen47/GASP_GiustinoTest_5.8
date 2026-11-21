"""Read-only (runstart-phase, 2026-11-21): phase / contact_l / contact_r curve keys of the run start clips and the stock run loop
samples, to compare cadence and where each clip sits in the foot cycle at its exit point (length - 0.75 s).
Writes Saved/AnimPipeline/rows/runstart_phase_curves.json, prints 2 lines."""
import json, os
import unreal

CLIPS = ["/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r7", "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r8a",
         "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r8b", "/Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_v4r8c",
         "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Start_F_Lfoot",
         "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Relaxed_Run_Loop_F"]
OUT = os.path.join(unreal.Paths.project_saved_dir(), "AnimPipeline", "rows", "runstart_phase_curves.json")
res = {}
bs = unreal.load_asset("/Game/Characters/UEFN_Mannequin/Animations/Run/BS_Relaxed_Run_Loop_F_Slopes")
try:
    res["bs_samples"] = [str(s.animation.get_path_name()) + " @ " + str(s.sample_value) for s in bs.get_editor_property("sample_data")]
    CLIPS += [s.animation.get_path_name().split(".")[0] for s in bs.get_editor_property("sample_data")]
except Exception as e:
    res["bs_samples"] = "err %s" % e
for a in dict.fromkeys(CLIPS):
    s = unreal.load_asset(a)
    if not s:
        res[a] = "missing"; continue
    d = {"len": round(s.get_play_length(), 4)}
    for c in ("phase", "contact_l", "contact_r", "movedata_speed"):
        try:
            t, v = unreal.AnimationLibrary.get_float_keys(s, c)
            d[c] = [[round(x, 4), round(y, 4)] for x, y in zip(t, v)]
        except Exception as e:
            d[c] = "err %s" % str(e)[:80]
    res[a] = d
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(res, open(OUT, "w"), indent=0)
print("ESITO: OK clips %d" % len(res))
print(OUT)
