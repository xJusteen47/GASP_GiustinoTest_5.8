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
#  2. (opt-in, --test-character; edits a shared Blueprint) BP_GASNPP_TestCharacter: GSTN_GA_Jump and GSTN_GA_WallSlide in the NpAbilitySystem DefaultAbilities, plus a
#     GSTN_AbilityInputComponent (binds the ability input actions for the local player).
#  3. (opt-in) SandboxCharacter_Mover: the Falling mode's GSTN_WallEntryTransition becomes a GSTN_AbilityWallEntryTransition with
#     the same settings. Pawns without an ability system behave as before (bAllowWithoutAbilitySystem).
#  4. (opt-in, --combat-input) Combat M2 test content in /Game/GASNPPTest/Input (specs in combat_test_specs()):
#     IA_GASNPP_Attack/Special (+ keys in IMC_GASNPP_Test), IC_GASNPP_Test (UGSTN_InputConfig), SS_GASNPP_Standard (slot
#     set, filled by UGSTN_StandardSlots.FillSlotSet), BS_GASNPP_Test (UGSTN_CommandBindingSet: Dash/Buff, Attack.* -> test
#     attack, Special.* -> test stance, the C14 movement kit). It does NOT edit BP_GASNPP_TestCharacter: it warns when the
#     Blueprint's GSTN_CombatSimComponent does not point to these assets, and the harness grants the abilities at run time.
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

# Test content of combat M2 (combat-m2-sim-input.md section 8): an input config, a command binding set and the slot set
# they use, assigned to the GSTN_CombatSimComponent of BP_GASNPP_TestCharacter. The asset specs are in
# combat_test_specs() only; the setup_combat_* functions apply them, skip (and log) whatever cannot be built yet and
# change nothing when the assets already match.

TEST_INPUT_DIR = '/Game/GASNPPTest/Input'
TEST_IMC = TEST_INPUT_DIR + '/IMC_GASNPP_Test'
SHAPES = ('Neutral', 'Directional', 'CrouchNeutral', 'CrouchDirectional')
CONTEXTS = ('Input.Context.Grounded', 'Input.Context.Airborne')


def combat_test_specs():
    """
    Everything the combat test content needs, as plain data. Edit this to add buttons/bindings in the second step.

    Tags and classes are names resolved when applying: an entry whose gameplay tag is not registered, whose input action
    does not exist or whose ability class does not exist is skipped with a log line, so the script can run before the
    missing pieces exist and be run again afterwards.
    """
    return {
        # New Enhanced Input actions (bool) with the keys mapped in TEST_IMC. Only created when missing.
        # The keys are free picks for the test map: change them if they clash with IMC_Sandbox.
        'new_actions': [
            ('IA_GASNPP_Attack', ['LeftMouseButton']),
            ('IA_GASNPP_Special', ['MiddleMouseButton']),
            # C13: the jump button of the platform jump kit (the scenarios drive it with scripted buttons; the key is for manual tries)
            ('IA_GASNPP_Jump', ['Y']),
            # C14: dodge, interact and sprint (dash) buttons of the movement kit
            ('IA_GASNPP_Dodge', ['U']),
            ('IA_GASNPP_Interact', ['O']),
            ('IA_GASNPP_Sprint', ['L']),
        ],
        # Bit order of the buttons = order here (it is the bit in every input mask: keep it stable)
        'input_config': {
            'asset': ('IC_GASNPP_Test', TEST_INPUT_DIR),
            'buttons': [
                ('Input.Button.Attack', TEST_INPUT_DIR + '/IA_GASNPP_Attack'),
                ('Input.Button.Special', TEST_INPUT_DIR + '/IA_GASNPP_Special'),
                ('Input.Button.Crouch', '/Game/Input/IA_Crouch'),
                # Dash/Buff buttons: tags in Config/Tags/GSTNCommands.ini. A button whose tag or action is missing is skipped
                ('Input.Button.Dash', TEST_INPUT_DIR + '/IA_GASNPP_Dash'),
                ('Input.Button.Buff', TEST_INPUT_DIR + '/IA_GASNPP_Buff'),
                # C13: bit 5, after Buff, so the indices of the buttons above stay as they were
                ('Input.Button.Jump', TEST_INPUT_DIR + '/IA_GASNPP_Jump'),
                # C14: bits 6, 7 and 8, after Jump, for the same reason
                ('Input.Button.Dodge', TEST_INPUT_DIR + '/IA_GASNPP_Dodge'),
                ('Input.Button.Interact', TEST_INPUT_DIR + '/IA_GASNPP_Interact'),
                ('Input.Button.Sprint', TEST_INPUT_DIR + '/IA_GASNPP_Sprint'),
            ],
            'move_action': IA_MOVE,
        },
        # The standard command slots (combat.md 9.1) as a shared slot set asset, filled by UGSTN_StandardSlots.FillSlotSet
        'slot_set': ('SS_GASNPP_Standard', TEST_INPUT_DIR),
        # Plain button-press slots added to that set (command, button): the harness' own commands. Not added twice.
        'press_slots': [('Command.Dash', 'Input.Button.Dash'), ('Command.Buff', 'Input.Button.Buff'),
                        # C13: the default proposal of the jump kit: Press(Jump), and a down press (Crouch) for the fast fall
                        ('Command.Movement.Jump', 'Input.Button.Jump'), ('Command.Movement.FastFall', 'Input.Button.Crouch')],
        # C14: the default movement slots of the plugin (UGSTN_MovementSlots): dash, dodge, interact tap = grab and hold = pick up.
        # The swapped data set (tap = pick up, hold = grab) is a Core test; the PIE scenario runs the default one
        'movement_slots': {'swap_tap_hold': False},
        # Test abilities (project module, Source/GASP_Giustino2/GASNPPTest) the pawn's ability system must be granted for a
        # binding to start them. Added to the NpAbilitySystemComponent's DefaultAbilities when missing.
        'abilities': ['GSTN_GA_TestDash', 'GSTN_GA_TestBuff', 'GSTN_GA_TestAttack', 'GSTN_GA_TestStance'],
        'binding_set': {
            'asset': ('BS_GASNPP_Test', TEST_INPUT_DIR),
            # (command, require tags, ignore tags, ability class name in `unreal`; None = skipped, logged)
            'bindings': [
                # Command.Dash / Command.Buff get their slot from 'press_slots' above (a binding without a slot never starts)
                ('Command.Dash', [], [], 'GSTN_GA_TestDash'),
                ('Command.Buff', [], [], 'GSTN_GA_TestBuff'),
                # C13 jump kit (plugin abilities; the harness grants them with ServerSetJumpKit, they are not in the Blueprint's DefaultAbilities)
                ('Command.Movement.Jump', [], [], 'GSTN_GA_PlatformJump'),
                ('Command.Movement.FastFall', [], [], 'GSTN_GA_PlatformFastFall'),
                # C14 movement kit (plugin abilities, granted by the harness with ServerSetMoveKit). The dodge is one command bound by context;
                # the escape dodge (a kit with the blocks dropped as data) takes over when the pawn owns Test.State.Escape
                ('Command.Movement.Dash', [], [], 'GSTN_GA_Dash'),
                ('Command.Defence.Dodge', ['Input.Context.Grounded'], ['Test.State.Escape'], 'GSTN_GA_GroundDodge'),
                ('Command.Defence.Dodge', ['Input.Context.Grounded', 'Test.State.Escape'], [], 'GSTN_GA_TestEscapeDodge'),
                ('Command.Defence.Dodge', ['Input.Context.Airborne'], [], 'GSTN_GA_AirDodge'),
                ('Command.Interact.Grab', [], [], 'GSTN_GA_TestGrab'),
                ('Command.Interact.PickUp', [], [], 'GSTN_GA_TestPickUp'),
                # The four standard attack commands, grounded / airborne: one test attack (owns Test.State.Attacking)
                *[(f'Command.Attack.{shape}', [ctx], [], 'GSTN_GA_TestAttack')
                  for shape in SHAPES for ctx in CONTEXTS],
                # The Special button is the stance button: held, it owns Input.Lock.Command.Attack (scenario_combat_lock)
                *[(f'Command.Special.{shape}', [ctx], [], 'GSTN_GA_TestStance')
                  for shape in SHAPES for ctx in CONTEXTS],
            ],
        },
    }


def _make_tag(name):
    tag = unreal.GameplayTag()
    # TagName is read-only from Python: go through the text form.
    tag.import_text(f'(TagName="{name}")')
    return tag


def _tag_exists(name):
    try:
        return bool(unreal.GameplayTagLibrary.is_gameplay_tag_valid(_make_tag(name)))
    except Exception:
        return True  # cannot check from here: the asset validation will report it


def _tag_container(names):
    return unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([_make_tag(n) for n in names])


def _get_or_create(name, folder, asset_class, factory):
    asset = unreal.load_asset(f'{folder}/{name}')
    if asset:
        return asset, False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, factory)
    log(f'created {folder}/{name}')
    return asset, True


def setup_combat_test_actions(specs):
    """Creates the attack/special actions and maps their keys in the test mapping context."""
    changed = False
    imc = unreal.load_asset(TEST_IMC)
    for name, keys in specs['new_actions']:
        action, created = _get_or_create(name, TEST_INPUT_DIR, unreal.InputAction, unreal.InputAction_Factory())
        if created:
            action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
            unreal.EditorAssetLibrary.save_loaded_asset(action)
            changed = True
        if not imc:
            log(f'WARNING: {TEST_IMC} not found, {name} is not mapped to any key')
            continue
        if any(m.action == action for m in imc.get_editor_property('default_key_mappings').mappings):
            continue
        backup(TEST_IMC)
        for key_name in keys:
            key = unreal.Key()
            key.set_editor_property('key_name', key_name)
            imc.map_key(action, key)
        unreal.EditorAssetLibrary.save_loaded_asset(imc)
        log(f'{name} mapped to {keys} in IMC_GASNPP_Test')
        changed = True
    return changed


def setup_combat_input_config(specs):
    """The test UGSTN_InputConfig: buttons in spec order plus the move action. Returns (asset, changed)."""
    cfg = specs['input_config']
    asset, created = _get_or_create(*cfg['asset'], unreal.GSTN_InputConfig, unreal.DataAssetFactory())
    wanted = []
    for tag_name, action_path in cfg['buttons']:
        action = unreal.load_asset(action_path)
        if not _tag_exists(tag_name) or not action:
            log(f'  button {tag_name} skipped ({"tag not registered" if action else action_path + " not found"})')
            continue
        wanted.append((tag_name, action))
    move = unreal.load_asset(cfg['move_action'])

    current = [(str(b.get_editor_property('tag').get_editor_property('tag_name')), b.get_editor_property('action'))
               for b in asset.get_editor_property('buttons')]
    changed = created
    if current != wanted:
        buttons = []
        for tag_name, action in wanted:
            d = unreal.GSTN_InputButtonDef()
            d.set_editor_property('tag', _make_tag(tag_name))
            d.set_editor_property('action', action)
            buttons.append(d)
        asset.set_editor_property('buttons', buttons)
        changed = True
    if asset.get_editor_property('move_action') != move:
        asset.set_editor_property('move_action', move)
        changed = True
    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        log(f'{cfg["asset"][0]}: {[t for t, _ in wanted]} + MoveAction')
    return asset, changed


def setup_combat_slot_set(specs):
    """The standard slots (UGSTN_StandardSlots.FillSlotSet) plus the harness' press slots (UGSTN_TestSlotUtils.AddButtonPressSlot).
    Returns (asset, changed)."""
    name, folder = specs['slot_set']
    asset, created = _get_or_create(name, folder, unreal.GSTN_InputSlotSet, unreal.DataAssetFactory())
    changed = created
    # Rebuilt from scratch on every run (clear, fill, add), so stale entries (e.g. press slots made by an older build) are replaced
    # and reruns never duplicate; the asset is only saved when the rebuilt slots differ from the old ones.
    before = str(asset.get_editor_property('slots'))
    fill = getattr(getattr(unreal, 'GSTN_StandardSlots', None), 'fill_slot_set', None)
    if not fill:
        log('  standard slots NOT filled: unreal.GSTN_StandardSlots.fill_slot_set not found (editor built before FillSlotSet became a '
            'UFUNCTION?). Rebuild the editor and run again')
    else:
        asset.set_editor_property('slots', [])
        fill(asset)
        log(f'{name}: standard slots rebuilt')

    add = getattr(getattr(unreal, 'GSTN_TestSlotUtils', None), 'add_button_press_slot', None)
    for command, button in specs['press_slots']:
        if not (_tag_exists(command) and _tag_exists(button)):
            log(f'  press slot {command} / {button} skipped (tag not registered)')
        elif not add:
            log('  press slots NOT added: unreal.GSTN_TestSlotUtils not found (rebuild the editor and run again)')
            break
        elif add(asset, _make_tag(command), _make_tag(button)):
            log(f'{name}: press slot {command} <- {button}')
    movement = getattr(getattr(unreal, 'GSTN_MovementSlots', None), 'append_to_slot_set', None)
    if movement:
        movement(asset, specs['movement_slots']['swap_tap_hold'])
        log(f'{name}: movement slots (dash, dodge, interact tap/hold) added')
    else:
        log('  movement slots NOT added: unreal.GSTN_MovementSlots not found (rebuild the editor and run again)')
    if str(asset.get_editor_property('slots')) != before:
        changed = True
    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return asset, changed


def setup_combat_binding_set(specs, slot_set):
    """The test UGSTN_CommandBindingSet. Bindings whose tags or ability are not available yet are skipped and logged."""
    cfg = specs['binding_set']
    asset, created = _get_or_create(*cfg['asset'], unreal.GSTN_CommandBindingSet, unreal.DataAssetFactory())
    changed = created
    if list(asset.get_editor_property('slot_sets')) != [slot_set]:
        asset.set_editor_property('slot_sets', [slot_set])
        changed = True

    bindings = []
    for command, require, ignore, ability_name in cfg['bindings']:
        cls = getattr(unreal, ability_name, None) if ability_name else None
        missing_tags = [t for t in [command] + require + ignore if not _tag_exists(t)]
        if cls is None or missing_tags:
            log(f'  binding {command} {require} skipped ({f"ability class {ability_name} not in this editor build" if cls is None else "tags not registered: " + str(missing_tags)})')
            continue
        req = unreal.GSTN_TagRequirements()
        req.set_editor_property('require_tags', _tag_container(require))
        req.set_editor_property('ignore_tags', _tag_container(ignore))
        b = unreal.GSTN_CommandBinding()
        b.set_editor_property('command', _make_tag(command))
        b.set_editor_property('requirements', req)
        b.set_editor_property('ability', cls.static_class())
        bindings.append(b)

    def key(b):
        r = b.get_editor_property('requirements')
        return (str(b.get_editor_property('command').get_editor_property('tag_name')), b.get_editor_property('ability'),
                len(r.get_editor_property('require_tags').get_editor_property('gameplay_tags')))
    if [key(b) for b in asset.get_editor_property('bindings')] != [key(b) for b in bindings]:
        asset.set_editor_property('bindings', bindings)
        changed = True
    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        log(f'{cfg["asset"][0]}: {len(bindings)} bindings')
    return asset, changed


def check_test_character_abilities(specs):
    """Reports (never changes) the test abilities that BP_GASNPP_TestCharacter's ability system does not list in DefaultAbilities.

    The Blueprint is shared: this script does not edit it. The harness grants the abilities at run time instead
    (UGSTN_GASNPPTestComponent.ServerSetJumpKit / ServerSetMoveKit), so a missing entry is only informative."""
    bp = unreal.load_asset(TEST_CHAR)
    asc = next((obj for _, obj in blueprint_components(bp) if isinstance(obj, unreal.NpAbilitySystemComponent)), None)
    if not asc:
        log(f'ERROR: no NpAbilitySystemComponent on {TEST_CHAR}')
        return
    defaults = list(asc.get_editor_property('default_abilities'))
    for name in specs['abilities']:
        cls = getattr(unreal, name, None)
        if cls is not None and cls.static_class() not in defaults:
            log(f'  note: {name} is not in the DefaultAbilities of {TEST_CHAR} (not edited: the harness grants abilities at run time)')


def setup_combat_input():
    """Creates/updates the combat test content from combat_test_specs(). Never edits or saves BP_GASNPP_TestCharacter:
    it only reports when the Blueprint's GSTN_CombatSimComponent does not point to the assets made here."""
    specs = combat_test_specs()
    changed = setup_combat_test_actions(specs)
    check_test_character_abilities(specs)
    config, c = setup_combat_input_config(specs)
    changed |= c
    slot_set, c = setup_combat_slot_set(specs)
    changed |= c
    binding_set, c = setup_combat_binding_set(specs, slot_set)
    changed |= c

    bp = unreal.load_asset(TEST_CHAR)
    combat = next((obj for _, obj in blueprint_components(bp) if isinstance(obj, unreal.GSTN_CombatSimComponent)), None)
    if not combat:
        log(f'ERROR: no GSTN_CombatSimComponent on {TEST_CHAR}')
        return changed
    # Soft pointers: compare asset paths, not objects (an object compare is never equal and made the old script save the Blueprint every run)
    def path_of(value):
        return str(value.get_path_name()) if value else ''
    assigned = {path_of(combat.get_editor_property('input_config')), path_of(combat.get_editor_property('slot_bindings'))}
    wanted = {path_of(config), path_of(binding_set)}
    if assigned != wanted:
        log(f'  WARNING: the GSTN_CombatSimComponent of {TEST_CHAR} points to {sorted(assigned)}, not to {sorted(wanted)}. '
            'This script does not edit the shared Blueprint: assign InputConfig / SlotBindings by hand if that is wanted')
    return changed


def run(wall_transition=False, combat_input=False, test_character=False):
    """Step 2 and step 3 change shared Blueprints (the test character, the sandbox character) and are opt-in; step 4 never does."""
    log(f'mapping context changed: {setup_mapping_context()}')
    if test_character:
        log(f'test character changed: {setup_test_character()}')
    else:
        log('test character skipped (run(test_character=True) or --test-character: it edits BP_GASNPP_TestCharacter)')
    if wall_transition:
        log(f'wall transition changed: {setup_wall_transition()}')
    else:
        log('wall transition skipped (run(wall_transition=True) or --wall-transition)')
    if combat_input:
        log(f'combat input changed: {setup_combat_input()}')


if __name__ == '__main__':
    import sys
    run(wall_transition='--wall-transition' in sys.argv, combat_input='--combat-input' in sys.argv,
        test_character='--test-character' in sys.argv)
