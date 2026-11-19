"""Route our states of ABP_GSTN_Locomotion to the visible pose (routing fix, 2026-11-19). Idempotent: run it after any script that
(re)builds our states (crouch_states.py, run_stop_states.py, the run start states).

Why: the "State Controller" machine of GASP's experimental setup outputs NO pose; the visible pose is the Blend Stack, written on state
entry by SetBlendStackAnimFromChooser keyed by the state name (root chooser CHT_GSTN_LocomotionAnimations, column StateName).
What it does, for every state in OUR_STATES that exists:
  1. empties the state's graph (the old Sequence Evaluators were never visible) and binds its On State Entry to the C++
     UGSTN_LocomotionStateLibrary::SetBlendStackFromStateName (same call as a GASP state, key = state name, "Run Stop" -> "Run Stop L/R");
  2. one chooser of our clips, CHT_GSTN_OwnStates (same signature as the root): column StateName + output struct (start time, blend time,
     curve, profile), one row per key with the clip read from DA_AnimSet_Locomotion_Default (so the row and the C++ timing use one clip);
  3. in the root chooser, one row per key -> Evaluate Chooser CHT_GSTN_OwnStates (added once, kept in sync).
Report: Saved/Routing/route_states.txt. Headless: copy to a path without spaces, -run=pythonscript."""
import json
import unreal

OUT = unreal.Paths.project_saved_dir() + "Routing/route_states.txt"
LOC = "/Game/GSTN/Animation/Locomotion/"
ABP_PATH = LOC + "ABP_GSTN_Locomotion"
ROOT_CHT = LOC + "CHT_GSTN_LocomotionAnimations"
OWN_CHT_NAME = "CHT_GSTN_OwnStates"
DA_PATH = "/Game/GSTN/Animation/Locomotion/DA_AnimSet_Locomotion_Default"
LIB_CLASS = "/Script/GSTNCombatNP.GSTN_LocomotionStateLibrary"
ENTRY_FN = "SetBlendStackFromStateName"
CROUCH_IDLE = "/Game/GSTN/Animation/Crouch/AS_GSTN_Crouch_Idle.AS_GSTN_Crouch_Idle"
OUR_STATES = ["Skid", "Run Stop", "Run Stop Blend", "Crouch Enter", "Crouch Idle", "Crouch Exit", "Run Start"]

log = []
def p(*a): log.append(" ".join(str(x) for x in a))

G = unreal.GSTN_AnimBlueprintToolset.get_default_object()
B = unreal.load_class(None, "/Script/BlueprintToolsets.BlueprintToolset").get_default_object()
O = unreal.load_class(None, "/Script/EditorToolset.ObjectToolset").get_default_object()
C = unreal.GSTN_ChooserTableToolset.get_default_object()
def g(fn, *a): return G.call_method(fn, tuple(a))
def b(fn, *a): return B.call_method(fn, tuple(a))
def o(fn, *a): return O.call_method(fn, tuple(a))
def c(fn, *a): return C.call_method(fn, tuple(a))

def soft_path(struct, field):
    v = struct.get_editor_property(field)
    s = str(v.get_path_name()) if hasattr(v, "get_path_name") else str(v)
    if "'" in s:
        s = s.split("'")[1]
    return s

def out_cell(start, blend, curve="QuadraticInOut", profile=""):
    # Field names of the GASP struct S_CHT_MoverCharacterAnimations_OUT (copied from its rows)
    return ("(StartTime_6_43E5D7A74934F395460447B8CF09CCD0=%f,BlendTime_17_03484D0C47973BE17EEED696F1BA5A71=%f,"
            "BlendCurve_23_8AC89E2A450F58ED51D5AA82C99A579A=%s,BlendProfile_20_AEB69035499C59E1822DAABD551E8A04=\"%s\","
            "Tags_10_A4BB08A3498365A2555AB9BD0C25C6D6=)" % (start, blend, curve, profile))

def find_da():
    da = unreal.load_asset(DA_PATH)
    if da:
        return da
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in reg.get_assets_by_class(unreal.TopLevelAssetPath("/Script/GSTNCombatNP", "GSTN_LocomotionAnimSet")):
        if "Default" in str(a.asset_name):
            return a.get_asset()
    return None

def rows_spec(da):
    """key -> (clip path, start time, blend time, looping)"""
    rs, cr, sk = da.get_editor_property("run_stop"), da.get_editor_property("crouch"), da.get_editor_property("skid")
    return {
        # blend times = the cross-fades the old state transitions had (handovers skid-main-graph-1, crouch-veloce-2, run-stop-1)
        "Skid": (soft_path(sk, "clip"), 0.0, 0.08, False),
        "Run Stop L": (soft_path(rs, "clip_left_foot"), 0.0, 0.1, False),
        "Run Stop R": (soft_path(rs, "clip_right_foot"), 0.0, 0.1, False),
        "Run Stop Blend": (soft_path(rs, "idle_clip"), 0.0, rs.get_editor_property("blend_seconds"), True),
        "Crouch Enter": (soft_path(cr, "enter_clip"), 0.0, 0.03, False),
        "Crouch Idle": (CROUCH_IDLE, 0.0, 0.05, True),
        "Crouch Exit": (soft_path(cr, "exit_clip"), 0.0, 0.03, False),
    }

def set_loop(asset, looping):
    for name in ("loop", "b_loop"):
        try:
            if asset.get_editor_property(name) != looping:
                asset.set_editor_property(name, looping)
                unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
                p("   loop ->", looping, asset.get_name())
            return
        except Exception:
            continue
    p("   loop flag not settable on", asset.get_name())

def own_chooser(root):
    path = LOC + OWN_CHT_NAME
    cht = unreal.load_asset(path)
    if not cht:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        cht = tools.create_asset(OWN_CHT_NAME, LOC[:-1], unreal.ChooserTable, unreal.ChooserTableFactory())
        for prop in ("result_type", "output_object_type", "context_data"):
            cht.set_editor_property(prop, root.get_editor_property(prop))
        p("created", path)
    info = c("GetChooserTable", cht)
    cols = list(info.columns)
    if not cols:
        c("AddChooserColumn", cht, "ChooserNameColumn")
        c("SetChooserColumnInput", cht, 0, "NameContextProperty", 0, ["StateName"])
        oc = c("AddChooserColumn", cht, "OutputStructColumn")
        c("SetChooserColumnInput", cht, oc, "StructContextProperty", 1, [])
        p("columns added")
    return cht

def sync_rows(cht, spec, key_cell, extra_cells):
    """One row per key; existing rows are found by their first cell"""
    info = c("GetChooserTable", cht)
    existing = {}
    for i, r in enumerate(info.rows):
        cells = list(r.cells)
        if cells:
            for k in spec:
                if '"%s"' % k in cells[0]:
                    existing[k] = i
    for k, v in spec.items():
        row = existing.get(k)
        if row is None:
            row = c("AddChooserRow", cht, v["type"], None, None)
        c("SetChooserCell", cht, 0, row, key_cell(k))
        c("SetChooserRowResult", cht, row, v["type"], v["props"])
        for col, val in extra_cells(k):
            c("SetChooserCell", cht, col, row, val)
        p("   row", row, k, v["props"])

def route_state(machine, name):
    try:
        st = g("FindState", machine, name)
    except Exception:
        return False
    sg = next(x for x in unreal.ObjectIterator(unreal.EdGraph) if x.get_outer() == st)
    result = None
    for n in list(b("FindNodes", sg, "", None, False)):
        if "StateResult" in n.get_class().get_name():
            result = n
        else:
            b("DeleteNode", n)
    ok = o("SetProperties", result, json.dumps({"StateEntryFunction": {
        "memberParent": {"refPath": LIB_CLASS}, "memberName": ENTRY_FN, "bSelfContext": False}}))
    back = o("GetProperties", result, ["StateEntryFunction"])
    p("state", name, "entry set", ok, back[:160])
    note = ("Pose: Blend Stack, chooser row '%s' in %s (UGSTN_LocomotionStateLibrary::SetBlendStackFromStateName). "
            "This machine outputs no pose: a node here would never be seen." % (name if name != "Run Stop" else "Run Stop L / R", OWN_CHT_NAME))
    unreal.BlueprintEditorLibrary.set_node_comment(st, note)
    return True

try:
    abp = unreal.load_asset(ABP_PATH)
    root = unreal.load_asset(ROOT_CHT)
    da = find_da()
    p("DA", da.get_path_name() if da else None, "main class", da.get_editor_property("main_anim_class") if da else None,
      "skid in main", da.get_editor_property("skid_in_main_graph") if da else None)
    machine = next(i.machine for i in g("FindStateMachines", abp, "") if i.name == "State Controller")

    routed = [s for s in OUR_STATES if route_state(machine, s)]
    p("routed", routed)

    spec = rows_spec(da)
    own = own_chooser(root)
    own_rows = {}
    for k, (clip, start, blend, looping) in spec.items():
        asset = unreal.load_asset(clip)
        if not asset:
            p("MISSING clip", k, clip); continue
        set_loop(asset, looping)
        own_rows[k] = {"type": "AssetChooser", "props": {"Asset": clip}, "cell": out_cell(start, blend)}
    sync_rows(own, own_rows, lambda k: '(Comparison=MatchEqual,Value="%s")' % k, lambda k: [(1, own_rows[k]["cell"])])

    own_ref = own.get_path_name()
    root_rows = {k: {"type": "EvaluateChooser", "props": {"Chooser": own_ref}} for k in own_rows}
    sync_rows(root, root_rows, lambda k: '(Comparison=MatchEqual,Value="%s")' % k, lambda k: [])

    for t in (own, root):
        p("save", t.get_name(), unreal.EditorAssetLibrary.save_loaded_asset(t, False))
    ok = unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    status = abp.get_editor_property("status")
    p("compile", ok, status)
    if status != unreal.BlueprintStatus.BS_ERROR:
        p("save abp", unreal.EditorAssetLibrary.save_loaded_asset(abp, False))
    else:
        p("NOT SAVED: compile error")
    p("own table", str(c("GetChooserTable", own))[:4000])
except Exception:
    import traceback; p("ERROR", traceback.format_exc())
open(OUT, "w", encoding="utf-8").write("\n".join(log))
