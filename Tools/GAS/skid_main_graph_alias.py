"""Skid like the owner's 5.7 setup: profile on ABP_GSTN_Locomotion with the skid in the main graph, and ONE alias (locomotion + idle
states) with a single transition into Skid instead of one arrow per state (owner 2026-11-18). Idempotent. Report -> Saved/Logs/ci/skid_alias.txt"""
import unreal
OUT = unreal.Paths.project_saved_dir() + "Logs/ci/skid_alias.txt"
log = []
def p(*a): log.append(" ".join(str(x) for x in a))
try:
    T = unreal.GSTN_AnimBlueprintToolset
    da = unreal.load_asset("/Game/Blueprints/Animation/DA_AnimSet_Locomotion_Default")
    gen = unreal.load_object(None, "/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion.ABP_GSTN_Locomotion_C")
    da.set_editor_property("main_anim_class", gen)
    da.set_editor_property("skid_in_main_graph", True)
    p("profile", da.get_editor_property("main_anim_class"), da.get_editor_property("skid_in_main_graph"))
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    machine = None
    for info in T.find_state_machines(abp, ""):
        d = T.get_state_machine(info.machine)
        if any(s.name == "Skid" for s in d.states):
            machine, detail = info.machine, d
    p("machine", detail.name)
    into = [t for t in detail.transitions if t.to == "Skid" and t.from_ != "SkidEntry"]
    p("into Skid:", [(t.from_, t.rule, round(t.crossfade_duration, 3)) for t in into])
    names = {s.name: s for s in detail.states}
    sources = [t.from_ for t in into if names.get(t.from_) and names[t.from_].kind != unreal.GSTN_StateKind.ALIAS]
    if "SkidEntry" not in names:
        alias = T.add_state_alias(machine, "SkidEntry", sources, False, None)
    else:
        alias = names["SkidEntry"].state
    skid = T.find_state(machine, "Skid")
    tr = T.add_transition(alias, skid)
    cond = unreal.GSTN_RuleCondition()
    cond.set_editor_property("operand", unreal.GSTN_RuleOperand.FUNCTION)
    cond.set_editor_property("name", "IsAnimOwnerSkidding")
    cond.set_editor_property("function_class", unreal.GSTN_RunPresentationComponent.static_class())
    cond.set_editor_property("compare", unreal.GSTN_RuleCompare.IS_TRUE)
    grp = unreal.GSTN_RuleGroup(); grp.set_editor_property("all_of", [cond])
    rule = unreal.GSTN_TransitionRule(); rule.set_editor_property("any_of", [grp])
    p("rule", T.set_transition_rule(tr, rule))
    tr.set_editor_property("crossfade_duration", into[0].crossfade_duration if into else 0.08)
    for t in into:
        if t.from_ in sources:
            T.remove_transition(t.transition)
    d2 = T.get_state_machine(machine)
    p("after:", [(t.from_, t.to, t.rule) for t in d2.transitions if t.to == "Skid"])
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    p("save abp", unreal.EditorAssetLibrary.save_loaded_asset(abp, False), "save da", unreal.EditorAssetLibrary.save_loaded_asset(da, False))
except Exception as e:
    import traceback; p("ERROR", traceback.format_exc())
open(OUT, "w").write("\n".join(log))
