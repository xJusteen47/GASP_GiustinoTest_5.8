import csc, json, os
app = csc.app.get_application(); sm = app.get_scene_manager()
v1 = [s for s in sm.scenes() if s.name() == "jab_v1.casc"][0]
sm.set_current_scene(v1)
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
tool = app.get_tools_manager().get_tool("FbxSceneLoader")
loader = tool.get_fbx_loader(v1)
st = csc.fbx.FbxSettings(); st.bake_animation = True; loader.set_settings(st)
loader.export_joints(base + "test_a.fbx")
tool.export_fbx_scene(v1, base + "test_b.fbx")
print(json.dumps({"a": os.path.getsize(base + "test_a.fbx"), "b": os.path.getsize(base + "test_b.fbx"), "current": sm.current_scene().name()}))
