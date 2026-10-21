# Sets up the assets for the project's gameplay abilities (GAS-NPP). Idempotent: running it again changes nothing.
#
# Run from the editor Python console:
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_gas_assets as s; s.run()   # wall_transition=True for step 3, combat_input=True for step 4
# or headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_gas_assets.py [--wall-transition]"
#
# Steps:
#  1. /Game/GAS/Input/IMC_GAS_Abilities: lists IA_Jump for the ability system (keys copied from IMC_Sandbox). It must be
#     in Project Settings > Ability Simulation Settings > Ability System Mapping Contexts (Config/DefaultGame.ini).
#  2. BP_GASNPP_TestCharacter: GSTN_GA_Jump and GSTN_GA_WallSlide in the NpAbilitySystem DefaultAbilities, plus a
#     GSTN_AbilityInputComponent (binds the ability input actions for the local player).
#  3. (opt-in) SandboxCharacter_Mover: the Falling mode's GSTN_WallEntryTransition becomes a GSTN_AbilityWallEntryTransition with
#     the same settings. Pawns without an ability system behave as before (bAllowWithoutAbilitySystem).
#  4. (opt-in) BP_GASNPP_TestCharacter: the GSTN_CombatSimComponent reads the stick from IA_Move (MoveAction).
# The Blueprints are copied to Saved/Backups/GAS before being changed (Content is not under version control).
#
# Current layout (2026-10-17, set by hand from the editor, not by this script): the NpAbilitySystem (GA_Jump, GA_WallSlide)
# and the GSTN_AbilityInputComponent live on SandboxCharacter_Mover. BP_GASNPP_TestCharacter only overrides the inherited
# ability system (adds the test Dash/Buff, GSTN_TestAttributeSet, GSTN_GE_StaminaRegen) and keeps GSTN_GASNPPTestComponent.
# Step 2 finds those inherited components and leaves them as they are.
import os, shutil, re
import unreal

IMC_PATH = '/Game/GAS/Input/IMC_GAS_Abilities'
SANDBOX_IMC = '/Game/Input/IMC_Sandbox'
IA_JUMP = '/Game/Input/IA_Jump'
TEST_CHAR = '/Game/GASNPPTest/BP_GASNPP_TestCharacter'
SANDBOX_CHAR = '/Game/Blueprints/SandboxCharacter_Mover'
FALLING_MODE = 'Falling'
IA_MOVE = '/Game/Input/IA_Move'

subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log(f'[setup_gas_assets] {msg}')


def backup(asset_path):
    src = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + asset_path[len('/Game/'):] + '.uasset')
    dst_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backups', 'GAS')
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, os.path.basename(src))
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        log(f'backup {src} -> {dst}')


def editor_property_names(cls):
    """Editor property names of a class, from its generated docstring."""
    return re.findall(r'- ``(\w+)`` \(', cls.__doc__ or '')


def blueprint_components(bp):
    out = []
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
        data = sdl.get_data(handle)
        obj = sdl.get_object_for_blueprint(data, bp)
        if obj:
            out.append((handle, obj))
    return out


# ------------------------------------------------------------------------------------------------ 1. mapping context

def setup_mapping_context():
    imc = unreal.load_asset(IMC_PATH)
    if not imc:
        imc = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'IMC_GAS_Abilities', '/Game/GAS/Input', unreal.InputMappingContext, unreal.InputMappingContext_Factory())
        log(f'created {IMC_PATH}')

    jump = unreal.load_asset(IA_JUMP)
    if any(m.action == jump for m in imc.get_editor_property('default_key_mappings').mappings):
        return False

    sandbox = unreal.load_asset(SANDBOX_IMC)
    keys = [m.key for m in sandbox.get_editor_property('default_key_mappings').mappings if m.action == jump] if sandbox else []
    if not keys:
        space = unreal.Key()
        space.set_editor_property('key_name', 'SpaceBar')
        keys = [space]
    for key in keys:
        imc.map_key(jump, key)
    unreal.EditorAssetLibrary.save_loaded_asset(imc)
    log(f'IA_Jump mapped in {IMC_PATH} ({len(keys)} keys)')
    return True


# --------------------------------------------------------------------------------------------- 2. test character

def setup_test_character():
    bp = unreal.load_asset(TEST_CHAR)
    changed = False

    abilities = [unreal.GSTN_GA_Jump.static_class(), unreal.GSTN_GA_WallSlide.static_class()]
    asc = next((obj for _, obj in blueprint_components(bp) if isinstance(obj, unreal.NpAbilitySystemComponent)), None)
    if not asc:
        log(f'ERROR: no NpAbilitySystemComponent on {TEST_CHAR}')
        return False
    defaults = list(asc.get_editor_property('default_abilities'))
    missing = [a for a in abilities if a not in defaults]
    if missing:
        backup(TEST_CHAR)
        asc.set_editor_property('default_abilities', defaults + missing)
        log(f'DefaultAbilities += {[a.get_name() for a in missing]}')
        changed = True

    if not any(isinstance(obj, unreal.GSTN_AbilityInputComponent) for _, obj in blueprint_components(bp)):
        backup(TEST_CHAR)
        root = subobjects.k2_gather_subobject_data_for_blueprint(bp)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.GSTN_AbilityInputComponent,
                                              blueprint_context=bp)
        _, fail = subobjects.add_new_subobject(params)
        if not fail.is_empty():
            log(f'ERROR adding GSTN_AbilityInputComponent: {fail}')
        else:
            log('added GSTN_AbilityInputComponent')
            changed = True

    # The class default only finds the mapping context if it existed when the editor started
    imc = unreal.load_asset(IMC_PATH)
    for _, obj in blueprint_components(bp):
        if isinstance(obj, unreal.GSTN_AbilityInputComponent):
            contexts = list(obj.get_editor_property('ability_mapping_contexts'))
            if imc and imc not in contexts:
                backup(TEST_CHAR)
                obj.set_editor_property('ability_mapping_contexts', contexts + [imc])
                log('GSTN_AbilityInputComponent.AbilityMappingContexts += IMC_GAS_Abilities')
                changed = True

    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return changed


# ---------------------------------------------------------------------------------------------- 3. wall transition

def setup_wall_transition():
    bp = unreal.load_asset(SANDBOX_CHAR)
    mover = next((obj for _, obj in blueprint_components(bp) if isinstance(obj, unreal.MoverComponent)), None)
    if not mover:
        log(f'ERROR: no Mover component on {SANDBOX_CHAR}')
        return False

    modes = mover.get_editor_property('movement_modes')
    falling = modes.get(FALLING_MODE)
    if not falling:
        log(f'ERROR: no "{FALLING_MODE}" mode on {SANDBOX_CHAR}')
        return False

    transitions = list(falling.get_editor_property('transitions'))
    names = editor_property_names(unreal.GSTN_WallEntryTransition)
    changed = False
    for i, old in enumerate(transitions):
        if type(old) is not unreal.GSTN_WallEntryTransition:
            continue
        backup(SANDBOX_CHAR)
        new = unreal.new_object(unreal.GSTN_AbilityWallEntryTransition, outer=falling)
        for name in names:
            try:
                new.set_editor_property(name, old.get_editor_property(name))
            except Exception as e:
                log(f'  could not copy {name}: {e}')
        transitions[i] = new
        changed = True
        log(f'{FALLING_MODE}: transition {i} GSTN_WallEntryTransition -> GSTN_AbilityWallEntryTransition ({len(names)} settings copied)')

    if changed:
        falling.set_editor_property('transitions', transitions)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return changed


# ------------------------------------------------------------------------------------------------ 4. combat input

def setup_combat_input():
    """The combat component samples the stick of the local player from this action (the classifier needs the raw stick)."""
    bp = unreal.load_asset(TEST_CHAR)
    move = unreal.load_asset(IA_MOVE)
    combat = next((obj for _, obj in blueprint_components(bp) if isinstance(obj, unreal.GSTN_CombatSimComponent)), None)
    if not combat or not move:
        log(f'ERROR: GSTN_CombatSimComponent or {IA_MOVE} not found')
        return False
    if combat.get_editor_property('move_action') == move:
        return False
    backup(TEST_CHAR)
    combat.set_editor_property('move_action', move)
    log('GSTN_CombatSimComponent.MoveAction = IA_Move')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    return True


def run(wall_transition=False, combat_input=False):
    """Step 3 changes a movement asset owned by the movement work and step 4 the combat test character: opt-in."""
    log(f'mapping context changed: {setup_mapping_context()}')
    log(f'test character changed: {setup_test_character()}')
    if wall_transition:
        log(f'wall transition changed: {setup_wall_transition()}')
    else:
        log('wall transition skipped (run(wall_transition=True) or --wall-transition)')
    if combat_input:
        log(f'combat input changed: {setup_combat_input()}')


if __name__ == '__main__':
    import sys
    run(wall_transition='--wall-transition' in sys.argv, combat_input='--combat-input' in sys.argv)
