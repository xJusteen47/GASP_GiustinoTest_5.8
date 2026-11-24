"""Skid attempt 3 (locomotion-finish-3; Fable FAIL docs/notes/reviews/locomotion-finish-2/review_fable_skid.md: after the early exit the
locomotion blends into a pose that does not step, feet glide 170 cm). The reviewer's fix: hand the skid straight to the run loop at a
matched phase when the stick still drives the character the new way.
  1. phase: the run loop frame whose root-local feet (position + velocity) best match the skid clip at EXIT_AT -> routing/skid_exit_run.json
     (route_states.py reads it for the "Skid Exit Run" row of CHT_GSTN_OwnStates);
  2. state "Skid Exit Run" beside Skid:  Skid -> Skid Exit Run: elapsed >= EXIT_AT AND GetAnimOwnerGroundSpeed >= RUN_MIN
     Skid -> Conduit: !IsAnimOwnerSkidding OR (elapsed >= EXIT_AT AND speed < RUN_MIN)   (stick released: stop facing the new way)
     Skid Exit Run -> Conduit: elapsed >= HOLD OR speed < RUN_MIN - 50, crossfade XF (motion matching takes over in a run).
Headless, editor closed, _ue59 lock. Run route_states.py after it. Report Saved/Skid/exit_run.txt"""
import json, math, os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = unreal.Paths.project_saved_dir() + "Skid/exit_run.txt"
PHASE_JSON = os.path.join(PROJ, "Tools", "GAS", "routing", "skid_exit_run.json")
RUN_LOOP = "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F"
EXIT_AT, RUN_MIN, HOLD, XF = 0.42, 200.0, 0.75, 0.20   # Fable lf3 r1: hand back after one full run step (0.3 s froze f164-175)
# Attempt 5 (Fable lf3 a4: the release at capture f193 fell in the hand-back blend, 20 frozen frames, 84 cm slide): Skid Exit Run leaves
# AT ONCE when the stick is let go or turned away (GetAnimOwnerStickAlong < STICK_MIN), and the loop starts LOOP_LEAD frames after the
# matched phase (the 41 cm exit drag)
STICK_MIN, LOOP_LEAD = 0.3, 4 / 60.0
ELAPSED = "GetInstanceCurrentStateElapsedTime"
log = []
def p(*a): log.append(" ".join(str(x) for x in a))
G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
B = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
def b(fn, *a): return B.call_method(fn, tuple(a))
RUN = unreal.GSTN_RunPresentationComponent.static_class()
Cmp = unreal.GSTN_RuleCompare


def feet(seq, t):
    pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(seq, t, unreal.AnimPoseEvaluationOptions())
    root = unreal.AnimPoseExtensions.get_bone_pose(pose, "root", unreal.AnimPoseSpaces.WORLD)
    out = []
    for bone in ("foot_l", "foot_r"):
        w = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
        v = root.inverse_transform_location(w.translation)
        out += [v.x, v.y, v.z]
    return out


def phase(skid):
    dt = 1.0 / 30
    a0, a1 = feet(skid, EXIT_AT), feet(skid, EXIT_AT + dt)
    run = unreal.load_asset(RUN_LOOP)
    length = run.get_play_length()
    best = None

    def yaw(v, deg):
        # the skid clip's root frame may face another axis than the run loop's (Mixamo source): try the four quarter turns
        c, s = round(math.cos(math.radians(deg))), round(math.sin(math.radians(deg)))
        out = []
        for k in (0, 3):
            x, y, z = v[k:k + 3]
            out += [c * x - s * y, s * x + c * y, z]
        return out
    runs = [(i / 60.0, feet(run, i / 60.0), feet(run, (i / 60.0 + dt) % length)) for i in range(int(length * 60))]
    for deg in (0, 90, 180, 270):
        r0, r1 = yaw(a0, deg), yaw(a1, deg)
        for t, b0, b1 in runs:
            pos = sum((x - y) ** 2 for x, y in zip(r0, b0))
            vel = sum(((x1 - x0) - (y1 - y0)) ** 2 for x0, x1, y0, y1 in zip(r0, r1, b0, b1))
            cost = math.sqrt(pos) + 0.5 * math.sqrt(vel)
            if best is None or cost < best[0]:
                best = (cost, t, b0, deg)
    p("best yaw", best[3])
    p("skid feet @exit", [round(x, 1) for x in a0])
    p("run loop", round(length, 3), "best t", round(best[1], 3), "cost", round(best[0], 1), "feet", [round(x, 1) for x in best[2]])
    return best[1]


def cond(name, compare, value="", getter=False):
    c = unreal.GSTN_RuleCondition()
    c.set_editor_property("operand", unreal.GSTN_RuleOperand.ANIM_GETTER if getter else unreal.GSTN_RuleOperand.FUNCTION)
    c.set_editor_property("name", name)
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


try:
    da = unreal.load_asset("/Game/GSTN/Animation/Layers/DA_AnimSet_Locomotion_Default")
    clip = da.get_editor_property("skid").get_editor_property("clip")
    if not isinstance(clip, unreal.AnimSequence):
        s = str(clip.get_path_name()) if hasattr(clip, "get_path_name") else str(clip)
        clip = unreal.load_asset(s.split("'")[1] if "'" in s else s)
    t0 = phase(clip)
    t0 = (t0 + LOOP_LEAD) % unreal.load_asset(RUN_LOOP).get_play_length()
    json.dump({"clip": RUN_LOOP + "." + RUN_LOOP.split("/")[-1], "start": round(t0, 4), "blend": 0.15, "exit_at": EXIT_AT},
              open(PHASE_JSON, "w"), indent=1)
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = detail = None
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        if any(s.name == "Skid" for s in d.states):
            machine, detail = info.machine, d
    names = {s.name: s for s in detail.states}
    if "Skid Exit Run" in names:
        g("RemoveState", names["Skid Exit Run"].state); p("removed old Skid Exit Run")
    skid = g("FindState", machine, "Skid")
    cnode = g("FindState", machine, "Conduit")
    sp = b("GetNodeInfos", [skid])[0].position
    run_state = g("AddState", machine, "Skid Exit Run", unreal.GSTN_StateKind.STATE, unreal.IntPoint(sp.x + 300, sp.y - 200))
    for t in g("GetStateMachine", machine).transitions:
        if t.from_ == "Skid" and t.to == "Conduit":
            p("skid->conduit", g("SetTransitionRule", t.transition, rule([cond("IsAnimOwnerSkidding", Cmp.IS_FALSE)],
                [cond(ELAPSED, Cmp.GREATER_EQUAL, f"{EXIT_AT:.3f}", True), cond("GetAnimOwnerGroundSpeed", Cmp.LESS, f"{RUN_MIN:.1f}")])))
            t.transition.set_editor_property("priority_order", 1)
    tr = g("AddTransition", skid, run_state)
    p("skid->run", g("SetTransitionRule", tr, rule([cond(ELAPSED, Cmp.GREATER_EQUAL, f"{EXIT_AT:.3f}", True),
                                                     cond("GetAnimOwnerGroundSpeed", Cmp.GREATER_EQUAL, f"{RUN_MIN:.1f}"),
                                                     cond("GetAnimOwnerStickAlong", Cmp.GREATER_EQUAL, f"{STICK_MIN:.2f}")])))
    tr.set_editor_property("crossfade_duration", 0.15)
    tr.set_editor_property("priority_order", 0)
    tr2 = g("AddTransition", run_state, cnode)
    p("run->conduit", g("SetTransitionRule", tr2, rule([cond(ELAPSED, Cmp.GREATER_EQUAL, f"{HOLD:.3f}", True)],
                                                        [cond("GetAnimOwnerGroundSpeed", Cmp.LESS, f"{RUN_MIN - 50:.1f}")],
                                                        [cond("GetAnimOwnerStickAlong", Cmp.LESS, f"{STICK_MIN:.2f}")])))
    tr2.set_editor_property("crossfade_duration", XF)
    unreal.BlueprintEditorLibrary.set_node_comment(run_state, "Skid Exit Run: stick still held after the turn -> run loop at the matched "
                                                   "foot phase (Tools/GAS/skid_exit_run.py), then motion matching")
    unreal.BlueprintEditorLibrary.set_node_comment_pinned(run_state, True)
    for t in g("GetStateMachine", machine).transitions:
        if "Skid" in t.from_ or "Skid" in t.to:
            p(t.from_, "->", t.to, "xf", round(t.crossfade_duration, 3), "prio", t.transition.get_editor_property("priority_order"))
except Exception:
    import traceback; p("ERROR", traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
