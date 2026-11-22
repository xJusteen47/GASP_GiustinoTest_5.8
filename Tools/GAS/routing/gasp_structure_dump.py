"""Read-only dump of GASP Mover assets (ABP state machine + all graphs, chooser tables incl. every nested table, Pose Search DBs). Headless UE 5.9:
  Run-Timed.ps1 -Kind Test -Exe UnrealEditor-Cmd.exe -ArgLine "<uproject> -run=pythonscript -script=<copy of this file in a path without spaces> -unattended -nullrhi"
Env: ABP_PATH, CHOOSERS (comma list), TAG (file prefix), ONLY=choosers. Writes Saved/GaspStructure/<TAG>_*.txt. Format choosers with gasp_cht_fmt.py <in> <out>.
Used by docs/knowledge/gasp/gasp-structure-5.9.md (thread gasp-structure-1)."""
import os, re, json, unreal
OUTD = unreal.Paths.project_saved_dir() + "GaspStructure/"
os.makedirs(OUTD, exist_ok=True)
ABP = os.environ.get("ABP_PATH", "/Game/Blueprints/SandboxCharacter_Mover_ABP")
ONLY = os.environ.get("ONLY", "")
TAG = os.environ.get("TAG", "stock")
G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
B = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object()
O = unreal.load_class(None, "/Script/EditorToolset.ObjectToolset").get_default_object()
C = unreal.GSTN_ChooserTableToolset.get_default_object()
files = {}
def p(f, *a): files.setdefault(f, []).append(" ".join(str(x) for x in a))
def safe(fn, *a):
    try: return fn(*a)
    except Exception as e: return "ERR %s" % e
def g(fn, *a): return G.call_method(fn, tuple(a))
def b(fn, *a): return B.call_method(fn, tuple(a))
def o(fn, *a): return O.call_method(fn, tuple(a))

def short(s):
    return re.sub(r"/Script/[A-Za-z]+\.([A-Za-z_]+)'(/[^']+)'", lambda m: m.group(2).split(".")[-1], s)

def fmt_node(info):
    s = str(info)
    t = re.search(r'type_id: "([^"]*)"', s)
    out = ["   type=" + (t.group(1) if t else "?")]
    for pm in re.finditer(r'\(Name="([^"]+)",TypeId="([^"]*)"(.*?)(?=,\(Name=|\), output_pins|\)\}>$|$)', s):
        name, typ, rest = pm.groups()
        if name in ("execute", "then", "self"): continue
        seg = "      %s:%s" % (name, typ[:28])
        v = re.search(r'Value="([^"]*)"', rest)
        if v and v.group(1): seg += " =" + v.group(1)[:200]
        nodes = re.findall(r"\.([A-Za-z0-9_]+)'\"", rest)[1:]
        if nodes: seg += " <-> " + ",".join(sorted(set(nodes)))[:240]
        out.append(seg)
    return out

def dump_graph(f, gr, label):
    p(f, "=== %s %s (outer %s)" % (label, gr.get_name(), gr.get_outer().get_name()))
    nodes = safe(b, "FindNodes", gr, "", None, False)
    if isinstance(nodes, str): p(f, nodes); return
    nodes = list(nodes)
    infos = safe(b, "GetNodeInfos", nodes)
    if isinstance(infos, str): p(f, infos); return
    for n, i in zip(nodes, infos):
        p(f, "--" + n.get_name() + " [" + n.get_class().get_name() + "]")
        for l in fmt_node(i): p(f, l)

abp = unreal.load_asset(ABP)
pkg = abp.get_outermost()
graphs = [x for x in unreal.ObjectIterator(unreal.EdGraph) if x.get_outermost() == pkg]
p("abp_index", "ABP", ABP, "graphs", len(graphs))
for gr in sorted(graphs, key=lambda x: (x.get_outer().get_name(), x.get_name())):
    p("abp_index", "graph", gr.get_name(), "| outer", gr.get_outer().get_name(), "|", gr.get_outer().get_class().get_name())
# all graphs, full
for gr in ([] if ONLY == 'choosers' else graphs):
    dump_graph("abp_graphs", gr, "GRAPH")
# state machines
choosers = set()
for info in safe(g, "FindStateMachines", abp, "") or []:
    d = safe(g, "GetStateMachine", info.machine)
    p("abp_sm", "=== MACHINE", str(info)[:600])
    if isinstance(d, str): p("abp_sm", d); continue
    for s in d.states:
        p("abp_sm", "STATE", s.name, "|", str(s)[:400])
        try:
            sg = next(x for x in unreal.ObjectIterator(unreal.EdGraph) if x.get_outer() == s.state)
            for n in list(b("FindNodes", sg, "", None, False)):
                cn = n.get_class().get_name()
                if "StateResult" in cn:
                    p("abp_sm", "   result fns:", str(safe(o, "GetProperties", n, ["StateEntryFunction", "StateExitFunction", "StateFullyBlendedInFunction", "StateFullyBlendedOutFunction"]))[:900])
                else:
                    p("abp_sm", "   node:", cn, n.get_name())
        except StopIteration:
            pass
        except Exception as e:
            p("abp_sm", "   ERR", e)
    for t in d.transitions:
        p("abp_sm", "TR", str(t)[:900])

# choosers, recursive through nested / referenced tables
seen = set()
def dump_cht(path, depth=0):
    if path in seen or depth > 4: return
    seen.add(path)
    cht = unreal.load_asset(path)
    if not cht: p("choosers", "MISSING", path); return
    info = safe(C.call_method, "GetChooserTable", (cht,))
    p("choosers", "=== CHOOSER", path)
    p("choosers", short(str(info)))
    for m in re.findall(r"ChooserTable'(/Game/[^']+)'", str(info)):
        dump_cht(m.split(".")[0], depth + 1)
    # nested tables (sub-objects)
    for sub in unreal.ObjectIterator(unreal.ChooserTable):
        if sub.get_outermost() == cht.get_outermost() and sub != cht:
            nm = sub.get_path_name()
            if nm in seen: continue
            seen.add(nm)
            p("choosers", "=== NESTED", nm)
            p("choosers", short(str(safe(C.call_method, "GetChooserTable", (sub,)))))
for path in os.environ.get("CHOOSERS", "/Game/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_MoverCharacterAnimations_PoseMatch,/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Mover,/Game/Blueprints/Data/CHT_RotationOffsetCurve").split(","):
    dump_cht(path)

# pose search databases / schemas used by the state-machine tables + the Mover MM chooser
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for folder in ["/Game/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData", "/Game/Characters/UEFN_Mannequin/Animations/MotionMatchingData"]:
    for a in ar.get_assets_by_path(folder, recursive=True):
        cls = str(a.asset_class_path.asset_name)
        if cls not in ("PoseSearchDatabase", "PoseSearchSchema", "PoseSearchNormalizationSet"): continue
        obj = a.get_asset()
        p("posesearch", "=== %s %s" % (cls, a.package_name))
        names = [x for x in str(safe(o, "ListProperties", obj, "", "")).split(",")][:200]
        p("posesearch", "  props:", short(str(safe(o, "ListProperties", obj, "", "")))[:3000])
        if cls == "PoseSearchDatabase":
            try:
                p("posesearch", "  schema:", obj.get_editor_property("schema").get_name())
            except Exception as e: p("posesearch", "  schema ERR", e)
            try:
                n = obj.get_num_animation_assets()
                p("posesearch", "  num anims:", n)
                for i in range(n):
                    aa = obj.get_animation_asset(i)
                    p("posesearch", "   ", aa.get_name() if aa else None)
            except Exception as e: p("posesearch", "  anims ERR", e)
        else:
            p("posesearch", "  get:", short(str(safe(o, "GetProperties", obj, ["SampleRate", "Channels", "NumberOfPermutations", "PermutationsSampleRate", "DataPreprocessor", "Skeletons"])))[:6000])

for f, lines in files.items():
    open(OUTD + TAG + "_" + f + ".txt", "w", encoding="utf-8").write("\n".join(lines))
open(OUTD + TAG + "_done.txt", "w").write("ok %d files" % len(files))
