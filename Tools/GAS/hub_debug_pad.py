# One debug pad at the spawn of L_Test_Hub that opens the GSTN Debug Options editor window (owner 2026-11-16, like GASP's level button in its base
# map). It replaces the single-purpose debug pads (the old red "DEBUG HITBOX/HURTBOX" toggle and any other Z0_Pad_Debug* actor). Reproducible:
# it loads the hub, removes the old debug pads, places the new pad and its sign, and saves ONLY L_Test_Hub (owner exception to "never save maps").
# Kit pads (kit pickups) are not debug pads and stay. Run in the editor's Python console, editor not in Play:
#
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import hub_debug_pad as p; p.run()        # p.run(save=False) to look first
#
# Report: Saved/TestHub/debug_pad.txt (removed actor labels, the new pad). Standing on the pad in Play calls gstn.Debug.OpenWindow (test_hub watcher).
import os

import unreal

import build_test_hub_map as hub

OLD_PREFIXES = ('Z0_Pad_Debug',)
NEW_PAD = 'Z0_Pad_DebugOptions'


def _actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def run(save=True):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if world is None or world.get_path_name().split('.')[0] != hub.MAP_PATH:
        les.load_level(hub.MAP_PATH)
    removed = []
    for actor in list(_actors()):
        label = actor.get_actor_label()
        if label.startswith(OLD_PREFIXES) or label.startswith(NEW_PAD):
            removed.append(label)
            actor.destroy_actor()
    label, (x, y) = hub.DEBUG_PAD
    hub.box(NEW_PAD, (x, y, 5.0), (hub.PAD_RADIUS * 2.0, hub.PAD_RADIUS * 2.0, 10.0), 'Red', mesh=hub.CYLINDER)
    hub.sign(NEW_PAD + '_Text', label + chr(10) + 'salici: si apre la finestra', (x + 30.0, y), face_to=(0.0, y), size=24.0, height=170.0)
    saved = bool(les.save_current_level()) if save else False
    out = os.path.join(unreal.Paths.project_saved_dir(), 'TestHub', 'debug_pad.txt')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', encoding='utf-8') as f:
        f.write('removed: ' + ', '.join(sorted(removed)) + '\n')
        f.write(f'new: {NEW_PAD} at ({x}, {y}); saved={saved}\n')
    unreal.log(f'[hub_debug_pad] removed {len(removed)}, new pad at ({x}, {y}), saved={saved}')
    return removed, saved
