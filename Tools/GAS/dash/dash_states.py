"""L3 boost dash clip in ABP_GSTN_Locomotion (dash-ganon r3; model: Tools/GAS/run_start/run_start_states.py). Headless, editor closed,
_ue59 lock held:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="<project>/Tools/GAS/dash/dash_states.py" -unattended -nullrhi
(copy the script to a path without spaces first if the project path has spaces, as for route_states.py.)

  1. imports docs/notes/reviews/dash-ganon/<DASH_ROUND or r3>/AS_GSTN_Dash_Ganon_<round>.fbx as /Game/GSTN/Animation/Dash/AS_GSTN_Dash_Ganon
     (UEFN mannequin skeleton, 60 fps, root motion on the root = the sim's boost dash; skipped when the asset exists and
     DASH_REIMPORT is not 1). Root motion ON + root lock REF_POSE + force root lock (as the run stop clips / GASP starts): the clip's
     root speed only feeds movedata_speed and warping, the capsule stays the sim's;
  2. GASP curves and Foley via Tools/GAS/routing/curves_author.py, kind "start" (contact_l/r, phase, movedata_speed, enable_warping 1);
  3. state "Dash" + alias "-> Dash" from GASP's idle and moving ground states: enter when GetAnimOwnerBoostDashTime is in [0, ENTRY_WINDOW),
     leave to Conduit when it is < 0 (stick released) or past the clip's length minus the exit blend. It also wins over "Run Start":
     the boost ends the stamp on the same frame (run start time -> -1), so Run Start leaves through its own rule;
  4. route_states.py binds the state to the Blend Stack (row "Dash" -> the clip, 0.03 s blend).
Report -> Saved/Dash/dash_states.txt, prints max 5 lines. Idempotent (old "Dash" / "-> Dash" are removed first)."""
import os
import runpy
import unreal

PROJ = unreal.Paths.project_dir()
OUT = unreal.Paths.project_saved_dir() + "Dash/dash_states.txt"
ROUND = os.environ.get("DASH_ROUND", "r3")   # r2 = the Fable offline PASS, r3 = r2 + its non-gating fixes 1-4
FBX = PROJ + f"docs/notes/reviews/dash-ganon/{ROUND}/AS_GSTN_Dash_Ganon_{ROUND}.fbx"
FOLDER, NAME = "/Game/GSTN/Animation/Dash", "AS_GSTN_Dash_Ganon"
CLIP = f"{FOLDER}/{NAME}"
SOURCE = "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop"
ALIAS_FROM = ["Idle Loop", "Idle Transition", "Idle Break", "Locomotion Transition", "Locomotion Loop", "Run Start"]
ENTRY_WINDOW = 0.10  # s from the boost: L3 pressed later than this (already running) does not replay the dash from the load
EXIT_BLEND = 0.12    # s: the dash hands over to the run loop with this cross-fade
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
RUN = unreal.GSTN_RunPresentationComponent.static_class()


def import_clip():
    if unreal.EditorAssetLibrary.does_asset_exist(CLIP) and os.environ.get("DASH_REIMPORT") != "1":
        p("clip exists", CLIP); return unreal.load_asset(CLIP)
    if not os.path.exists(FBX):
        raise RuntimeError(f"missing {FBX}")
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
    try:
        ad.set_editor_property("use_default_sample_rate", False)
        ad.set_editor_property("custom_sample_rate", 60)
    except Exception as ex:
        p("sample rate props", ex)
    t = unreal.AssetImportTask()
    for k, v in (("filename", FBX), ("destination_path", FOLDER), ("destination_name", NAME), ("automated", True),
                 ("replace_existing", True), ("save", True), ("options", ui)):
        t.set_editor_property(k, v)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    seq = unreal.load_asset(CLIP)
    seq.set_editor_property("enable_root_motion", True)
    seq.set_editor_property("force_root_lock", True)
    seq.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)
    unreal.EditorAssetLibrary.save_asset(CLIP, only_if_is_dirty=False)
    n = len(unreal.AnimationLibrary.get_animation_track_names(seq))
    p("imported", CLIP, "length", round(seq.get_play_length(), 3), "tracks", n)
    # sanity: the root must travel forward (the dash's root motion); 0 = the FBX lost its root track
    opts = unreal.AnimPoseEvaluationOptions()
    r0 = unreal.AnimPoseExtensions.get_bone_pose(unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, 0.0, opts), "root",
                                                 unreal.AnimPoseSpaces.WORLD).translation
    r1 = unreal.AnimPoseExtensions.get_bone_pose(unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, seq.get_play_length(), opts),
                                                 "root", unreal.AnimPoseSpaces.WORLD).translation
    p("root travel cm (expect ~227 forward, +Y)", round(r1.x - r0.x, 1), round(r1.y - r0.y, 1), round(r1.z - r0.z, 1))
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
    # 2. curves (same tool and rules as our other locomotion clips, docs/notes/gasp-curves.md)
    os.environ["CURVES_CLIPS"] = f"{CLIP}:start"
    try:
        runpy.run_path(PROJ + "Tools/GAS/routing/curves_author.py", run_name="__main__")
        p("curves: see Saved/Routing/curves_author.txt")
    except Exception as ex:
        p("curves FAILED", ex)
    # 3. state + alias
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = detail = None
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        if any(s.name == "Skid" for s in d.states):
            machine, detail = info.machine, d
    names = {s.name: s for s in detail.states}
    alias_from = [n for n in ALIAS_FROM if n in names]
    p("alias from", alias_from)
    for n in ["-> Dash", "Dash"]:
        if n in names:
            g("RemoveState", names[n].state); p("removed old", n)
    pos = unreal.IntPoint(0, 0)
    try:
        sp = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object().call_method(
            "GetNodeInfos", ([g("FindState", machine, "Skid")],))[0].position
        pos = unreal.IntPoint(sp.x - 700, sp.y + 300)
    except Exception as ex:
        p("position fallback", ex)
    state = g("AddState", machine, "Dash", unreal.GSTN_StateKind.STATE, unreal.IntPoint(pos.x + 400, pos.y))
    alias = g("AddStateAlias", machine, "-> Dash", alias_from, False, pos)
    conduit = g("FindState", machine, "Conduit")
    Cmp = unreal.GSTN_RuleCompare
    transition(alias, state, rule([cond("GetAnimOwnerBoostDashTime", Cmp.GREATER_EQUAL, "0"),
                                    cond("GetAnimOwnerBoostDashTime", Cmp.LESS, f"{ENTRY_WINDOW:.3f}")]), 0.03)
    transition(state, conduit, rule([cond("GetAnimOwnerBoostDashTime", Cmp.LESS, "0")],
                                    [cond("GetAnimOwnerBoostDashTime", Cmp.GREATER_EQUAL, f"{length - EXIT_BLEND:.3f}")]), EXIT_BLEND)
    unreal.BlueprintEditorLibrary.set_node_comment(alias, "L3 boost dash, Ganon-style (owner 2026-11-19): Tools/GAS/dash/dash_states.py")
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
# 4. route_states.py binds the state to the Blend Stack (row "Dash" -> CLIP)
runpy.run_path(PROJ + "Tools/GAS/routing/route_states.py", run_name="__main__")
