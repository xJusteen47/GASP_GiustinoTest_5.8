# Cascadeur side of `animpipe casc-export` (run in Cascadeur: the cascadeur-plus run_python tool, or csc_run.py).
# Needs PLAN = path of Saved/AnimPipeline/casc/<move>_export.json (written by casc-export) and, as the CURRENT tab,
# a scene on the UEFN mannequin rig that may be overwritten (a copy, never an owner file).
# 1. imports the card's take FBX onto the selected joints: a plain import_animation of an Unreal FBX leaves a 1-frame
#    scene, and without up_axis Z the character lies on its back (Unreal is Z-up);
# 2. deletes every key that is not a card key frame (sparse keys, owner 2026-11-02);
# 3. clamped bezier on every section (Cascadeur keeps STEP on 1-frame sections and on the last key: no in-betweens);
# 4. saves the tab as plan["casc"] and exports the joints (baked) to plan["casc_fbx"] for the --ref readback.
import csc, json, os
from collections import Counter
P = json.load(open(globals()["PLAN"]))
app = csc.app.get_application(); sm = app.get_scene_manager(); tab = sm.current_scene(); ds = tab.domain_scene()
mv = ds.model_viewer(); lv = ds.layers_viewer()
assert "/Saved/AnimPipeline/casc/" in str(tab.get_path_name()), tab.get_path_name()
ds.selector().select({o for o in mv.get_objects() if mv.get_object_type_name(o) == "Joint"})
loader = app.get_tools_manager().get_tool("FbxSceneLoader").get_fbx_loader(tab)
st = csc.fbx.FbxSettings(); st.up_axis = csc.fbx.FbxSettingsAxis.Z; loader.set_settings(st)
loader.import_animation_to_selected_objects(P["fbx"])
ds.selector().select(set())
keep = set(P["keys"])
I = csc.layers.layer.Interpolation


def thin(model, update, su):
    le = model.layers_editor()
    for lid in lv.all_layer_ids():
        for f in sorted(lv.layer(lid).key_frame_indices()):
            if f not in keep:
                le.unset_section(f, lid)


def clamped(model, update, su):
    le = model.layers_editor()
    for lid in lv.all_layer_ids():
        for f in lv.layer(lid).key_frame_indices():
            le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", I.CLAMPED_BEZIER))


ok = [ds.modify_update("casc thin keys", thin), ds.modify_update("casc clamped", clamped)]
keys = sorted({f for l in lv.all_layer_ids() for f in lv.layer(l).key_frame_indices()})
tab.save(P["casc"])
st = csc.fbx.FbxSettings(); st.bake_animation = True; loader.set_settings(st)
loader.export_joints(P["casc_fbx"])
result = {"ok": ok, "size": ds.data_viewer().get_animation_size(), "keys": keys, "match": keys == sorted(keep),
          "interp": dict(Counter(str(lv.layer(l).interval(f).interpolation).split(".")[-1] for l in lv.all_layer_ids() for f in keys)),
          "casc": P["casc"], "fbx_bytes": os.path.getsize(P["casc_fbx"])}
print(json.dumps(result))
