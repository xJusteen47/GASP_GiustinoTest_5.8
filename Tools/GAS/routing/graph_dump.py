"""Dump named graphs of ABP_GSTN_Locomotion (nodes, pins, values, links) to Saved/Routing/graphs.txt.
Env GRAPHS = comma separated graph names (function graphs, state graphs or AnimGraph). Headless (-run=pythonscript)."""
import os, re, unreal
OUT = unreal.Paths.project_saved_dir() + "Routing/graphs.txt"
ABP = os.environ.get("ABP_PATH", "/Game/GSTN/Animation/Locomotion/ABP_GSTN_Locomotion")
WANT = [x.strip() for x in os.environ.get("GRAPHS", "").split(",") if x.strip()]
B = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object()
log = []

def short(s):
    m = re.findall(r"\.([A-Za-z0-9_]+)'", s)
    return m[-1] if m else s

def fmt(info):
    s = str(info)
    t = re.search(r'type_id: "([^"]*)"', s)
    lines = ["   type=" + (t.group(1) if t else "?")]
    for pm in re.finditer(r'\(Name="([^"]+)",TypeId="([^"]*)"(.*?)(?=,\(Name=|\), output_pins|\)\}>$|$)', s):
        name, typ, rest = pm.groups()
        if name in ("execute", "then", "self"):
            continue
        seg = "      %s:%s" % (name, typ[:28])
        v = re.search(r'Value="([^"]*)"', rest)
        if v and v.group(1):
            seg += " =" + v.group(1)[:160]
        nodes = re.findall(r"\.([A-Za-z0-9_]+)'\"", rest)[1:]
        if nodes:
            seg += " <-> " + ",".join(sorted(set(nodes)))[:200]
        lines.append(seg)
    return lines

abp = unreal.load_asset(ABP)
pkg = abp.get_outermost()
for gr in unreal.ObjectIterator(unreal.EdGraph):
    if gr.get_outermost() != pkg or gr.get_name() not in WANT:
        continue
    log.append("=== %s (outer %s)" % (gr.get_name(), gr.get_outer().get_name()))
    nodes = list(B.call_method("FindNodes", (gr, "", None, False)))
    for n, i in zip(nodes, B.call_method("GetNodeInfos", (nodes,))):
        log.append("--" + n.get_name())
        log.extend(fmt(i))
open(OUT, "w", encoding="utf-8").write("\n".join(log))
