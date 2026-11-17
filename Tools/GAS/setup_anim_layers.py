# Builds the override layer seam of the main anim graph WITH SLOTS (anim-layer-skid-wallslide-piano L1, polish M0) and the wall layer
# (setup_wall_anim_layer.py, reused). Idempotent: running it again changes nothing (every step is "find or add"; a Blueprint is backed up
# to Saved/Backups/GAS and saved only when a step changes it). Replaces step 3 of setup_wall_anim_layer.py (its run() now calls this one).
#
# Run headless (editor closed; needs the GSTNAnimationEditor module built):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_anim_layers.py" -unattended -nullrhi
# or from the editor Python console (close the Blueprints first):
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_anim_layers as s; s.run()
# --check only prints the main ABP's slots and the compiler messages, changing nothing. --variant: the heavy wall variant (as before).
#
# Steps:
#  1. One anim layer interface per override slot (SLOTS): ALI_GSTN_Wall (WallLayer), ALI_GSTN_Skid (SkidLayer), ALI_GSTN_RunStart
#     (RunStartLayer). Only the wall has a layer Blueprint today; skid (L3) and run start (M4) bring theirs later, with no change here.
#  2. The wall layer, its anim set, the collection and the character's component: setup_wall_anim_layer.py steps 1, 2, 4, 5.
#  3. /Game/Blueprints/SandboxCharacter_Mover_ABP (shared, backed up): reparented to GSTN_CharacterAnimInstance, implements every
#     interface (its own layer graphs stay empty: defaults the linked classes replace, never shown because the slot is 0 without a linked
#     layer that asks), and the seam in front of the Pose History collector:
#       Procedural_FullBody -> [0] Blend Poses by int (ActiveOverrideSlot, OverrideBlendTime, inertialization) -> Inertialization -> Pose History
#       Linked Anim Layer WallLayer     -> [1]
#       Linked Anim Layer SkidLayer     -> [2]
#       Linked Anim Layer RunStartLayer -> [3]
#     An older seam (Blend Poses by bool on bOverrideLayerActive) is replaced in place, its base pose and Inertialization kept.
# The slot numbers must match GSTNOverrideSlot (GSTN_OverrideLayerSource.h). The priority (wall > skid > run start) is C++, not graph.
import os, sys
import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_wall_anim_layer as wall  # noqa: E402

ANIM_DIR = wall.ANIM_DIR
MAIN_ABP = wall.MAIN_ABP
INSERT_BEFORE = wall.INSERT_BEFORE
SLOT_VARIABLE = 'ActiveOverrideSlot'
BLEND_TIME_VARIABLE = 'OverrideBlendTime'

# slot -> (interface asset, layer name). Must match GSTNOverrideSlot
SLOTS = {
    1: ('ALI_GSTN_Wall', 'WallLayer'),
    2: ('ALI_GSTN_Skid', 'SkidLayer'),
    3: ('ALI_GSTN_RunStart', 'RunStartLayer'),
}

lib = unreal.GSTN_AnimGraphAuthoringLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
errors = []


def log(msg):
    unreal.log(f'[setup_anim_layers] {msg}')


def fail(msg):
    errors.append(msg)
    unreal.log_error(f'[setup_anim_layers] {msg}')


def ensure_interface(name, layer):
    path = f'{ANIM_DIR}/{name}'
    ali = wall.load(path)
    changed = False
    if not ali:
        ali = asset_tools.create_asset(name, ANIM_DIR, unreal.AnimBlueprint, unreal.AnimLayerInterfaceFactory())
        if not ali:
            fail(f'could not create {path}')
            return None
        log(f'created {path}')
        changed = True
    if not lib.find_graph(ali, layer):
        changed |= lib.add_anim_layer(ali, layer)
        log(f'{name}: layer {layer} added')
    if changed:
        wall.compile_and_check(ali)
        wall.save(ali)
    return ali


def ensure_main_abp(interfaces):
    """interfaces: slot -> (ALI asset, layer name)."""
    abp = unreal.load_asset(MAIN_ABP)
    before_bytes = wall.snapshot(MAIN_ABP)
    before = lib.describe_graphs(abp)
    if '(parent GSTN_CharacterAnimInstance)' not in (before.splitlines()[0] if before else ''):
        unreal.BlueprintEditorLibrary.reparent_blueprint(abp, unreal.GSTN_CharacterAnimInstance)
        log(f'{MAIN_ABP}: reparented to GSTN_CharacterAnimInstance')
    wall.compile_and_check(abp)  # ActiveOverrideSlot must be on the skeleton class before the getter resolves
    specs = []
    for slot, (ali, layer) in sorted(interfaces.items()):
        lib.implement_layer_interface(abp, ali)
        spec = unreal.GSTN_OverrideSlotLayer()
        spec.set_editor_property('slot', slot)
        spec.set_editor_property('layer_interface', ali)
        spec.set_editor_property('layer_name', layer)
        specs.append(spec)
    wall.compile_and_check(abp)
    if not lib.insert_override_layer_slots(abp, specs, SLOT_VARIABLE, BLEND_TIME_VARIABLE, True, INSERT_BEFORE):
        fail(f'{MAIN_ABP}: the override slot seam was not built')
    if lib.describe_graphs(abp) == before:
        log(f'{MAIN_ABP}: seam already as wanted ({len(specs)} slots)')
        return abp
    if wall.compile_and_check(abp):
        wall.backup_snapshot(MAIN_ABP, before_bytes)
        wall.save(abp)
    else:
        fail(f'{MAIN_ABP} does not compile: NOT saved (reload it without saving)')
    return abp


def check():
    abp = unreal.load_asset(MAIN_ABP)
    wall.compile_and_check(abp)
    text = lib.describe_graphs(abp)
    for slot, (name, layer) in sorted(SLOTS.items()):
        log(f'check: slot {slot} {name}::{layer} implemented={f"implements {name}_C" in text} in graph={f"layer {name}_C::{layer}" in text}')


def run(variant=False):
    main_abp = unreal.load_asset(MAIN_ABP)
    skeleton = main_abp.get_editor_property('target_skeleton')
    interfaces = {}
    for slot, (name, layer) in SLOTS.items():
        ali = ensure_interface(name, layer)
        if not ali:
            return False
        interfaces[slot] = (ali, layer)

    wall_ali = interfaces[1][0]
    layer_bp = wall.ensure_layer_blueprint(wall_ali, skeleton)
    ensure_main_abp(interfaces)
    rules = [(wall.ensure_set(wall.SET_NAME, layer_bp), None, 0)]
    if variant:
        heavy = wall.ensure_heavy_variant(layer_bp, skeleton)
        if heavy:
            rules.append((wall.ensure_set(wall.HEAVY_SET_NAME, heavy), wall.tag_query_any(wall.HEAVY_TRAIT), 10))
    collection = wall.ensure_collection(rules)
    wall.ensure_component(collection)

    all_errors = errors + wall.errors
    if all_errors:
        log(f'FINISHED WITH {len(all_errors)} ERROR(S):')
        for e in all_errors:
            log(f'  {e}')
    else:
        log('finished, no errors')
    return not all_errors


if __name__ == '__main__':
    if '--check' in sys.argv:
        check()
    else:
        run(variant='--variant' in sys.argv)
