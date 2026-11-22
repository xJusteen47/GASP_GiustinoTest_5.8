"""Dump how ABP_GSTN_Locomotion routes the visible pose (routing fix thread, 2026-11-19).
Writes Saved/Routing/dump.txt: AnimGraph nodes + bindings, State Controller states with their On State Entry functions,
the OnStateEntry_* / SetBlendStackAnimFromChooser function graphs, the chooser CHT_GSTN_LocomotionAnimations, the state enum.
Headless: copy to a path without spaces and run with -run=pythonscript."""
import unreal
OUT = unreal.Paths.project_saved_dir() + "Routing/dump.txt"
ABP = "/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion"
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
B = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object()
O = unreal.load_class(None, "/Script/EditorToolset.ObjectToolset").get_default_object()
C = unreal.GSTN_ChooserTableToolset.get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
def b(fn, *a): return B.call_method(fn, tuple(a))
def o(fn, *a): return O.call_method(fn, tuple(a))

def safe(f, *a):
    try:
        return f(*a)
    except Exception as e:
        return "ERR %s" % e

def dump_graph(graph, label):
    p("=== GRAPH", label, graph.get_name())
    nodes = safe(b, "FindNodes", graph, "", None, False)
    if isinstance(nodes, str):
        p(nodes); return
    nodes = list(nodes)
    infos = safe(b, "GetNodeInfos", nodes)
    if isinstance(infos, str):
        p(infos); return
    for n, i in zip(nodes, infos):
        p("--NODE", n.get_name(), n.get_class().get_name())
        p("   ", str(i)[:3000])

def props(obj, names):
    return safe(o, "GetProperties", obj, names)

try:
    abp = unreal.load_asset(ABP)
    graphs = [x for x in unreal.ObjectIterator(unreal.EdGraph) if x.get_outermost() == abp.get_outermost()]
    p("graph count", len(graphs))
    p("graph names", sorted(set(x.get_name() for x in graphs)))
    # top-level AnimGraph
    for gr in graphs:
        if gr.get_name() == "AnimGraph" and gr.get_outer() == abp:
            dump_graph(gr, "TOP")
            for n in list(b("FindNodes", gr, "", None, False)):
                cn = n.get_class().get_name()
                if any(k in cn for k in ("TwoWayBlend", "BlendListByInt", "BlendStack", "BlendListByBool")):
                    p("--PROPS", n.get_name(), cn)
                    p("   list:", safe(o, "ListProperties", n, "", "", ))
                    p("   get:", props(n, ["Node", "Binding", "PropertyBindings", "ShowPinForProperties"])[:6000])
    # functions of interest
    for gr in graphs:
        nm = gr.get_name()
        if nm.startswith("OnStateEntry") or "BlendStack" in nm or "Chooser" in nm:
            if gr.get_outer() == abp:
                dump_graph(gr, "FUNC")
    # state controller
    for info in g("FindStateMachines", abp, ""):
        d = g("GetStateMachine", info.machine)
        p("=== MACHINE", str(info)[:400])
        for s in d.states:
            p("STATE", s.name, str(s)[:300])
            try:
                sg = next(x for x in unreal.ObjectIterator(unreal.EdGraph) if x.get_outer() == s.state)
                for n in list(b("FindNodes", sg, "", None, False)):
                    if "StateResult" in n.get_class().get_name():
                        p("   entry:", props(n, ["StateEntryFunction", "StateExitFunction", "StateFullyBlendedInFunction"])[:800])
                    else:
                        p("   node:", n.get_class().get_name(), n.get_name())
            except StopIteration:
                pass
        for t in d.transitions:
            p("TR", t.from_, "->", t.to, "|", t.rule, "| xf", round(t.crossfade_duration, 3))
    # chooser
    for path in ["/Game/GSTN/Animation/Locomotion/CHT_GSTN_LocomotionAnimations"]:
        cht = unreal.load_asset(path)
        if not cht:
            for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_package_name(path):
                p("asset", a)
        p("=== CHOOSER", path, cht)
        if cht:
            p(str(safe(C.call_method, "GetChooserTable", (cht,)))[:30000])
    en = unreal.load_object(None, "/Game/Blueprints/Data/E_ExperimentalStateMachineState.E_ExperimentalStateMachineState")
    p("=== ENUM", props(en, ["DisplayNameMap"])[:3000])
except Exception:
    import traceback; p("ERROR", traceback.format_exc())
open(OUT, "w", encoding="utf-8").write("\n".join(log))
