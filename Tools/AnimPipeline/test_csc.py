import csc
app = csc.app.get_application()
scene = app.get_scene_manager().current_scene()
print("Current scene in Cascadeur:", scene.get_path_name() if scene else "None")
