"""T4 (2026-11-24): anim_layer equivalence on the w3 code at 22a318f1 (c1 passo-02 = arm_r out, the last step there).
ref = 22a318f1 builder as is (ARM_R_OUT=10); src = same builder with W3_ARM_R_OUT=0; lay = src + STEP_C1_02 via anim_layer_ue.
Dumps ref/lay joints, writes the comparison to the lifecal worktree, then deletes every temp asset/file it made in _ue59."""
import json, math, os, runpy, shutil, sys, traceback
import unreal

S = os.path.dirname(os.path.abspath(__file__))
WT = r"C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_lifecal"
MCP = os.path.join(WT, "Tools", "AnimPipeline", "gstn_anim_mcp")
OUTD = os.path.join(WT, "Saved", "AnimPipeline", "layers")
os.makedirs(OUTD, exist_ok=True)
EAL = unreal.EditorAssetLibrary
D = "/Game/_T4tmp"
SRC, REF, LAY = D + "/AS_T4_w3src", D + "/AS_T4_w3ref", D + "/AS_T4_w3lay"
PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
RS = os.path.join(PROJ, "Saved", "AnimPipeline", "runstart")
res = {"ok": False}
made_files = []
try:
    os.environ["GSTN_SAVE"] = "1"
    for dst, arm in ((SRC, "0"), (REF, "10.0")):
        os.environ["W3_DST"], os.environ["W3_ARM_R_OUT"] = dst, arm
        os.environ["W3_OUT"] = os.path.join(OUTD, "t4_build_%s.json" % dst.rsplit("/", 1)[-1])
        runpy.run_path(os.path.join(S, "w3_22a318f1.py"), run_name="__main__")
    sys.path.insert(0, os.path.join(WT, "Tools", "AnimPipeline", "tests"))
    sys.path.insert(0, MCP)
    from test_anim_layer import STEP_C1_02
    ip = os.path.join(OUTD, "t4_stack.json")
    json.dump({"src": SRC, "dst": LAY, "layers": STEP_C1_02}, open(ip, "w"), indent=1)
    os.environ.update({"GSTN_MCP_DIR": MCP, "GSTN_INTENT_JSON": ip, "GSTN_SAVE": "1",
                       "GSTN_LAYER_REPORT": os.path.join(OUTD, "t4_layer_report.json")})
    os.environ.pop("GSTN_LAYER_DUMP", None)
    try:
        runpy.run_path(os.path.join(MCP, "anim_layer_ue.py"), run_name="__main__")
    except SystemExit as e:
        res["layer_exit"] = e.code
    os.environ["CJD_ASSETS"] = ",".join((REF, LAY, SRC))
    os.environ["CJD_FACING"] = "x"
    runpy.run_path(os.path.join(MCP, "clip_joints_dump_ue.py"), run_name="__main__")
    J = {}
    for p in (REF, LAY, SRC):
        n = p.rsplit("/", 1)[-1]
        f = os.path.join(RS, "joints_%s.json" % n)
        made_files.append(f)
        J[n] = json.load(open(f))["joints"]
        shutil.copy(f, os.path.join(OUTD, os.path.basename(f)))

    def cmp(a, b):
        per, worst = {}, (0.0, None, None)
        for k in a:
            if k in b:
                ds = [math.dist(a[k][i], b[k][i]) for i in range(min(len(a[k]), len(b[k])))]
                i = max(range(len(ds)), key=lambda j: ds[j])
                per[k] = round(ds[i], 3)
                if ds[i] > worst[0]:
                    worst = (round(ds[i], 3), k, i)
        return {"max_cm": worst[0], "joint": worst[1], "frame": worst[2], "per_joint": per}
    res["lay_vs_ref"] = cmp(J["AS_T4_w3ref"], J["AS_T4_w3lay"])
    res["src_vs_ref"] = cmp(J["AS_T4_w3ref"], J["AS_T4_w3src"])     # size of the step itself, for scale
    res["ok"] = True
except Exception:
    res["error"] = traceback.format_exc()[-1500:]
finally:
    for a in (LAY, REF, SRC):
        if EAL.does_asset_exist(a):
            res.setdefault("deleted", []).append([a, bool(EAL.delete_asset(a))])
    try:
        EAL.delete_directory(D)
    except Exception:
        pass
    for f in made_files:
        try:
            os.remove(f)
        except OSError:
            pass
    json.dump(res, open(os.path.join(OUTD, "t4_layer_compare.json"), "w"), indent=1)
    print("T4LAYER ok=%s lay_vs_ref=%s src_vs_ref=%s" % (res["ok"], (res.get("lay_vs_ref") or {}).get("max_cm"),
                                                       (res.get("src_vs_ref") or {}).get("max_cm")))
