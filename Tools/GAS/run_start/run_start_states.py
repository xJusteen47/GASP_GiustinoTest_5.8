"""Frantic run start in ABP_GSTN_Locomotion (run-start-frantic-2, handover run-start-frantic-1). Headless, editor closed, _ue59 lock:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="<project>/Tools/GAS/run_start/run_start_states.py" -unattended -nullrhi
  1. imports docs/notes/reviews/run-start-frantic/r3/AS_RunStart_Frantic_r3.fbx as /Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Frantic
     (UEFN mannequin skeleton, 30 fps as authored; skipped when the asset exists and RUNSTART_REIMPORT is not 1)
  2. state "Run Start" + alias "-> Run Start" (GASP idle states), enter when GetAnimOwnerRunStartTime >= 0, leave to Conduit when it is
     < 0 or past the clip's length. The state is routed by route_states.py (Blend Stack row "Run Start", plays at the clip's own rate).
Report -> Saved/RunStart/run_start_states.txt, prints max 5 lines."""
import os
import unreal

PROJ = unreal.Paths.project_dir()
OUT = unreal.Paths.project_saved_dir() + "RunStart/run_start_states.txt"
FBX = PROJ + "docs/notes/reviews/run-start-frantic/r3/AS_RunStart_Frantic_r3.fbx"
FOLDER, NAME = "/Game/GSTN/Animation/RunStart", "AS_GSTN_RunStart_Frantic"
CLIP = f"{FOLDER}/{NAME}"
SOURCE = "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop"
ALIAS_FROM = ["Idle Loop", "Idle Transition", "Idle Break"]
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
RUN = unreal.GSTN_RunPresentationComponent.static_class()


def import_clip():
    if unreal.EditorAssetLibrary.does_asset_exist(CLIP) and os.environ.get("RUNSTART_REIMPORT") != "1":
        p("clip exists", CLIP); return unreal.load_asset(CLIP)
    skel = unreal.load_asset(SOURCE).get_editor_property("skeleton")
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", False)
    ui.set_editor_property("import_animations", True)
    ui.set_editor_property("import_as_skeletal", True)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property("skeleton", skel)
    ad = ui.get_editor_property("anim_sequence_import_data")
    ad.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    ad.set_editor_property("import_bone_tracks", True)
    t = unreal.AssetImportTask()
    for k, v in (("filename", FBX), ("destination_path", FOLDER), ("destination_name", NAME), ("automated", True),
                 ("replace_existing", True), ("save", True), ("options", ui)):
        t.set_editor_property(k, v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    seq = unreal.load_asset(CLIP)
    p("imported", CLIP, "length", round(seq.get_play_length(), 3), "tracks", len(unreal.AnimationLibrary.get_animation_track_names(seq)))
    return seq


def cond(fn, compare, value=""):
    c = unreal.GSTN_RuleCondition()
    c.set_editor_property("operand", unreal.GSTN_RuleOperand.FUNCTION)
    c.set_editor_property("name", fn)
    c.set_editor_property("function_class", RUN)
    c.set_editor_property("compare", compare)
    c.set_editor_property("value", value)
    return c


def rule(*groups):
    out = []
    for conds in groups:
        grp = unreal.GSTN_RuleGroup(); grp.set_editor_property("all_of", list(conds)); out.append(grp)
    r = unreal.GSTN_TransitionRule(); r.set_editor_property("any_of", out); return r


def transition(frm, to, r, xf):
    tr = g("AddTransition", frm, to)
    p("rule", g("SetTransitionRule", tr, r))
    tr.set_editor_property("crossfade_duration", xf)
    return tr


try:
    seq = import_clip()
    length = seq.get_play_length()
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = detail = None
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        if any(s.name == "Skid" for s in d.states):
            machine, detail = info.machine, d
    names = {s.name: s for s in detail.states}
    for n in ["-> Run Start", "Run Start"]:
        if n in names:
            g("RemoveState", names[n].state); p("removed old", n)
    pos = unreal.IntPoint(0, 0)
    try:
        sp = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object().call_method(
            "GetNodeInfos", ([g("FindState", machine, "Skid")],))[0].position
        pos = unreal.IntPoint(sp.x - 700, sp.y - 300)
    except Exception as ex:
        p("position fallback", ex)
    state = g("AddState", machine, "Run Start", unreal.GSTN_StateKind.STATE, unreal.IntPoint(pos.x + 400, pos.y))
    alias = g("AddStateAlias", machine, "-> Run Start", [n for n in ALIAS_FROM if n in names], False, pos)
    conduit = g("FindState", machine, "Conduit")
    Cmp = unreal.GSTN_RuleCompare
    transition(alias, state, rule([cond("GetAnimOwnerRunStartTime", Cmp.GREATER_EQUAL, "0")]), 0.05)
    transition(state, conduit, rule([cond("GetAnimOwnerRunStartTime", Cmp.LESS, "0")],
                                    [cond("GetAnimOwnerRunStartTime", Cmp.GREATER_EQUAL, f"{length - 0.1:.3f}")]), 0.15)
    unreal.BlueprintEditorLibrary.set_node_comment(alias, "Frantic run start (owner 2026-11-18, choice B): Tools/GAS/run_start/run_start_states.py")
    ok = unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    status = abp.get_editor_property("status")
    p("compile", ok, status)
    if status != unreal.BlueprintStatus.BS_ERROR:
        p("save", unreal.EditorAssetLibrary.save_loaded_asset(abp, False))
    else:
        p("NOT SAVED: compile error")
except Exception:
    import traceback; p("ERROR", traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
for line in log[:5]:
    print(line[:200])
# route_states.py binds the state to the Blend Stack (row "Run Start" -> CLIP)
import runpy
runpy.run_path(PROJ + "Tools/GAS/routing/route_states.py", run_name="__main__")
