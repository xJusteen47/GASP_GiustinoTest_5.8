"""Skid attempt 2, fix A (docs/notes/reviews/skid-runstop-4/review_fable_skid.md; handover locomotion-finish-1 item 4).
The Skid state held SS_180_Turn_Root180 while the simulation already ran the other way (feet slid 126/219 cm, f139-172). Now:
  Skid -> Conduit: !IsAnimOwnerSkidding()  OR  state elapsed >= EXIT_AT (the clip's turn is done), crossfade XF
  -> Skid -> Skid: IsAnimOwnerSkidding() AND GetAnimOwnerSkidElapsed < REENTRY_MAX (no re-entry of the same skid after the early exit)
Headless, editor closed, _ue59 lock: UnrealEditor-Cmd <uproject> -run=pythonscript -script=<copy without spaces> -nullrhi.
Backup Saved/Backups/Skid/ABP_GSTN_Locomotion_pre_lf2.uasset (made by the caller). Report Saved/Skid/exit_fix.txt"""
import os
import unreal

OUT = unreal.Paths.project_saved_dir() + "Skid/exit_fix.txt"
EXIT_AT = float(os.environ.get("SKID_EXIT_AT", "0.42"))
XF = float(os.environ.get("SKID_EXIT_XF", "0.2"))
REENTRY_MAX = 0.4  # s since the skid began: the early exit comes at >= EXIT_AT of state time, so a re-entry is always later
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
RUN = unreal.GSTN_RunPresentationComponent.static_class()
Cmp = unreal.GSTN_RuleCompare


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
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    detail = None
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        if any(s.name == "Skid" for s in d.states):
            detail = d
    done = 0
    for t in detail.transitions:
        if t.from_ == "Skid" and t.to == "Conduit":
            p("exit", g("SetTransitionRule", t.transition, rule([cond("IsAnimOwnerSkidding", Cmp.IS_FALSE)],
                                                                 [cond("GetInstanceCurrentStateElapsedTime", Cmp.GREATER_EQUAL, f"{EXIT_AT:.3f}", True)])))
            t.transition.set_editor_property("crossfade_duration", XF)
            done += 1
        elif t.from_ == "-> Skid" and t.to == "Skid":
            p("entry", g("SetTransitionRule", t.transition, rule([cond("IsAnimOwnerSkidding", Cmp.IS_TRUE),
                                                                  cond("GetAnimOwnerSkidElapsed", Cmp.LESS, f"{REENTRY_MAX:.3f}")])))
            done += 1
    p("transitions changed", done)
    for t in g("GetStateMachine", detail.machine).transitions:
        if "kid" in t.from_ or "kid" in t.to:
            p(t.from_, "->", t.to, "xf", round(t.crossfade_duration, 3), "rule", t.rule)
    ok = unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    status = abp.get_editor_property("status")
    p("compile", ok, status)
    if done == 2 and status != unreal.BlueprintStatus.BS_ERROR:
        p("save", unreal.EditorAssetLibrary.save_loaded_asset(abp, False))
    else:
        p("NOT SAVED")
except Exception:
    import traceback; p("ERROR", traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
