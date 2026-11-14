# Wires the victim reaction animations (react-v3 clips in /Game/GASNPPTest/Combat/Reaction) into the game, by data only. Idempotent.
# Owner 2026-11-14: "state -> animation via data, not hardcoded"; no manual Blueprint wiring.
#
# Run with the editor CLOSED (headless), after a build that contains UGSTN_ReactionAnimComponent and UGSTN_GA_GetupRoll travel:
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_reaction_anims.py" -unattended -nullrhi
# It prints at most 5 lines; the full log is Saved/Reaction/setup_reaction_anims.log.
#
# What it does:
#  1. DA_ReactionAnims (UGSTN_ReactionAnimSet) in the Reaction folder, one rule per state (priority high to low):
#       Knockdown (Combat.State.Knockdown): AS_React_GroundHit then AS_React_LieUp held (chained inside the component: no game pose between them)
#       Splat (Combat.State.Splat): AS_React_Splat held        Tumble (Combat.State.Tumble): AS_React_Tumble looped
#       HitStand (Combat.State.Hitstun): AS_React_HitStand, blends out at its end, restarts on Combat.Event.Hit.Received
#  2. UGSTN_ReactionAnimComponent on /Game/Blueprints/SandboxCharacter_Mover (BP_GASNPP_TestCharacter inherits it) with that set.
#  3. Get-up abilities (/GSTNDefence/Test): GA_GetupNeutral_Test plays AM_React_GetUp, GA_GetupRoll_Test AM_React_Roll (80 cm toward the feet),
#     new GA_GetupRollBack_Test AM_React_RollBack (65 cm toward the head); timelines as long as the clips; on the end the last pose is HELD
#     while it blends out over HOLD_BLEND_FRAMES (the hunched crouch after a get-up: the game pose under it has that long to settle).
#     GA_GroundBounce_Test plays nothing (the Knockdown rule does the ground hit).
#  4. Every KIT_* under /Game/GASNPPTest that has a get-up roll gets GA_GetupRollBack_Test as its roll back.
import math
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_defence_link_content as dl  # noqa: E402  (the shared asset helpers: _Asset, _ability, _tag)

REACT = '/Game/GASNPPTest/Combat/Reaction'
SET_NAME = 'DA_ReactionAnims'
SANDBOX_CHAR = '/Game/Blueprints/SandboxCharacter_Mover'
DEFENCE_TEST = '/GSTNDefence/Test'
KIT_ROOT = '/Game/GASNPPTest'
SIM_HZ = 62.5
HOLD_BLEND_FRAMES = 20      # simulation frames the held last pose of a get-up blends out over (crouch mitigation; 0-30 to tune)

# name, required, blocked, priority, animation, then, end, blend in, chain blend, blend out, restart event (60 fps frames)
# Owner rule 2026-11-14: the first frame of a new animation/state must read as a clear change (feel): blend in 0-1 frames, never soft.
# Long blends only where a join would otherwise pop to the game pose (the end of a reaction, HOLD_BLEND_FRAMES).
RULES = [
    ('Knockdown', 'Combat.State.Knockdown', None, 40, 'AS_React_GroundHit', 'AS_React_LieUp', 'HOLD_LAST_POSE', 1, 1, 6, None),
    ('Splat', 'Combat.State.Splat', None, 30, 'AS_React_Splat', None, 'HOLD_LAST_POSE', 0, 1, 6, None),
    ('Tumble', 'Combat.State.Tumble', None, 20, 'AS_React_Tumble', None, 'LOOP', 1, 1, 6, None),
    ('HitStand', 'Combat.State.Hitstun', 'Combat.State.Tumble', 10, 'AS_React_HitStand', None, 'BLEND_OUT', 0, 1, 8, 'Combat.Event.Hit.Received'),
]

# ability, parent class, montage, travel cm, direction, keys (simulation frames, fraction): roll-travel.md converted from 60 to 62.5 Hz
GETUPS = [
    ('GA_GetupNeutral_Test', 'GSTN_GA_GetupNeutral', 'AM_React_GetUp', 0.0, None, []),
    ('GA_GetupRoll_Test', 'GSTN_GA_GetupRoll', 'AM_React_Roll', 80.0, 'FACING', [(0, 0.0), (6, 0.0), (10, 0.7), (16, 1.0)]),
    ('GA_GetupRollBack_Test', 'GSTN_GA_GetupRoll', 'AM_React_RollBack', 65.0, 'AGAINST_FACING',
     [(0, 0.0), (5, 0.0), (8, 0.17), (10, 0.68), (12, 0.97), (13, 1.0)]),
]

_lines = []


def log(msg):
    _lines.append(msg)
    unreal.log(f'[setup_reaction_anims] {msg}')


def _container(names):
    c = unreal.GameplayTagContainer()
    for n in names:
        unreal.BlueprintGameplayTagLibrary.add_gameplay_tag(c, dl._tag(n))
    return c


def _anim(name):
    return unreal.load_asset(f'{REACT}/{name}') if name else None


def reaction_set():
    entry = dl._data_asset(REACT, SET_NAME, unreal.GSTN_ReactionAnimSet)
    rules = []
    for name, req, blocked, prio, anim, then, end, bin_, chain, bout, restart in RULES:
        r = unreal.GSTN_ReactionAnimRule()
        r.set_editor_property('name', name)
        r.set_editor_property('required_tags', _container([req]))
        r.set_editor_property('blocked_tags', _container([blocked] if blocked else []))
        r.set_editor_property('priority', prio)
        r.set_editor_property('animation', _anim(anim))
        if then:
            r.set_editor_property('then_animation', _anim(then))
        r.set_editor_property('end_behaviour', getattr(unreal.GSTN_ReactionAnimEnd, end))
        r.set_editor_property('blend_in_frames', bin_)
        r.set_editor_property('chain_blend_frames', chain)
        r.set_editor_property('blend_out_frames', bout)
        if restart:
            r.set_editor_property('restart_event', dl._tag(restart))
        rules.append(r)
    entry.array('rules', rules)
    entry.save()
    log(f'{SET_NAME}: {len(rules)} rules')
    return entry.asset


def character_component(rset):
    import setup_traversal_presentation as tp
    bp = unreal.load_asset(SANDBOX_CHAR)
    comp, added = tp.add_component(bp, unreal.GSTN_ReactionAnimComponent, 'GSTN_ReactionAnim')
    if not comp:
        log('ERROR: could not add the reaction component')
        return
    if comp.get_editor_property('reaction_set') != rset:
        comp.set_editor_property('reaction_set', rset)
        added = True
    if added:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    log(f'SandboxCharacter_Mover reaction component: {"written" if added else "unchanged"}')


def getup(name, parent, montage_name, distance, direction, keys):
    cls = dl._ability(DEFENCE_TEST, name, parent)
    cdo = unreal.get_default_object(cls)
    montage = unreal.load_asset(f'{REACT}/{montage_name}')
    if not montage:
        log(f'ERROR: {montage_name} missing, {name} left alone')
        return cls
    before = cdo.get_editor_property('timeline').export_text()
    frames = max(1, math.ceil(montage.get_play_length() * SIM_HZ))
    cdo.set_editor_property('total_frames', frames)
    tl = cdo.get_editor_property('timeline')
    tl.set_editor_property('total_frames', frames)
    tl.set_editor_property('first_actionable_frame', frames)
    tl.set_editor_property('montage', montage)
    tl.set_editor_property('end_blend_out_frames', HOLD_BLEND_FRAMES)
    tl.set_editor_property('hold_pose_on_end', True)
    cdo.set_editor_property('timeline', tl)
    if direction:
        cdo.set_editor_property('travel_distance_cm', distance)
        cdo.set_editor_property('travel_direction', getattr(unreal.GSTN_GetupTravelDirection, direction))
        tk = []
        for f, frac in keys:
            k = unreal.GSTN_GetupTravelKey()
            k.set_editor_property('frame', f)
            k.set_editor_property('fraction', frac)
            tk.append(k)
        cdo.set_editor_property('travel_keys', tk)
    path = f'{DEFENCE_TEST}/{name}'
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    log(f'{name}: {frames} frames, {montage_name}, travel {distance:.0f} cm' + ('' if before != tl.export_text() else ' (timeline unchanged)'))
    return cls


def ground_bounce_plays_nothing():
    cls = unreal.EditorAssetLibrary.load_blueprint_class(f'{DEFENCE_TEST}/GA_GroundBounce_Test')
    if not cls:
        return
    cdo = unreal.get_default_object(cls)
    tl = cdo.get_editor_property('timeline')
    if tl.get_editor_property('montage'):
        tl.set_editor_property('montage', None)
        cdo.set_editor_property('timeline', tl)
        unreal.EditorAssetLibrary.save_asset(f'{DEFENCE_TEST}/GA_GroundBounce_Test', only_if_is_dirty=False)
        log('GA_GroundBounce_Test: montage cleared (the Knockdown rule plays the ground hit)')


def kits(rollback_cls):
    changed = []
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in reg.get_assets_by_path(KIT_ROOT, recursive=True):
        if not str(data.asset_name).startswith('KIT_'):
            continue
        kit = data.get_asset()
        if isinstance(kit, unreal.GSTN_PrimaryKitDefinition) and unreal.GSTN_DefenceContentLibrary.write_kit_getup_roll_back(kit, rollback_cls):
            unreal.EditorAssetLibrary.save_loaded_asset(kit, False)
            changed.append(str(data.asset_name))
    log(f'kits with a roll back: {", ".join(changed) if changed else "none changed"}')


def run():
    del _lines[:]
    rset = reaction_set()
    character_component(rset)
    classes = {g[0]: getup(*g) for g in GETUPS}
    ground_bounce_plays_nothing()
    kits(classes['GA_GetupRollBack_Test'])
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'Reaction')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_reaction_anims.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    return _lines[-5:]


if __name__ == '__main__':
    for line in run():
        print(line)
