"""Frantic run start in ABP_GSTN_Locomotion (run-start-frantic-2, handover run-start-frantic-1). Headless, editor closed, _ue59 lock:
  UnrealEditor-Cmd <uproject> -run=pythonscript -script="<project>/Tools/GAS/run_start/run_start_states.py" -unattended -nullrhi
  1. imports docs/notes/reviews/run-start-frantic/r3/AS_RunStart_Frantic_r3.fbx as /Game/GSTN/Animation/RunStart/AS_GSTN_RunStart_Frantic
     (UEFN mannequin skeleton, 30 fps as authored; skipped when the asset exists and RUNSTART_REIMPORT is not 1)
  2. state "Run Start" + alias "-> Run Start" (GASP idle states), enter when GetAnimOwnerRunStartTime >= 0, leave to Conduit when it is
     < 0 or past the clip's length. The state is routed by route_states.py (Blend Stack row "Run Start", plays at the clip's own rate).
Report -> Saved/RunStart/run_start_states.txt, prints max 5 lines."""
import json
import os
import unreal

PROJ = unreal.Paths.project_dir()
OUT = unreal.Paths.project_saved_dir() + "RunStart/run_start_states.txt"
FBX = PROJ + "docs/notes/reviews/run-start-frantic/r3/AS_RunStart_Frantic_r3.fbx"
# runstart-v3-1: RUNSTART_VARIANT=v3 (default) plays AS_GSTN_RunStart_v3 (imported by Tools/AnimPipeline/cards/runstart/runstart_v3_ue.py)
# to EXIT_AT, then state "Run Start Exit Run" = stock run loop at the clip's own end phase (no Conduit/transition chooser in between, so
# SS_180_Turn_Root180 cannot enter); RUNSTART_VARIANT=frantic = rollback (Frantic + foot-plant hand-off, no exit state). The variant is
# written to Tools/GAS/routing/run_start.json, which route_states.py reads for the "Run Start" / "Run Start Exit Run" rows.
VARIANT = os.environ.get("RUNSTART_VARIANT", "v3")
FOLDER = "/Game/GSTN/Animation/RunStart"
NAME = "AS_GSTN_RunStart_v3" if VARIANT == "v3" else "AS_GSTN_RunStart_Frantic"
CLIP = f"{FOLDER}/{NAME}"
RUN_LOOP = "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F"
V3_BUILD = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + "AnimPipeline/runstart/v3_build.json"
RUN_JSON = unreal.Paths.convert_relative_path_to_full(PROJ) + "Tools/GAS/routing/run_start.json"
RUN_MIN, STICK_MIN, HOLD, XF_RUN = 200.0, 0.3, 0.75, 0.10  # as Skid Exit Run (skid_exit_run.py)
ELAPSED = "GetInstanceCurrentStateElapsedTime"
SOURCE = "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop"
ALIAS_FROM = ["Idle Loop", "Idle Transition", "Idle Break"]
ALIAS_FROM_MOVING = ["Locomotion Transition", "Locomotion Loop"]  # GASP start states (v2)
ALIAS_WINDOW = 0.25  # s from the push: later than this the run start is not entered any more
EXIT_BLEND = float(os.environ.get("RUNSTART_EXIT_BLEND", "0.15"))  # crossfade into GASP's motion-matched run at the hand-off
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
RUN = unreal.GSTN_RunPresentationComponent.static_class()


def import_clip():
    if VARIANT == "v3":
        seq = unreal.load_asset(CLIP)
        if not seq:
            raise RuntimeError("missing " + CLIP + ": run runstart_v3_ue.py first")
        return seq
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


def cond(fn, compare, value="", getter=False):
    c = unreal.GSTN_RuleCondition()
    c.set_editor_property("operand", unreal.GSTN_RuleOperand.ANIM_GETTER if getter else unreal.GSTN_RuleOperand.FUNCTION)
    c.set_editor_property("name", fn)
    if not getter:
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
    # the hand-off flag waits for a contact_l/r rising edge: without the curves it falls back to RunStart.HandOffMaxAfterStamp
    curves = [str(n) for n in unreal.AnimationLibrary.get_animation_curve_names(seq, unreal.RawCurveTrackTypes.RCT_FLOAT)]
    p("clip curves", sorted(curves))
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = detail = None
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        if any(s.name == "Skid" for s in d.states):
            machine, detail = info.machine, d
    names = {s.name: s for s in detail.states}
    p("states", sorted(names))
    # v2 (locomotion-finish): the push also lets GASP leave idle on the same frame (our getter is one frame late), so the alias also
    # covers GASP's moving ground states; the alias only fires in the first ALIAS_WINDOW s so the state cannot re-enter after its exit
    alias_from = [n for n in ALIAS_FROM + ALIAS_FROM_MOVING if n in names]
    p("alias from", alias_from)
    for n in ["-> Run Start", "Run Start", "Run Start Exit Run"]:
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
    alias = g("AddStateAlias", machine, "-> Run Start", alias_from, False, pos)
    conduit = g("FindState", machine, "Conduit")
    Cmp = unreal.GSTN_RuleCompare
    transition(alias, state, rule([cond("GetAnimOwnerRunStartTime", Cmp.GREATER_EQUAL, "0"),
                                    cond("GetAnimOwnerRunStartTime", Cmp.LESS, f"{ALIAS_WINDOW:.3f}")]), 0.05)
    # v3 (run-start-handoff-1, owner 11-19 "resta fermo troppo, non fluisce nel loop"): leave on the hand-off flag (stamp over, then the
    # clip's first foot plant; data in DA_AnimSet_Locomotion_Default RunStart|Hand-off), the clip end stays as a fallback
    if VARIANT == "v3":
        # v3 is a whole run start (stomp + frantic run, 1.5 s) that ends on a run loop phase: it plays to EXIT_AT and hands over to the
        # stock run loop at that phase (no foot-plant hand-off: the stomp at f8 is a plant); the Conduit only when the run start is over
        b = json.load(open(V3_BUILD))
        exit_at, loop_t = b["handoff"]["exit_at"], b["handoff"]["loop_start"]
        run_state = g("AddState", machine, "Run Start Exit Run", unreal.GSTN_StateKind.STATE, unreal.IntPoint(pos.x + 750, pos.y))
        tr = transition(state, run_state, rule([cond(ELAPSED, Cmp.GREATER_EQUAL, f"{exit_at:.3f}", True),
                                                 cond("GetAnimOwnerGroundSpeed", Cmp.GREATER_EQUAL, f"{RUN_MIN:.1f}"),
                                                 cond("GetAnimOwnerStickAlong", Cmp.GREATER_EQUAL, f"{STICK_MIN:.2f}")]), XF_RUN)
        tr.set_editor_property("priority_order", 0)
        tr = transition(state, conduit, rule([cond("GetAnimOwnerRunStartTime", Cmp.LESS, "0")],
                                             [cond("GetAnimOwnerStickAlong", Cmp.LESS, f"{STICK_MIN:.2f}")],
                                             [cond(ELAPSED, Cmp.GREATER_EQUAL, f"{length:.3f}", True)]), EXIT_BLEND)
        tr.set_editor_property("priority_order", 1)
        transition(run_state, conduit, rule([cond(ELAPSED, Cmp.GREATER_EQUAL, f"{HOLD:.3f}", True)],
                                            [cond("GetAnimOwnerGroundSpeed", Cmp.LESS, f"{RUN_MIN - 50:.1f}")],
                                            [cond("GetAnimOwnerStickAlong", Cmp.LESS, f"{STICK_MIN:.2f}")]), 0.20)
        unreal.BlueprintEditorLibrary.set_node_comment(run_state, "Run Start Exit Run: v3 run start -> stock run loop at the clip's end phase "
                                                       "(Tools/GAS/run_start/run_start_states.py), then motion matching")
        spec = {"variant": "v3", "clip": CLIP + "." + NAME, "start": 0.0, "blend": 0.05,
                "exit_run": {"clip": RUN_LOOP + "." + RUN_LOOP.split("/")[-1], "start": round(loop_t, 4), "blend": XF_RUN}}
        p("v3 exit at", exit_at, "loop t", loop_t)
    else:
        # v3 (run-start-handoff-1, owner 11-19 "resta fermo troppo, non fluisce nel loop"): leave on the hand-off flag (stamp over, then
        # the clip's first foot plant; data in DA_AnimSet_Locomotion_Default RunStart|Hand-off), the clip end stays as a fallback
        transition(state, conduit, rule([cond("GetAnimOwnerRunStartTime", Cmp.LESS, "0")],
                                        [cond("GetAnimOwnerRunStartHandOff", Cmp.GREATER_EQUAL, "1")],
                                        [cond("GetAnimOwnerRunStartTime", Cmp.GREATER_EQUAL, f"{length - 0.1:.3f}")]), EXIT_BLEND)
        spec = {"variant": "frantic", "clip": CLIP + "." + NAME, "start": 0.0, "blend": 0.05}
    json.dump(spec, open(RUN_JSON, "w"), indent=1)
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
