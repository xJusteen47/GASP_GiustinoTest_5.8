import csc, json, os
app = csc.app.get_application()
vs = app.get_scene_manager().current_scene()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
assert vs.get_path_name().endswith("jab_test.casc"), vs.get_path_name()
vs.save(base + "jab_test.casc")
loader = app.get_tools_manager().get_tool("FbxSceneLoader").get_fbx_loader(vs)
st = csc.fbx.FbxSettings()
defaults = {"bake": st.bake_animation, "euler": st.apply_euler_filter, "mode": str(st.mode), "up": str(st.up_axis)}
st.bake_animation = True
st.mode = csc.fbx.FbxSettingsMode.Binary
loader.set_settings(st)
out = base + "jab_test.fbx"
loader.export_joints(out)
print(json.dumps({"defaults": defaults, "exported": out, "exists": os.path.exists(out),
                  "size": os.path.getsize(out) if os.path.exists(out) else 0}))
