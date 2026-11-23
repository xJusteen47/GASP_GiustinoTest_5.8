"""Read-only dump of ABP_GSTN_Locomotion's state machine (states, aliases, transitions) and of the Skid state's graph, to wire the run stop
like the skid. Report -> Saved/Logs/ci/run_stop_inspect.txt"""
import unreal
OUT = unreal.Paths.project_saved_dir() + "Logs/ci/run_stop_inspect.txt"
log = []
def p(*a): log.append(" ".join(str(x) for x in a))
try:
    _cdo = unreal.GSTN_AnimBlueprintToolset.get_default_object()
    class _T:
        def __getattr__(self, name):
            camel = "".join(w.capitalize() for w in name.split("_"))
            return lambda *a: _cdo.call_method(camel, tuple(a))
    T = _T()
    abp = unreal.load_asset("/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
    for info in T.find_state_machines(abp, ""):
        d = T.get_state_machine(info.machine)
        p("MACHINE", d.name, "path", info.get_editor_property("graph_path") if hasattr(info, "graph_path") else info)
        if not any(s.name == "Skid" for s in d.states):
            continue
        for s in d.states:
            p("  state", s.name, s.kind, getattr(s, "animation", ""))
        for t in d.transitions:
            p("  tr", t.from_, "->", t.to, "|", t.rule, "| xf", round(t.crossfade_duration, 3))
        skid = T.find_state(info.machine, "Skid")
        g = skid.get_editor_property("bound_graph")
        for n in g.get_editor_property("nodes"):
            p("  skid node", n.get_class().get_name(), n.get_name())
            try:
                node = n.get_editor_property("node")
                p("     anim node props:", node)
            except Exception:
                pass
        for gr in T.list_anim_graphs(abp):
            p("graph", gr)
except Exception:
    import traceback; p("ERROR", traceback.format_exc())
open(OUT, "w").write("\n".join(log))
unreal.SystemLibrary.quit_editor()
