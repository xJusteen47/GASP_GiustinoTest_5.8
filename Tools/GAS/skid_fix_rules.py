"""skid-fix-1 (owner 2026-11-20): the Skid state holds until the sim skid ends (the skid move now lasts the clip and follows its root),
like a stock GASP pivot that plays out before locomotion takes over. Only the two Skid exits are rewritten (no state added or removed):
  Skid -> Conduit (priority 0):        !IsAnimOwnerSkidding
  Skid -> Skid Exit Run (priority 1):  !IsAnimOwnerSkidding AND GetAnimOwnerGroundSpeed >= 200 AND GetAnimOwnerStickAlong >= 0.3
The old timed rule (elapsed >= 0.42) cut the clip mid-turn. Compile + save the ABP. Report Saved/SkidFix/rules.txt (<= 5 lines)."""
import os, traceback
import unreal

OUT = unreal.Paths.project_saved_dir() + "SkidFix/rules.txt"
RUN = unreal.GSTN_RunPresentationComponent.static_class()
Cmp = unreal.GSTN_RuleCompare
G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
log = []
def p(*a): log.append(" ".join(str(x) for x in a))
def cond(name, compare, value=""):
    c = unreal.GSTN_RuleCondition()
    c.set_editor_property("operand", unreal.GSTN_RuleOperand.FUNCTION)
    c.set_editor_property("name", name)
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
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = None
    for info in g("FindStateMachines", abp, ""):
        if any(s.name == "Skid" for s in g("GetStateMachine", info.machine).states):
            machine = info.machine
    done = []
    for t in g("GetStateMachine", machine).transitions:
        if t.from_ == "Skid" and t.to == "Conduit":
            g("SetTransitionRule", t.transition, rule([cond("IsAnimOwnerSkidding", Cmp.IS_FALSE)]))
            t.transition.set_editor_property("priority_order", 0); done.append("conduit")
        elif t.from_ == "Skid" and t.to == "Skid Exit Run":
            g("SetTransitionRule", t.transition, rule([cond("IsAnimOwnerSkidding", Cmp.IS_FALSE),
                                                       cond("GetAnimOwnerGroundSpeed", Cmp.GREATER_EQUAL, "200.0"),
                                                       cond("GetAnimOwnerStickAlong", Cmp.GREATER_EQUAL, "0.30")]))
            t.transition.set_editor_property("priority_order", 1); done.append("exitrun")
    skid = g("FindState", machine, "Skid")
    unreal.BlueprintEditorLibrary.set_node_comment(skid, "Skid: plays the owner's clip to its end; the sim skid follows the clip root "
                                                   "(skid-fix-1). Exits when the sim skid ends (Tools/GAS/skid_fix_rules.py)")
    ok = unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(abp, False)
    p("rules", done, "compile", ok, "saved", saved)
except Exception:
    p("ERROR", traceback.format_exc()[-800:])
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log) + "\n")
