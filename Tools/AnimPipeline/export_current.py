# Export a Cascadeur tab (by .casc file name in Saved/AnimPipeline) as a baked FBX with the same base name.
# The FBX exporter follows the CURRENT tab, so make the target tab current first and restore the previous one after.
# Set TAB (and optionally LOAD=True to open the file first) before running; defaults to the MCP scene's tab.
import csc, json, os
app = csc.app.get_application(); sm = app.get_scene_manager()
base = (os.environ.get("ANIMPIPELINE_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.getcwd())).replace("\\", "/").rstrip("/") + "/"
TAB = globals().get("TAB") or [s for s in sm.scenes() if s.domain_scene() == scene][0].name()
if globals().get("LOAD") and not any(s.name() == TAB for s in sm.scenes()):
    app.get_data_source_manager().load_scene(base + TAB)
prev = sm.current_scene()
tab = [s for s in sm.scenes() if s.name() == TAB][0]
sm.set_current_scene(tab)
path = str(tab.get_path_name())
assert "/Saved/AnimPipeline/" in path, path
if not globals().get("LOAD"):
    tab.save(path)
fbx = path[:-5] + ".fbx"
loader = app.get_tools_manager().get_tool("FbxSceneLoader").get_fbx_loader(tab)
st = csc.fbx.FbxSettings(); st.bake_animation = True; loader.set_settings(st)
loader.export_joints(fbx)
size = tab.domain_scene().data_viewer().get_animation_size()
sm.set_current_scene(prev)
print(json.dumps({"tab": TAB, "fbx": fbx, "bytes": os.path.getsize(fbx), "frames": size, "current_after": sm.current_scene().name()}))
