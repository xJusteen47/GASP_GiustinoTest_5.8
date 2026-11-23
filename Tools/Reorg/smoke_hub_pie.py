"""Smoke after the Content move: the editor opened the startup map (L_Test_Hub, now under /Game/GSTN/Maps), PIE runs 8 s, quit.
Writes Saved/Logs/ci/reorg_smoke.txt: SMOKE: OK|FAIL <map> <pawn>."""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Saved', 'Logs', 'ci', 'reorg_smoke.txt')
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state = {'t': 0.0, 'started': False, 'done': False}


def write(msg):
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write(msg + '\n')


def tick(dt):
    if state['done']:
        return
    state['t'] += dt
    if not state['started'] and state['t'] > 5.0:
        les.editor_request_begin_play()
        state['started'] = True
    elif state['started'] and state['t'] > 15.0:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        pawn = None
        if world:
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            pawn = pc.get_controlled_pawn() if pc else None
        wname = world.get_path_name() if world else ''
        ok = bool(pawn) and '/Game/GSTN/Maps/' in wname
        write(f"SMOKE: {'OK' if ok else 'FAIL'} map={wname} pawn={pawn.get_class().get_name() if pawn else None}")
        state['done'] = True
        les.editor_request_end_play()
        unreal.SystemLibrary.quit_editor()


write('SMOKE: MANCANTE (non finito)')
unreal.register_slate_post_tick_callback(tick)
