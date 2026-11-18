"""Run stop in ABP_GSTN_Locomotion (owner 2026-11-18: stick let go during a run, no reverse input; a raw clip entered by the main state
machine, never through motion matching). Builds, next to Skid:
  - alias "-> Run Stop" (Locomotion Loop/Transition, Idle Loop/Transition) with two transitions:
      -> "Run Stop"       IsAnimOwnerRunStopping()      0.06 s (sharp entry, brief section 2)
      -> "Run Stop Blend" IsAnimOwnerRunStopBlending()  0.30 s (the plain interpolation into idle the owner asked to compare)
  - "Run Stop": Blend Poses by bool (IsAnimOwnerRunStopLeftFoot) between two Sequence Evaluators (GASP M_Neutral_Run_Stop_F_Lfoot /
    _Rfoot), explicit time = GetAnimOwnerRunStopClipTime() (speed-matched brake, then settle; C++ UGSTN_RunPresentationComponent)
  - "Run Stop Blend": the profile's idle clip
  - out of both: -> Conduit when the function turns false, 0.2 s; comment boxes for the owner.
Clean rebuild on every run (removes its own nodes first). Headless: run from a path without spaces. Report -> Saved/Logs/ci/run_stop_states.txt"""
import unreal
OUT = unreal.Paths.project_saved_dir() + "Logs/ci/run_stop_states.txt"
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
B = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
def b(fn, *a): return B.call_method(fn, tuple(a))
RUN = unreal.GSTN_RunPresentationComponent.static_class()
ANIM = "/Game/Characters/UEFN_Mannequin/Animations/"

def rule(fn, negate=False):
    c = unreal.GSTN_RuleCondition()
    c.set_editor_property("operand", unreal.GSTN_RuleOperand.FUNCTION)
    c.set_editor_property("name", fn)
    c.set_editor_property("function_class", RUN)
    c.set_editor_property("compare", unreal.GSTN_RuleCompare.IS_FALSE if negate else unreal.GSTN_RuleCompare.IS_TRUE)
    grp = unreal.GSTN_RuleGroup(); grp.set_editor_property("all_of", [c])
    r = unreal.GSTN_TransitionRule(); r.set_editor_property("any_of", [grp]); return r

def transition(frm, to, fn, negate, xf):
    tr = g("AddTransition", frm, to)
    p("rule", g("SetTransitionRule", tr, rule(fn, negate)))
    tr.set_editor_property("crossfade_duration", xf)
    tr.set_editor_property("priority_order", 0)
    return tr

def inner_graph(node):
    return next(x for x in unreal.ObjectIterator(unreal.EdGraph) if x.get_outer() == node)

def pins(node, out):
    info = b("GetNodeInfos", [node])[0]
    return {x.name: x.pin_id for x in (info.output_pins if out else info.input_pins)}

def node_of_type(graph, filt, must, pos):
    types = b("FindNodeTypes", graph, filt, None)
    pick = [t for t in types if all(m in t.replace(" ", "") for m in must)]
    p("types", filt, "->", pick[:3], "all", types[:6])
    return b("CreateNode", graph, pick[0], unreal.IntPoint(pos[0], pos[1]), None)

try:
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = None
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        if any(s.name == "Skid" for s in d.states):
            machine, detail = info.machine, d
    names = {s.name: s for s in detail.states}
    for n in ["-> Run Stop", "Run Stop", "Run Stop Blend"]:
        if n in names:
            g("RemoveState", names[n].state); p("removed old", n)
    mgraph = inner_graph(machine)
    for cb in b("ListCommentBoxes", mgraph):
        if b("GetCommentBoxText", cb).startswith("Run Stop"):
            b("RemoveCommentBox", cb)
    skid = g("FindState", machine, "Skid")
    sp = b("GetNodeInfos", [skid])[0].position
    p("skid at", sp)
    stop = g("AddState", machine, "Run Stop", unreal.GSTN_StateKind.STATE, unreal.IntPoint(sp.x + 350, sp.y + 250))
    blend = g("AddState", machine, "Run Stop Blend", unreal.GSTN_StateKind.STATE, unreal.IntPoint(sp.x + 350, sp.y + 420))
    alias = g("AddStateAlias", machine, "-> Run Stop", ["Locomotion Loop", "Locomotion Transition", "Idle Loop", "Idle Transition"], False,
              unreal.IntPoint(sp.x, sp.y + 330))
    conduit = g("FindState", machine, "Conduit")

    # Run Stop: two evaluators, one per planted foot, the same clip time
    sg = inner_graph(stop)
    result = b("FindNodes", sg, "", None, False)[0]
    ev_l = node_of_type(sg, "M_Neutral_Run_Stop_F_Lfoot", ["Valuta", "M_Neutral_Run_Stop_F_Lfoot"], (-700, -120))
    ev_r = node_of_type(sg, "M_Neutral_Run_Stop_F_Rfoot", ["Valuta", "M_Neutral_Run_Stop_F_Rfoot"], (-700, 120))
    for ev in (ev_l, ev_r):
        p("evaluator", ev.get_class().get_name())
    bl = g("AddAnimNode", sg, unreal.load_class(None, "/Script/AnimGraph.AnimGraphNode_BlendListByBool"), unreal.IntPoint(-350, 0))
    f_time = node_of_type(sg, "GetAnimOwnerRunStopClipTime", ["GetAnimOwnerRunStopClipTime"], (-1050, 200))
    f_left = node_of_type(sg, "IsAnimOwnerRunStopLeftFoot", ["IsAnimOwnerRunStopLeftFoot"], (-700, 300))
    pr, pl, pe, pbl = pins(result, False), pins(ev_l, True), pins(ev_r, True), pins(bl, False)
    p("blend inputs", list(pbl.keys()), "evaluator in", list(pins(ev_l, False).keys()))
    out_bl = list(pins(bl, True).values())[0]
    b("ConnectPins", out_bl, list(pr.values())[0])
    true_pin = pbl["BlendPose_0"]  # BlendListByBool: pose 0 = True
    false_pin = pbl["BlendPose_1"]
    b("ConnectPins", list(pl.values())[0], true_pin)
    b("ConnectPins", list(pe.values())[0], false_pin)
    t_out = pins(f_time, True)["ReturnValue"]
    for ev in (ev_l, ev_r):
        b("ConnectPins", t_out, pins(ev, False)["ExplicitTime"])
    b("ConnectPins", pins(f_left, True)["ReturnValue"], pbl["bActiveValue"])
    for k, v in pbl.items():
        if "BlendTime" in k:
            b("SetPinValue", v, "0.0")
    b("AddCommentBox", sg, "Run Stop: the foot swinging at release plants in front (left = true). Clip time from C++: brake timed on the sim's "
      "speed, then the settle (FGSTN_RunStopSettings in DA_AnimSet_Locomotion_Default)", [ev_l, ev_r, bl, f_time, f_left], None, 40)

    # Run Stop Blend: the idle pose, the transition's cross-fade is the interpolation
    p("blend anim", g("SetStateAnimation", blend, unreal.load_asset(ANIM + "Idle/M_Neutral_Stand_Idle_Loop")))

    transition(alias, stop, "IsAnimOwnerRunStopping", False, 0.06)
    transition(alias, blend, "IsAnimOwnerRunStopBlending", False, 0.30)
    transition(stop, conduit, "IsAnimOwnerRunStopping", True, 0.2)
    transition(blend, conduit, "IsAnimOwnerRunStopBlending", True, 0.2)
    note = ("Run Stop (owner 2026-11-18): stick let go during a run, NO reverse input (reverse = Skid). "
            "gstn.RunStop.Mode: 1 clip (Run Stop), 2 blend into idle (Run Stop Blend), 0 off (GASP stops)")
    try:
        b("AddCommentBox", mgraph, note, [alias, stop, blend], None, 40)
    except Exception as e:
        # The state machine graph refuses comment boxes through the toolset: a pinned node comment on the alias instead
        p("comment box refused:", e)
        unreal.BlueprintEditorLibrary.set_node_comment(alias, note)
        unreal.BlueprintEditorLibrary.set_node_comment_pinned(alias, True)

    d2 = g("GetStateMachine", machine)
    for t in d2.transitions:
        if "Run Stop" in t.from_ or "Run Stop" in t.to:
            p("tr", t.from_, "->", t.to, "|", t.rule, "| xf", round(t.crossfade_duration, 3))
    ok = unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    status = abp.get_editor_property("status")
    p("compile", ok, status)
    if status != unreal.BlueprintStatus.BS_ERROR:
        p("save", unreal.EditorAssetLibrary.save_loaded_asset(abp, False))
    else:
        p("NOT SAVED: compile error")
except Exception:
    import traceback; p("ERROR", traceback.format_exc())
open(OUT, "w").write("\n".join(log))
unreal.SystemLibrary.quit_editor()
