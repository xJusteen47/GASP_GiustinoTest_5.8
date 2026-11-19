"""Fast crouch states in ABP_GSTN_Locomotion (crouch-veloce-2; brief crouch-veloce-1, GASP ANIM rule: raw clips in the main state machine).
Builds, next to Skid:
  - alias "-> Fast Crouch" (the GASP idle/locomotion states) with transitions into
      "Crouch Enter"  Sequence Evaluator AS_GSTN_Crouch_Enter, time = GetAnimOwnerFastCrouchTime   (IsAnimOwnerFastCrouchEntering, 0.03 s)
      "Crouch Idle"   AS_GSTN_Crouch_Idle (looping player)                                         (IsAnimOwnerFastCrouchIdle, 0.05 s: landing cut)
  - Enter -> Idle 0 s, Enter/Idle -> Exit 0.03 s, Exit -> Enter 0.03 s
  - "Crouch Exit" Sequence Evaluator AS_GSTN_Crouch_Exit (its last frame is the standing idle)
  - every crouch state -> Conduit when IsAnimOwnerFastCrouching turns false, 0.1 s
  Transitions carry the follow-through blend profile BLP_GSTN_FollowThrough when the node takes one.
Phases come from C++ (UGSTN_RunPresentationComponent::TickFastCrouch, gstn.FastCrouch.Mode). Clean rebuild on every run.
Headless: run from a path without spaces. Report -> Saved/Crouch/crouch_states.txt"""
import unreal
OUT = unreal.Paths.project_saved_dir() + "Crouch/crouch_states.txt"
CR = "/Game/GSTN/Animation/Crouch/"
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
B = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
def b(fn, *a): return B.call_method(fn, tuple(a))
RUN = unreal.GSTN_RunPresentationComponent.static_class()
PROFILE = unreal.load_asset(CR + "BLP_GSTN_FollowThrough")
ALIAS_FROM = ["Locomotion Loop", "Locomotion Transition", "Idle Loop", "Idle Transition", "Idle Break"]

def rule(fn, negate=False):
    c = unreal.GSTN_RuleCondition()
    c.set_editor_property("operand", unreal.GSTN_RuleOperand.FUNCTION)
    c.set_editor_property("name", fn)
    c.set_editor_property("function_class", RUN)
    c.set_editor_property("compare", unreal.GSTN_RuleCompare.IS_FALSE if negate else unreal.GSTN_RuleCompare.IS_TRUE)
    grp = unreal.GSTN_RuleGroup(); grp.set_editor_property("all_of", [c])
    r = unreal.GSTN_TransitionRule(); r.set_editor_property("any_of", [grp]); return r

def transition(frm, to, fn, negate, xf, profile=True):
    tr = g("AddTransition", frm, to)
    p("rule", fn, g("SetTransitionRule", tr, rule(fn, negate)))
    tr.set_editor_property("crossfade_duration", xf)
    tr.set_editor_property("priority_order", 0)
    if profile and PROFILE:
        try:
            tr.set_editor_property("blend_profile", PROFILE)
        except Exception as e:
            p("blend profile refused:", e)
    return tr

def inner_graph(node):
    return next(x for x in unreal.ObjectIterator(unreal.EdGraph) if x.get_outer() == node)

def pins(node, out):
    info = b("GetNodeInfos", [node])[0]
    return {x.name: x.pin_id for x in (info.output_pins if out else info.input_pins)}

def node_of_type(graph, filt, must, pos):
    types = b("FindNodeTypes", graph, filt, None)
    pick = [t for t in types if all(m in t.replace(" ", "") for m in must)]
    p("types", filt, "->", pick[:2])
    return b("CreateNode", graph, pick[0], unreal.IntPoint(pos[0], pos[1]), None)

def timed_evaluator(state, clip):
    sg = inner_graph(state)
    result = b("FindNodes", sg, "", None, False)[0]
    ev = node_of_type(sg, clip, ["Valuta", clip], (-500, 0))
    f_time = node_of_type(sg, "GetAnimOwnerFastCrouchTime", ["GetAnimOwnerFastCrouchTime"], (-850, 120))
    b("ConnectPins", list(pins(ev, True).values())[0], list(pins(result, False).values())[0])
    b("ConnectPins", pins(f_time, True)["ReturnValue"], pins(ev, False)["ExplicitTime"])

try:
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = None
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        if any(s.name == "Skid" for s in d.states):
            machine, detail = info.machine, d
    names = {s.name: s for s in detail.states}
    p("states", sorted(names))
    for n in ["-> Fast Crouch", "Crouch Enter", "Crouch Idle", "Crouch Exit"]:
        if n in names:
            g("RemoveState", names[n].state); p("removed old", n)
    skid = g("FindState", machine, "Skid")
    sp = b("GetNodeInfos", [skid])[0].position
    x0, y0 = sp.x - 700, sp.y + 250
    enter = g("AddState", machine, "Crouch Enter", unreal.GSTN_StateKind.STATE, unreal.IntPoint(x0 + 350, y0))
    idle = g("AddState", machine, "Crouch Idle", unreal.GSTN_StateKind.STATE, unreal.IntPoint(x0 + 650, y0))
    exit_ = g("AddState", machine, "Crouch Exit", unreal.GSTN_StateKind.STATE, unreal.IntPoint(x0 + 500, y0 + 200))
    alias = g("AddStateAlias", machine, "-> Fast Crouch", [n for n in ALIAS_FROM if n in names], False, unreal.IntPoint(x0, y0 + 100))
    conduit = g("FindState", machine, "Conduit")

    timed_evaluator(enter, "AS_GSTN_Crouch_Enter")
    timed_evaluator(exit_, "AS_GSTN_Crouch_Exit")
    p("idle anim", g("SetStateAnimation", idle, unreal.load_asset(CR + "AS_GSTN_Crouch_Idle")))

    transition(alias, enter, "IsAnimOwnerFastCrouchEntering", False, 0.03)
    transition(alias, idle, "IsAnimOwnerFastCrouchIdle", False, 0.05)
    transition(enter, idle, "IsAnimOwnerFastCrouchIdle", False, 0.0, profile=False)
    transition(enter, exit_, "IsAnimOwnerFastCrouchExiting", False, 0.03)
    transition(idle, exit_, "IsAnimOwnerFastCrouchExiting", False, 0.03)
    transition(exit_, enter, "IsAnimOwnerFastCrouchEntering", False, 0.03)
    for s in (enter, idle, exit_):
        transition(s, conduit, "IsAnimOwnerFastCrouching", True, 0.1)
    note = ("Fast Crouch (owner 2026-11-18): standing ground crouch, our clips (Tools/GAS/crouch). Phase from C++ "
            "UGSTN_RunPresentationComponent::TickFastCrouch; gstn.FastCrouch.Mode 0 = GASP crouch, 1 = these states")
    unreal.BlueprintEditorLibrary.set_node_comment(alias, note)
    unreal.BlueprintEditorLibrary.set_node_comment_pinned(alias, True)

    d2 = g("GetStateMachine", machine)
    for t in d2.transitions:
        if "Crouch" in t.from_ or "Crouch" in t.to:
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

# Routing fix 2026-11-19: this machine outputs no pose; route_states.py empties our states and binds them to the Blend Stack chooser rows
import runpy
runpy.run_path(unreal.Paths.project_dir() + "Tools/GAS/routing/route_states.py", run_name="__main__")
