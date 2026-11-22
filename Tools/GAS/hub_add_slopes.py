"""Adds the Z7 slope zone to /Game/Levels/Test/L_Test_Hub and SAVES the map (the only map threads may save, coordinator 2026-11-19).

Headless, through the owner-allowed save route:
  & .\Tools\CI\Save-Assets.ps1 -Script Tools\GAS\hub_add_slopes.py -Tag hub_slopes
Dry run (no GSTN_SAVE=1): builds the zone in memory and reports, nothing saved. Prints max 5 lines.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_test_hub_map as hub  # noqa: E402

save = os.environ.get('GSTN_SAVE') == '1'
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ok = les.load_level(hub.MAP_PATH)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
path = world.get_path_name().split('.')[0] if world else None
if not ok or path != hub.MAP_PATH:
    print(f'hub_add_slopes: ERRORE map {path}')
else:
    hub.z7_only()
    n = len([a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label().startswith('Z7_')])
    saved = bool(les.save_current_level()) if save else None
    print(f'hub_add_slopes: {n} Z7 actors, saved {saved}')
