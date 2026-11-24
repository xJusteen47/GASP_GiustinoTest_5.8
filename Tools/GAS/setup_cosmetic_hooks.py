# CP4 of the Blueprint seams (cosmetic hooks): wires the presentation hooks into the game by data only. Idempotent. No manual Blueprint wiring.
# Run with the editor CLOSED (headless), after a build that contains UGSTN_MoveCosmeticsComponent, UGSTN_GCN_CombatHit and UGSTN_CosmeticAuthoring:
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_cosmetic_hooks.py" -unattended -nullrhi
# Run setup_designer_examples.py first (it makes the example moves). Prints at most 5 lines; full log in Saved/Examples/setup_cosmetic_hooks.log.
#
# What it does:
#  1. UGSTN_MoveCosmeticsComponent "GSTN_MoveCosmetics" on /Game/Blueprints/SandboxCharacter_Mover (BP_GASNPP_TestCharacter inherits it).
#  2. Four hit cues in /Game/GSTN/_Examples/Cosmetics, Blueprint children of GSTN_GCN_CombatHit (a GCN Burst placed at the hit, played once):
#     GCN_Example_Hit_Light / _Medium / _Heavy / _Finisher for GameplayCue.Hit.Light/Medium/Heavy/Finisher (owner rule: size follows importance,
#     four steps). Their Burst Effects stay EMPTY unless SPARK_SYSTEM below names a Niagara system: the project has no hit VFX yet (the log lists
#     the Niagara systems it found, for the VFX kit thread). A designer fills Burst Effects (spark, sound, shake for heavy only, rumble) in the asset.
#  3. The example moves (setup_designer_examples.py) get a cue and cosmetic windows, only when they have none yet (force=True rewrites):
#       GA_Example_Light   cue Light,  trail on the active frames + 2 tail on hand_r, whoosh 2 frames before the active frames
#       GA_Example_Strong  cue Heavy,  trail + 3 tail, whoosh
#       GA_Example_Special cue Medium, trail + 2 tail, whoosh
#     The trail's Effect is TRAIL_SYSTEM (empty = the window still runs and raises the Blueprint events, nothing is drawn).
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

SANDBOX_CHAR = '/Game/Blueprints/SandboxCharacter_Mover'
EXAMPLES = '/Game/GSTN/_Examples'
COSMETICS = f'{EXAMPLES}/Cosmetics'
# Niagara systems to use (asset paths); '' = none. Fill them once the VFX kit exists, then rerun with force=True
SPARK_SYSTEM = ''
TRAIL_SYSTEM = ''
TRAIL_SOCKET = 'hand_r'
CUES = ['Light', 'Medium', 'Heavy', 'Finisher']
# move -> (cue size, trail tail frames)
MOVES = {
    'GA_Example_Light': ('Light', 2),
    'GA_Example_Strong': ('Heavy', 3),
    'GA_Example_Special': ('Medium', 2),
}
WHOOSH_LEAD = 2

_lines = []


def log(msg):
    _lines.append(msg)
    unreal.log(f'[setup_cosmetic_hooks] {msg}')


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def character_component():
    import setup_traversal_presentation as tp
    bp = unreal.load_asset(SANDBOX_CHAR)
    if not bp:
        log(f'ERROR: no {SANDBOX_CHAR}')
        return
    comp, added = tp.add_component(bp, unreal.GSTN_MoveCosmeticsComponent, 'GSTN_MoveCosmetics')
    if not comp:
        log('ERROR: could not add the move cosmetics component')
        return
    if added:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    log(f'SandboxCharacter_Mover move cosmetics component: {"added" if added else "already there"}')


def _niagara(path):
    if not path:
        return None
    system = unreal.load_asset(path)
    if not system:
        log(f'WARNING: no Niagara system {path}')
    return system


def list_niagara_candidates():
    """The Niagara systems under /Game whose names look like hit sparks or trails: for the VFX kit thread, nothing is used automatically"""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for data in registry.get_assets_by_path('/Game', recursive=True):
        if str(data.asset_class_path.asset_name) != 'NiagaraSystem':
            continue
        name = str(data.asset_name)
        if any(k in name.lower() for k in ('spark', 'impact', 'hit', 'trail', 'ribbon', 'slash', 'swipe', 'dust')):
            found.append(f'{data.package_name}')
    log(f'Niagara candidates ({len(found)}): ' + (', '.join(found[:20]) if found else 'none'))


def hit_cue(size, force):
    name = f'GCN_Example_Hit_{size}'
    path = f'{COSMETICS}/{name}'
    bp = unreal.load_asset(path)
    created = False
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.GSTN_GCN_CombatHit.static_class())
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, COSMETICS, unreal.Blueprint, factory)
        if not bp:
            log(f'ERROR: could not create {name}')
            return None
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        created = True
    cdo = unreal.get_default_object(bp.generated_class())
    tag = _tag(f'GameplayCue.Hit.{size}')
    changed = created
    if cdo.get_editor_property('gameplay_cue_tag') != tag:
        cdo.set_editor_property('gameplay_cue_tag', tag)
        changed = True
    spark = _niagara(SPARK_SYSTEM)
    if spark and (created or force):
        effects = cdo.get_editor_property('burst_effects')
        particle = unreal.GameplayCueNotify_ParticleInfo()
        particle.set_editor_property('niagara_system', spark)
        effects.set_editor_property('burst_particles', [particle])
        cdo.set_editor_property('burst_effects', effects)
        changed = True
    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    log(f'{name}: {"written" if changed else "unchanged"}')
    return bp


def _window(tag, start, end, tail, socket, system):
    window = unreal.GSTN_MoveCosmeticWindow()
    window.set_editor_property('cosmetic_tag', _tag(tag))
    window.set_editor_property('start_frame', max(start, 0))
    window.set_editor_property('end_frame', end)
    window.set_editor_property('tail_frames', tail)
    window.set_editor_property('socket', socket)
    if system:
        window.set_editor_property('effect', system)
    return window


def example_move(name, size, tail, force):
    path = f'{EXAMPLES}/{name}'
    bp = unreal.load_asset(path)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path) if bp else None
    if not cls:
        log(f'WARNING: no {name} (run setup_designer_examples.py first)')
        return
    cdo = unreal.get_default_object(cls)
    changed = False
    cue = unreal.GSTN_CosmeticAuthoring.set_hit_cue_tag(cls, _tag(f'GameplayCue.Hit.{size}'))
    if cue < 0:
        log(f'WARNING: {name} uses a timeline asset: set its hitboxes\' cue there')
    changed |= cue > 0
    if force or len(cdo.get_editor_property('cosmetic_windows')) == 0:
        # 5.9 Python drops the bool return when out params exist: (first, last), or an error without hitbox
        try:
            first, last = unreal.GSTN_CosmeticAuthoring.get_active_frames(cls)
            ok = True
        except Exception:
            ok = False
        if not ok:
            log(f'WARNING: {name} has no hitbox: no trail window')
        else:
            trail = _niagara(TRAIL_SYSTEM)
            windows = [_window('Cosmetic.Trail', first, last, tail, TRAIL_SOCKET, trail),
                       _window('Cosmetic.Whoosh', first - WHOOSH_LEAD, first - WHOOSH_LEAD, 0, TRAIL_SOCKET, None)]
            cdo.set_editor_property('cosmetic_windows', windows)
            changed = True
            log(f'{name}: trail {first}-{last}+{tail}, whoosh {max(first - WHOOSH_LEAD, 0)}, cue {size}')
    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    else:
        log(f'{name}: unchanged')


def run(force=False):
    del _lines[:]
    if not unreal.EditorAssetLibrary.does_directory_exist(COSMETICS):
        unreal.EditorAssetLibrary.make_directory(COSMETICS)
    character_component()
    list_niagara_candidates()
    cues = [hit_cue(size, force) for size in CUES]
    for name, (size, tail) in MOVES.items():
        example_move(name, size, tail, force)
    log(f'done: {sum(1 for c in cues if c)} cues')
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'Examples')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_cosmetic_hooks.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    return _lines[-5:]


if __name__ == '__main__':
    for line in run():
        print(line)
