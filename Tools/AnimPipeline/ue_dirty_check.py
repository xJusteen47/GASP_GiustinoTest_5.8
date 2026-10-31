# Editor-side, read-only: list dirty (unsaved) map and content packages.
import unreal, json

def main():
    maps = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    content = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    return json.dumps({"dirty_maps": maps, "dirty_content": content, "world": world.get_path_name() if world else None})

def quit_editor():
    unreal.SystemLibrary.quit_editor()
    return "quit requested"
