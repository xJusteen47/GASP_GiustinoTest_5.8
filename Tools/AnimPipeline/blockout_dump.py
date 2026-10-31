# Back up the owner's blockout to Saved/AnimPipeline/jab_blockout.casc, export it raw, and dump its key poses.
import csc, json, os
app = csc.app.get_application()
vs = app.get_scene_manager().current_scene()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
if vs.get_path_name().endswith("samples/UEFN_Mannequin.casc"):
    vs.save(base + "jab_blockout.casc")
assert vs.get_path_name().endswith("jab_blockout.casc"), vs.get_path_name()
loader = app.get_tools_manager().get_tool("FbxSceneLoader").get_fbx_loader(vs)
st = csc.fbx.FbxSettings(); st.bake_animation = True; loader.set_settings(st)
loader.export_joints(base + "jab_blockout.fbx")

mv = scene.model_viewer(); dv = scene.data_viewer()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
def val(name, dname, f):
    for d in dv.get_all_data_id(ids[name]):
        try:
            if dv.get_data_name(d) == dname:
                v = dv.get_data_value(d, f)
                if hasattr(v, "to_quaternion"):
                    q = v.to_quaternion(); g = lambda x: float(x() if callable(x) else x)
                    return [round(g(q.w), 4), round(g(q.x), 4), round(g(q.y), 4), round(g(q.z), 4)]
                return [round(float(x), 2) for x in v]
        except Exception:
            pass
KEYS = [0, 1, 3, 8, 19]
points = [n for n, o in ids.items() if mv.get_object_type_name(o) in ("Point", "Limb Direction")]
joints = ["root", "pelvis", "spine_03", "spine_05", "neck_01", "head", "clavicle_r", "upperarm_r", "lowerarm_r", "hand_r",
          "clavicle_l", "upperarm_l", "lowerarm_l", "hand_l", "thigh_r", "calf_r", "foot_r", "ball_r", "thigh_l", "calf_l", "foot_l", "ball_l",
          "index_01_r", "index_02_r", "index_01_l", "index_02_l", "thumb_02_r", "thumb_02_l"]
dump = {"points": {n: {f: val(n, "Position", f) for f in KEYS} for n in points},
        "joints_pos": {n: {f: val(n, "Position", f) for f in KEYS} for n in joints},
        "joints_lrot": {n: {f: val(n, "Local Rotation", f) for f in KEYS} for n in joints}}
json.dump(dump, open(base + "blockout_dump.json", "w"), indent=1)
print(json.dumps({"saved": str(vs.get_path_name()), "fbx": os.path.getsize(base + "jab_blockout.fbx"), "n_points": len(points)}))
