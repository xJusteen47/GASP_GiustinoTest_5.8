"""Jump squat state in ABP_GSTN_Locomotion (jumpsquat-v0, owner 2026-11-17 decision 1A: 3 sim frames of squat before the take-off).
Runs in the GUI editor (boot of Run-Timed -Preset Capture, or MCP execute_python); headless UnrealEditor-Cmd is not needed.

  1. placeholder pose: samples the pelvis height of the stock jump start's load (M_Relaxed_Jump_F_Start_Stand_Rfoot, 0..0.667 s, the
     part GASP skips) and of the stock heavy land (M_Relaxed_Jump_F_Land_Stand_Heavy_Lfoot, 0..0.6 s); the lower pelvis wins and its
     time (minus 0.02 s) becomes the row's start. Written to Tools/GAS/routing/jump_squat.json (row "Jump Squat", blend 0 = hard cut:
     the squat lasts 48 ms, a blend would eat it);
  2. state "Jump Squat" + alias "-> Jump Squat" from the ground states (GASP idle/locomotion + ours that exist): enter while
     UGSTN_RunPresentationComponent::IsAnimOwnerJumpSquatting (sim FGSTN_JumpState::SquatFramesLeft > 0), leave to Conduit when false
     (take-off: the "-> In Air" alias takes over from the grounded state; a cancelled squat: back to idle/locomotion);
  3. route_states.py binds the state to the Blend Stack (row "Jump Squat" in CHT_GSTN_OwnStates).
Real animation comes later (Cascadeur). Report -> Saved/JumpSquat/jump_squat_states.txt. Idempotent."""
import json
import os
import runpy
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = PROJ + "Saved/JumpSquat/jump_squat_states.txt"
JSON = PROJ + "Tools/GAS/routing/jump_squat.json"
ANIM = "/Game/Characters/UEFN_Mannequin/Animations/Jump/"
CANDIDATES = [(ANIM + "M_Relaxed_Jump_F_Start_Stand_Rfoot", 0.0, 0.667), (ANIM + "M_Relaxed_Jump_F_Land_Stand_Heavy_Lfoot", 0.0, 0.6)]
ALIAS_FROM = ["Idle Loop", "Idle Transition", "Idle Break", "Locomotion Transition", "Locomotion Loop",
              "Run Start", "Dash", "Skid", "Skid Exit Run", "Run Stop", "Run Stop Blend"]
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
RUN = unreal.GSTN_RunPresentationComponent.static_class()


def pick_pose():
    best = None
    opts = unreal.AnimPoseEvaluationOptions()
    for path, t0, t1 in CANDIDATES:
        seq = unreal.load_asset(path)
        if not seq:
            p("missing", path); continue
        stand = None
        for i in range(int((t1 - t0) * 60) + 1):
            t = t0 + i / 60.0
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, opts)
            z = unreal.AnimPoseExtensions.get_bone_pose(pose, "pelvis", unreal.AnimPoseSpaces.WORLD).translation.z
            stand = z if stand is None else stand
            if best is None or z < best[2]:
                best = (seq.get_path_name(), t, z, path)
        p("sampled", path.split("/")[-1], "first pelvis z", round(stand, 1))
    p("pick", best[3].split("/")[-1], "t", round(best[1], 3), "pelvis z", round(best[2], 1))
    return best


def cond(fn, compare):
    c = unreal.GSTN_RuleCondition()
    c.set_editor_property("operand", unreal.GSTN_RuleOperand.FUNCTION)
    c.set_editor_property("name", fn)
    c.set_editor_property("function_class", RUN)
    c.set_editor_property("compare", compare)
    grp = unreal.GSTN_RuleGroup(); grp.set_editor_property("all_of", [c])
    r = unreal.GSTN_TransitionRule(); r.set_editor_property("any_of", [grp]); return r


def transition(frm, to, r, xf):
    tr = g("AddTransition", frm, to)
    p("rule", g("SetTransitionRule", tr, r))
    tr.set_editor_property("crossfade_duration", xf)
    tr.set_editor_property("priority_order", 0)
    return tr


try:
    path, t, z, _ = pick_pose()
    json.dump({"clip": path, "start": round(max(0.0, t - 0.02), 4), "blend": 0.0, "pelvis_z": round(z, 1)}, open(JSON, "w"), indent=1)
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = detail = None
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        if any(s.name == "Skid" for s in d.states):
            machine, detail = info.machine, d
    names = {s.name: s for s in detail.states}
    for n in ["-> Jump Squat", "Jump Squat"]:
        if n in names:
            g("RemoveState", names[n].state); p("removed old", n)
    alias_from = [n for n in ALIAS_FROM if n in names]
    p("alias from", alias_from)
    pos = unreal.IntPoint(0, 0)
    try:
        sp = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object().call_method(
            "GetNodeInfos", ([g("FindState", machine, "Skid")],))[0].position
        pos = unreal.IntPoint(sp.x - 700, sp.y + 450)
    except Exception as ex:
        p("position fallback", ex)
    state = g("AddState", machine, "Jump Squat", unreal.GSTN_StateKind.STATE, unreal.IntPoint(pos.x + 400, pos.y))
    alias = g("AddStateAlias", machine, "-> Jump Squat", alias_from, False, pos)
    conduit = g("FindState", machine, "Conduit")
    Cmp = unreal.GSTN_RuleCompare
    transition(alias, state, cond("IsAnimOwnerJumpSquatting", Cmp.IS_TRUE), 0.0)
    transition(state, conduit, cond("IsAnimOwnerJumpSquatting", Cmp.IS_FALSE), 0.0)
    unreal.BlueprintEditorLibrary.set_node_comment(alias, "Jump squat (owner 1A, 3 sim frames): Tools/GAS/jump_squat/jump_squat_states.py")
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
runpy.run_path(PROJ + "Tools/GAS/routing/route_states.py", run_name="__main__")
if os.environ.get("JUMPSQUAT_QUIT") == "1":
    unreal.SystemLibrary.quit_editor()
