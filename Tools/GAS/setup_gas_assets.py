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
#  5. (opt-in, --combat-content) C15 loadout test content in /Game/GASNPPTest/Combat (specs in combat_content_specs()): the kits
#     KIT_Test_Alpha / KIT_Test_Beta and the weapons WPN_Test_Blade / Spear / Support. New assets only. Needs the tags of
#     Config/Tags/GSTNCombatTest.ini and step 4's slot set.
#  6. (opt-in, --hits-content) M4 H8 hit test content in /Game/GASNPPTest/Combat (specs in hits_content_specs()): the hurtbox set
#     HBS_Test_Pawn (torso / head / feet capsules, tag leaves of Config/Tags/GSTNCombat.ini), the weapon WPN_Test_Hits and the kit KIT_Test_Hits
#     (Command.Attack.Neutral -> GSTN_GA_TestHitMove, whose frame data is in C++: a hitbox of each space), and BP_GASNPP_TestCharacter's
#     combat component HurtboxSet -> HBS_Test_Pawn (the one shared Blueprint this step edits; backed up first).
#  7. (opt-in, --h13-content) M4 H13a content (specs in h13_specs()): /GSTNCombat/Rules/GSTN_CombatRules_Standard (plugin content, the rules the match plays
#     by default), the weight hurtbox sets HBS_Test_Light / Medium / Heavy, the move variant timelines TL_Test_HitRoot / Baked / World / Ally, the extension
#     example Blueprints BP_*_Example and the physical materials, in /Game/GASNPPTest/Combat. Edits no shared Blueprint. The map L_Test_Combat is built by
#     build_test_combat_map.py (needs the editor with a world); hurtbox sets from a Physics Asset by make_hurtbox_set.py; baked sockets by bake_sockets.py.
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
                ('Command.Defence.Dodge', ['Input.Context.Grounded'], ['Test.State.Escape', 'Test.State.Planted'], 'GSTN_GA_GroundDodge'),
                ('Command.Defence.Dodge', ['Input.Context.Grounded', 'Test.State.Escape'], [], 'GSTN_GA_TestEscapeDodge'),
                # M4 H12: the ground dodge without its take-off, for the dodge scenarios (a hitbox fixed in the world meets the same feet at every frame)
                ('Command.Defence.Dodge', ['Input.Context.Grounded', 'Test.State.Planted'], [], 'GSTN_GA_TestPlantedGroundDodge'),
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


TEST_COMBAT_DIR = '/Game/GASNPPTest/Combat'


def combat_content_specs():
    """C15 test kits and weapons, as plain data. Names are asset names (they are the primary asset ids), tags are registered in
    Config/Tags/GSTNCombatTest.ini, classes are names in `unreal`. The support weapon ships without a stats effect and without bindings:
    it inherits the kit's stats (owner direction 2026-10-23)."""
    return {
        'weapons': [
            # (asset, identity tag, role tag, [(command, require, ignore, ability)], weapon stats effect class or None)
            ('WPN_Test_Blade', 'Weapon.Test.Blade', 'Test.Role.Primary',
             [('Command.Attack.Neutral', ['Input.Context.Grounded'], [], 'GSTN_GA_TestAttack')], None),
            ('WPN_Test_Spear', 'Weapon.Test.Spear', 'Test.Role.Primary',
             [('Command.Attack.Neutral', ['Input.Context.Grounded'], [], 'GSTN_GA_TestBuff')], 'GSTN_GE_TestPrimaryWeaponStats'),
            ('WPN_Test_Support', 'Weapon.Test.Support', 'Test.Role.Support', [], None),
        ],
        'kits': [
            # (asset, identity tag, primary weapon, {stat tag: value}, [(command, require, ignore, ability)])
            ('KIT_Test_Alpha', 'Kit.Test.Alpha', 'WPN_Test_Blade',
             {'Combat.SetByCaller.Stat.WalkSpeed': 600.0, 'Combat.SetByCaller.Stat.JumpVelocity': 700.0},
             [('Command.Movement.Jump', [], [], 'GSTN_GA_PlatformJump')]),
            ('KIT_Test_Beta', 'Kit.Test.Beta', 'WPN_Test_Spear',
             {'Combat.SetByCaller.Stat.WalkSpeed': 600.0, 'Combat.SetByCaller.Stat.JumpVelocity': 900.0},
             [('Command.Movement.Jump', [], [], 'GSTN_GA_PlatformJump'), ('Command.Movement.Dash', [], [], 'GSTN_GA_Dash')]),
        ],
        'allowed_support_role': 'Test.Role.Support',
        'base_slot_set': TEST_INPUT_DIR + '/SS_GASNPP_Standard',
        'init_effect': 'GSTN_GE_TestKitInit',
    }


def _any_tag_query(tag_name):
    """A FGameplayTagQuery matching any of the one tag, built by the engine (the token stream is not hand-written)."""
    return unreal.GameplayTagLibrary.make_gameplay_tag_query_match_any_tags(_tag_container([tag_name]))


def _binding(command, require, ignore, ability_name):
    """A FGSTN_AbilityBinding, or None when the ability class does not exist (logged)."""
    cls = getattr(unreal, ability_name, None)
    if cls is None or not _tag_exists(command):
        log(f'  binding {command} -> {ability_name} skipped (class or tag missing)')
        return None
    binding = unreal.GSTN_AbilityBinding()
    binding.set_editor_property('command', _make_tag(command))
    requirements = unreal.GSTN_TagRequirements()
    requirements.set_editor_property('require_tags', _tag_container(require))
    requirements.set_editor_property('ignore_tags', _tag_container(ignore))
    binding.set_editor_property('requirements', requirements)
    binding.set_editor_property('ability', cls)
    return binding


def setup_combat_content():
    """Creates/updates /Game/GASNPPTest/Combat from combat_content_specs(). New assets only: no Blueprint is edited. Every property is
    rewritten from the specs on each run (so a rerun converges to the specs) and the assets are saved. Returns True when something was saved."""
    specs = combat_content_specs()
    changed = False

    for name, identity, role, bindings, stats_effect in specs['weapons']:
        asset, created = _get_or_create(name, TEST_COMBAT_DIR, unreal.GSTN_WeaponDefinition, unreal.DataAssetFactory())
        asset.set_editor_property('display_name', name)
        asset.set_editor_property('weapon_tags', _tag_container([identity, role]))
        asset.set_editor_property('bindings', [b for b in (_binding(*row) for row in bindings) if b])
        effect = getattr(unreal, stats_effect, None) if stats_effect else None
        asset.set_editor_property('stats_effect', effect)
        asset.set_editor_property('stat_modifiers', [])   # the test effect has constant modifiers, it reads no set-by-caller number
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        changed = True

    for name, identity, weapon, stat_values, bindings in specs['kits']:
        asset, created = _get_or_create(name, TEST_COMBAT_DIR, unreal.GSTN_PrimaryKitDefinition, unreal.DataAssetFactory())
        asset.set_editor_property('kit_tags', _tag_container([identity]))
        asset.set_editor_property('primary_weapon', unreal.load_asset(f'{TEST_COMBAT_DIR}/{weapon}'))
        asset.set_editor_property('allowed_additional_weapons', _any_tag_query(specs['allowed_support_role']))
        asset.set_editor_property('max_additional_weapons', 1)
        values = []
        for tag, value in stat_values.items():
            stat = unreal.GSTN_StatValue()
            stat.set_editor_property('stat', _make_tag(tag))
            stat.set_editor_property('value', value)
            values.append(stat)
        stats = unreal.GSTN_KitStats()
        stats.set_editor_property('values', values)
        asset.set_editor_property('stats', stats)
        asset.set_editor_property('init_stats_effect', getattr(unreal, specs['init_effect'], None))
        command_set = unreal.GSTN_CommandSet()
        command_set.set_editor_property('base_set', unreal.load_asset(specs['base_slot_set']))
        asset.set_editor_property('command_set', command_set)
        asset.set_editor_property('kit_bindings', [b for b in (_binding(*row) for row in bindings) if b])
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        changed = True
    log(f'combat content written under {TEST_COMBAT_DIR}')
    return changed



def c18_content_specs():
    """C18 (loadout table swap, session 2) test content, plain data. Kits reuse the Alpha weapon and stats (WalkSpeed 600, JumpVelocity 700).
    (asset, identity, command set {removed, added: [(command, buttons, chord, fall_through, priority_offset)]}, bindings, super slot count, [super asset])"""
    jump = ('Command.Movement.Jump', [], [], 'GSTN_GA_PlatformJump')
    return {
        'scheme': ('SIS_Test_Code', ['Input.Button.Special']),
        # (asset, ability class, tag, code buttons)
        'supers': [('SUP_Test_A', 'GSTN_GA_TestSuperA', 'Super.Test.A', ['Input.Button.Dodge', 'Input.Button.Interact']),
                   ('SUP_Test_B', 'GSTN_GA_TestSuperB', 'Super.Test.B', ['Input.Button.Interact', 'Input.Button.Dodge'])],
        'kits': [
            # Gamma: the dash command is removed and an added command takes its button (Sprint)
            ('KIT_Test_Gamma', 'Kit.Test.Gamma',
             {'removed': ['Command.Movement.Dash'], 'added': [('Command.Test.Gamma', ['Input.Button.Sprint'], False, False, 0)]},
             [jump, ('Command.Test.Gamma', [], [], 'GSTN_GA_TestGammaMove')], 0, []),
            # Delta: plain special, and an EX chord (Special + Buff) that falls through to it when the meter is short
            ('KIT_Test_Delta', 'Kit.Test.Delta',
             {'removed': [], 'added': [('Command.Special.Ex', ['Input.Button.Special', 'Input.Button.Buff'], True, True, 100)]},
             [jump, ('Command.Special.Neutral', [], [], 'GSTN_GA_TestSpecial'), ('Command.Special.Ex', [], [], 'GSTN_GA_TestSpecialEx')], 0, []),
            # Squat: the jump command is bound to the test jump with a 6 frame squat (data on the ability)
            ('KIT_Test_Squat', 'Kit.Test.Squat', {'removed': [], 'added': []},
             [('Command.Movement.Jump', [], [], 'GSTN_GA_TestJumpSquat')], 0, []),
            # Sigma: two supers entered as codes (Special, then the super's own two buttons)
            ('KIT_Test_Sigma', 'Kit.Test.Sigma', {'removed': [], 'added': []}, [jump], 2, ['SUP_Test_A', 'SUP_Test_B']),
            # Omega: a timeline that waits combat frames before a strike, on the plain special
            ('KIT_Test_Omega', 'Kit.Test.Omega', {'removed': [], 'added': []},
             [jump, ('Command.Special.Neutral', [], [], 'GSTN_GA_TestWaitStrike')], 0, []),
        ],
    }


def setup_combat_content_c18():
    """The C18 assets under /Game/GASNPPTest/Combat: a PerSuperCode scheme, two supers and four kits. New assets only; rerun converges."""
    specs = c18_content_specs()
    base = combat_content_specs()
    utils = unreal.GSTN_TestSlotUtils

    def tags(names):
        return [_make_tag(n) for n in names]

    name, entry_buttons = specs['scheme']
    scheme, _ = _get_or_create(name, TEST_COMBAT_DIR, unreal.GSTN_SuperInputScheme, unreal.DataAssetFactory())
    scheme.set_editor_property('mode', unreal.GSTN_SuperInputMode.PER_SUPER_CODE)
    entry = utils.make_slot_entry(unreal.GameplayTag(), tags(entry_buttons), False, False, 0)
    scheme.set_editor_property('code_entry', entry.get_editor_property('inline'))
    unreal.EditorAssetLibrary.save_loaded_asset(scheme)

    for asset_name, ability, tag, buttons in specs['supers']:
        sup, _ = _get_or_create(asset_name, TEST_COMBAT_DIR, unreal.GSTN_SuperDefinition, unreal.DataAssetFactory())
        sup.set_editor_property('ability', getattr(unreal, ability))
        sup.set_editor_property('super_tags', _tag_container([tag]))
        sup.set_editor_property('meter', _make_tag('Combat.Meter.TestSuper'))
        sup.set_editor_property('meter_cost', 100.0)
        sup.set_editor_property('input_code', utils.make_press_sequence(tags(buttons)))
        unreal.EditorAssetLibrary.save_loaded_asset(sup)

    for asset_name, identity, command_set_spec, bindings, super_slots, supers in specs['kits']:
        kit, _ = _get_or_create(asset_name, TEST_COMBAT_DIR, unreal.GSTN_PrimaryKitDefinition, unreal.DataAssetFactory())
        kit.set_editor_property('kit_tags', _tag_container([identity]))
        kit.set_editor_property('primary_weapon', unreal.load_asset(f'{TEST_COMBAT_DIR}/WPN_Test_Blade'))
        kit.set_editor_property('allowed_additional_weapons', _any_tag_query(base['allowed_support_role']))
        kit.set_editor_property('max_additional_weapons', 1)
        values = []
        for stat_tag, value in {'Combat.SetByCaller.Stat.WalkSpeed': 600.0, 'Combat.SetByCaller.Stat.JumpVelocity': 700.0}.items():
            stat = unreal.GSTN_StatValue()
            stat.set_editor_property('stat', _make_tag(stat_tag))
            stat.set_editor_property('value', value)
            values.append(stat)
        stats = unreal.GSTN_KitStats()
        stats.set_editor_property('values', values)
        kit.set_editor_property('stats', stats)
        kit.set_editor_property('init_stats_effect', getattr(unreal, base['init_effect'], None))
        command_set = unreal.GSTN_CommandSet()
        command_set.set_editor_property('base_set', unreal.load_asset(base['base_slot_set']))
        command_set.set_editor_property('removed_commands', _tag_container(command_set_spec['removed']))
        command_set.set_editor_property('added_slots', [utils.make_slot_entry(_make_tag(c), tags(b), chord, fall, offset)
                                                        for c, b, chord, fall, offset in command_set_spec['added']])
        kit.set_editor_property('command_set', command_set)
        kit.set_editor_property('kit_bindings', [b for b in (_binding(*row) for row in bindings) if b])
        kit.set_editor_property('super_slot_count', super_slots)
        if super_slots:
            kit.set_editor_property('super_input_scheme', unreal.load_asset(f'{TEST_COMBAT_DIR}/{specs["scheme"][0]}'))
            kit.set_editor_property('allowed_supers', _any_tag_query('Super.Test'))
        unreal.EditorAssetLibrary.save_loaded_asset(kit)
    log('C18 combat content written (scheme, supers, kits Gamma / Delta / Sigma / Omega)')
    return True


# ---------------------------------------------------------------------------------------------------- 6. hit test content

def hits_content_specs():
    """M4 H8 test content as plain data. Hurtboxes are capsules in the local space of the pawn (the actor origin is the middle of its capsule).
    (name, bone, tag, priority, radius, half height incl. the hemispheres, local z)"""
    return {
        'hurtbox_set': 'HBS_Test_Pawn',
        'hurtboxes': [
            ('torso', 'spine_03', 'Combat.Hurtbox.Torso', 1, 35.0, 55.0, 15.0),
            ('head', 'head', 'Combat.Hurtbox.Head', 2, 22.0, 22.0, 68.0),
            ('feet', 'foot_l', 'Combat.Hurtbox.Feet', 0, 30.0, 30.0, -62.0),
        ],
        # M4 H12: the low attack of the dodge scenarios sits on crouch + attack
        'weapon': ('WPN_Test_Hits', 'Weapon.Test.Hits', 'Test.Role.Primary', [('Command.Attack.Neutral', ['Input.Context.Grounded'], [], 'GSTN_GA_TestHitMove'),
                                                                               ('Command.Attack.CrouchNeutral', ['Input.Context.Grounded'], [], 'GSTN_GA_TestFeetProbe')]),
        'kit': ('KIT_Test_Hits', 'Kit.Test.Hits', 'WPN_Test_Hits', {'Combat.SetByCaller.Stat.WalkSpeed': 600.0, 'Combat.SetByCaller.Stat.JumpVelocity': 700.0},
                [('Command.Movement.Jump', [], [], 'GSTN_GA_PlatformJump'), ('Command.Special.Neutral', [], [], 'GSTN_GA_TestSpecial'),
                 # M4 H12: the base dodges are kit data like the rest: bound by context, blocked in hitstun and hitstop by their own data
                 ('Command.Defence.Dodge', ['Input.Context.Grounded'], [], 'GSTN_GA_GroundDodge'),
                 ('Command.Defence.Dodge', ['Input.Context.Airborne'], [], 'GSTN_GA_AirDodge')]),
        # M4 H12: the same kit with a special escape in place of the ground dodge: its blocked tags are dropped as data (dodges in hitstun)
        'escape_kit': ('KIT_Test_HitsEscape', 'Kit.Test.HitsEscape', 'WPN_Test_Hits', {'Combat.SetByCaller.Stat.WalkSpeed': 600.0, 'Combat.SetByCaller.Stat.JumpVelocity': 700.0},
                       [('Command.Movement.Jump', [], [], 'GSTN_GA_PlatformJump'), ('Command.Special.Neutral', [], [], 'GSTN_GA_TestSpecial'),
                        ('Command.Defence.Dodge', ['Input.Context.Grounded'], [], 'GSTN_GA_TestEscapeDodge'),
                        ('Command.Defence.Dodge', ['Input.Context.Airborne'], [], 'GSTN_GA_AirDodge')]),
        # M4 H13b: the burst test kit. Special starts a plain ability that is allowed in a hit reaction and leaves it alone (the control), the
        # ground dodge button starts the one that ends the reaction (UGSTN_GA_Base::bEndReactionOnActivation)
        'burst_kit': ('KIT_Test_HitsBurst', 'Kit.Test.HitsBurst', 'WPN_Test_Hits', {'Combat.SetByCaller.Stat.WalkSpeed': 600.0, 'Combat.SetByCaller.Stat.JumpVelocity': 700.0},
                      [('Command.Movement.Jump', [], [], 'GSTN_GA_PlatformJump'), ('Command.Special.Neutral', [], [], 'GSTN_GA_TestReactionPeek'),
                       ('Command.Defence.Dodge', ['Input.Context.Grounded'], [], 'GSTN_GA_TestBurst')]),
    }


def setup_hits_content():
    """Creates/updates the hurtbox set, the hit weapon and kit, and points the test character's combat component at the set. Rewrites every
    property from the specs on each run (a rerun converges). Returns True when something was saved."""
    specs = hits_content_specs()
    base = combat_content_specs()
    changed = False

    set_asset, _ = _get_or_create(specs['hurtbox_set'], TEST_COMBAT_DIR, unreal.GSTN_HurtboxSetDefinition, unreal.DataAssetFactory())
    boxes = []
    for name, bone, tag, priority, radius, half_height, z in specs['hurtboxes']:
        shape = unreal.GSTN_HitShape()
        shape.set_editor_property('type', unreal.GSTN_ShapeType.CAPSULE)
        shape.set_editor_property('extent', unreal.Vector(radius, radius, half_height))
        shape.set_editor_property('local', unreal.Transform(location=unreal.Vector(0.0, 0.0, z)))
        box = unreal.GSTN_Hurtbox()
        box.set_editor_property('name', name)
        box.set_editor_property('bone', bone)
        box.set_editor_property('shape', shape)
        box.set_editor_property('tags', _tag_container([tag]))
        box.set_editor_property('priority', priority)
        boxes.append(box)
    set_asset.set_editor_property('hurtboxes', boxes)
    unreal.EditorAssetLibrary.save_loaded_asset(set_asset)
    changed = True

    name, identity, role, bindings = specs['weapon']
    weapon, _ = _get_or_create(name, TEST_COMBAT_DIR, unreal.GSTN_WeaponDefinition, unreal.DataAssetFactory())
    weapon.set_editor_property('display_name', name)
    weapon.set_editor_property('weapon_tags', _tag_container([identity, role]))
    weapon.set_editor_property('bindings', [b for b in (_binding(*row) for row in bindings) if b])
    weapon.set_editor_property('stats_effect', None)
    weapon.set_editor_property('stat_modifiers', [])
    unreal.EditorAssetLibrary.save_loaded_asset(weapon)

    for kit_key in ('kit', 'escape_kit', 'burst_kit'):
        name, identity, weapon_name, stat_values, bindings = specs[kit_key]
        kit, _ = _get_or_create(name, TEST_COMBAT_DIR, unreal.GSTN_PrimaryKitDefinition, unreal.DataAssetFactory())
        kit.set_editor_property('kit_tags', _tag_container([identity]))
        kit.set_editor_property('primary_weapon', unreal.load_asset(f'{TEST_COMBAT_DIR}/{weapon_name}'))
        kit.set_editor_property('allowed_additional_weapons', _any_tag_query(base['allowed_support_role']))
        kit.set_editor_property('max_additional_weapons', 1)
        values = []
        for tag, value in stat_values.items():
            stat = unreal.GSTN_StatValue()
            stat.set_editor_property('stat', _make_tag(tag))
            stat.set_editor_property('value', value)
            values.append(stat)
        stats = unreal.GSTN_KitStats()
        stats.set_editor_property('values', values)
        kit.set_editor_property('stats', stats)
        kit.set_editor_property('init_stats_effect', getattr(unreal, base['init_effect'], None))
        command_set = unreal.GSTN_CommandSet()
        command_set.set_editor_property('base_set', unreal.load_asset(base['base_slot_set']))
        kit.set_editor_property('command_set', command_set)
        kit.set_editor_property('kit_bindings', [b for b in (_binding(*row) for row in bindings) if b])
        unreal.EditorAssetLibrary.save_loaded_asset(kit)

    # The one shared Blueprint of this step (owner approved 2026-10-24): the combat component's hurtbox set, nothing else
    bp = unreal.load_asset(TEST_CHAR)
    comp = next((obj for _, obj in blueprint_components(bp) if isinstance(obj, unreal.GSTN_CombatSimComponent)), None)
    if not comp:
        log(f'ERROR: no GSTN_CombatSimComponent on {TEST_CHAR}')
    elif comp.get_editor_property('hurtbox_set') != set_asset:
        backup(TEST_CHAR)
        comp.set_editor_property('hurtbox_set', set_asset)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        log(f'{TEST_CHAR}: combat component HurtboxSet -> {specs["hurtbox_set"]}')
    # M4 H10 (owner approved 2026-10-24): the attacker's hit presentation and the fork's lag compensation component, nothing else
    for component_class in (unreal.GSTN_HitPresentationComponent, unreal.NetworkPredictionLagCompensation):
        if any(isinstance(obj, component_class) for _, obj in blueprint_components(bp)):
            continue
        backup(TEST_CHAR)
        root = subobjects.k2_gather_subobject_data_for_blueprint(bp)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=component_class, blueprint_context=bp)
        _, fail = subobjects.add_new_subobject(params)
        if not fail.is_empty():
            log(f'ERROR adding {component_class.__name__}: {fail}')
        else:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            unreal.EditorAssetLibrary.save_loaded_asset(bp)
            log(f'{TEST_CHAR}: added {component_class.__name__}')
    log(f'hit content written under {TEST_COMBAT_DIR}')
    return changed


# ----------------------------------------------------------------------------------------------- 7. H13a combat content

RULES_DIR = '/GSTNCombat/Rules'
STANDARD_RULES = 'GSTN_CombatRules_Standard'


def h13_specs():
    """M4 H13a test content as plain data. The three weight sets are the HBS_Test_Pawn hurtboxes with a different weight scale: light 0.6, medium 1.0
    and heavy 1.8 (a heavy combatant is harder to launch; its reaction is capped at PushBack), and the heavy set has one armoured chest (a high stagger
    threshold on the torso hurtbox: weak hits never stagger it)"""
    return {
        'weight_sets': [
            # (asset, weight scale, set MaxReaction, {hurtbox name: stagger threshold})
            ('HBS_Test_Light', 0.6, 'LAUNCH', {}),
            ('HBS_Test_Medium', 1.0, 'LAUNCH', {}),
            ('HBS_Test_Heavy', 1.8, 'PUSH_BACK', {'torso': 30.0}),
        ],
        # Timeline assets (UGSTN_CombatTimeline) of the four move variants: (asset, space, radius, hit group, (start, end) frame, keys, socket)
        'timelines': [
            ('TL_Test_HitRoot', 'ROOT', 50.0, 0, (4, 8), [(0, (80.0, 0.0, 0.0)), (3, (170.0, 0.0, 0.0))], None),
            ('TL_Test_HitBaked', 'BAKED_SOCKET', 45.0, 1, (10, 14), [], 'hand_r'),
            ('TL_Test_HitWorld', 'WORLD_AT_MOVE_START', 60.0, 2, (16, 20), [(0, (200.0, -160.0, 0.0))], None),
            ('TL_Test_HitAlly', 'ROOT', 140.0, 3, (20, 24), [], None),
        ],
        'blueprints': [
            # (asset, parent class name)
            ('BP_Step_Example', 'GSTN_HitResolutionStep'),
            ('BP_KnockbackModel_Example', 'GSTN_KnockbackModel_WeightDriven'),
            ('BP_OutcomeApplier_Example', 'GSTN_OutcomeApplier'),
            ('BP_AttackerOutcome_Example', 'GSTN_AttackerOutcome'),
            ('BP_RelationshipResolver_Example', 'GSTN_RelationshipResolver'),
        ],
        'physical_materials': ['PM_Test_Wall', 'PM_Test_Glass'],
    }


def setup_standard_rules():
    """/GSTNCombat/Rules/GSTN_CombatRules_Standard: UGSTN_CombatRules::FillStandard (the one place that knows the standard values, shared with the
    built-in copy) plus the native damage effect. The built-in standard rules (CreateStandard) name no damage effect because the Core cannot see the
    GAS layer; an asset can, and without one DamageMode is a number nothing applies. Returns True when saved"""
    asset, _ = _get_or_create(STANDARD_RULES, RULES_DIR, unreal.GSTN_CombatRules, unreal.DataAssetFactory())
    if asset is None:
        log(f'ERROR: could not create {RULES_DIR}/{STANDARD_RULES} (is the GSTNCombat plugin content folder mounted? it needs Plugins/GSTNCombat/Content)')
        return False
    unreal.GSTN_CombatRules.fill_standard(asset)
    asset.set_editor_property('damage_effect', unreal.GSTN_GE_Damage)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log(f'{RULES_DIR}/{STANDARD_RULES} written (damage effect GSTN_GE_Damage)')
    return True


def setup_weight_hurtbox_sets():
    """HBS_Test_Light / Medium / Heavy from the HBS_Test_Pawn specs (hits_content_specs): same hurtboxes, other weight scale, reaction cap and thresholds"""
    hits = hits_content_specs()
    for name, weight, max_reaction, thresholds in h13_specs()['weight_sets']:
        asset, _ = _get_or_create(name, TEST_COMBAT_DIR, unreal.GSTN_HurtboxSetDefinition, unreal.DataAssetFactory())
        boxes = []
        for box_name, bone, tag, priority, radius, half_height, z in hits['hurtboxes']:
            shape = unreal.GSTN_HitShape()
            shape.set_editor_property('type', unreal.GSTN_ShapeType.CAPSULE)
            shape.set_editor_property('extent', unreal.Vector(radius, radius, half_height))
            shape.set_editor_property('local', unreal.Transform(location=unreal.Vector(0.0, 0.0, z)))
            reaction = unreal.GSTN_HitReactionModifiers()
            reaction.set_editor_property('weight_scale', weight)
            if box_name in thresholds:
                threshold = unreal.GSTN_StaggerThreshold()
                threshold.set_editor_property('value', thresholds[box_name])
                reaction.set_editor_property('stagger_threshold', threshold)
            box = unreal.GSTN_Hurtbox()
            box.set_editor_property('name', box_name)
            box.set_editor_property('bone', bone)
            box.set_editor_property('shape', shape)
            box.set_editor_property('tags', _tag_container([tag]))
            box.set_editor_property('priority', priority)
            box.set_editor_property('reaction_scale', reaction)
            boxes.append(box)
        asset.set_editor_property('hurtboxes', boxes)
        asset.set_editor_property('max_reaction', getattr(unreal.GSTN_HitReaction, max_reaction))
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log('weight hurtbox sets written')
    return True


def _hit_data(damage):
    hit = unreal.GSTN_HitData()
    hit.set_editor_property('damage', damage)
    hit.set_editor_property('base_knockback', 500.0)
    hit.set_editor_property('angle_deg', 30.0)
    hit.set_editor_property('hitstop_mode', unreal.GSTN_HitstopMode.OVERRIDE)
    hit.set_editor_property('hitstop_frames_override', 8)
    hit.set_editor_property('attacker_hitstop_frames_override', 6)
    return hit


def setup_move_timelines():
    """The four hit move variants as timeline assets (UGSTN_CombatTimeline): a root hitbox with keys, a baked-socket hitbox (the frames come from
    bake_sockets.py, or a synthetic arc here), a WorldAtMoveStart hitbox and the Ally variant (half shape, no hitstun). An ability points its TimelineAsset at one.
    Rewritten on every run; IsDataValid of each prints the hit matrix"""
    part = unreal.GSTN_PartSelector()
    part.set_editor_property('group_name', 'Body')
    for name, space, radius, group, (start, end), keys, socket in h13_specs()['timelines']:
        asset, _ = _get_or_create(name, TEST_COMBAT_DIR, unreal.GSTN_CombatTimeline, unreal.DataAssetFactory())
        data = unreal.GSTN_CombatTimelineData()
        data.set_editor_property('total_frames', 32)
        data.set_editor_property('first_actionable_frame', 28)
        data.set_editor_property('override_frames', 0)
        spec = unreal.GSTN_HitboxSpec()
        shape = unreal.GSTN_HitShape()
        shape.set_editor_property('type', unreal.GSTN_ShapeType.SPHERE)
        shape.set_editor_property('extent', unreal.Vector(radius, radius, radius))
        spec.set_editor_property('shape', shape)
        spec.set_editor_property('space', getattr(unreal.GSTN_HitboxSpace, space))
        spec.set_editor_property('hit_group', group)
        spec.set_editor_property('data', _hit_data(6.0 if name == 'TL_Test_HitAlly' else 8.0))
        if keys:
            key_list = []
            for frame, offset in keys:
                key = unreal.GSTN_HitboxKey()
                key.set_editor_property('frame', frame)
                key.set_editor_property('offset', unreal.Vector(*offset))
                key_list.append(key)
            spec.set_editor_property('keys', key_list)
            spec.set_editor_property('sweep_between_frames', space == 'ROOT')
        if socket:
            spec.set_editor_property('socket', socket)
            # A synthetic arc until bake_sockets.py is run on a real animation: the hand sweeps from (100, 260) to (100, 140)
            frames = []
            for frame in range(32):
                frames.append(unreal.Transform(location=unreal.Vector(100.0, max(140.0, min(260.0, 260.0 - 40.0 * (frame - start))), 0.0)))
            baked = unreal.GSTN_BakedSocketFrames()
            baked.set_editor_property('frames', frames)
            data.set_editor_property('baked_sockets', {socket: baked})
        if name == 'TL_Test_HitAlly':
            # The ally takes half the shape and no hitstun (the hit still lands: damage and hitstop apply)
            variant = unreal.GSTN_HitVariant()
            variant.set_editor_property('shape_scale', 0.5)
            override = variant.get_editor_property('override')
            override.set_editor_property('fields', 1 << 7)  # EGSTN_HitDataField::HitstunMultiplier
            values = override.get_editor_property('values')
            values.set_editor_property('hitstun_multiplier', 0.0)
            override.set_editor_property('values', values)
            variant.set_editor_property('override', override)
            ally = unreal.GameplayTag()
            ally.import_text('(TagName="Combat.Relationship.Ally")')
            spec.set_editor_property('relationship_variants', {ally: variant})
        data = unreal.GSTN_TimelineUtils.add_hitbox_track(data, start, end, part, [spec])
        asset.set_editor_property('data', data)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log('move timelines written')
    return True


def setup_example_blueprints():
    """One Blueprint per extension class under TEST_COMBAT_DIR: proof that a step / model / applier / attacker outcome / resolver can be authored without C++.
    Created once (a rerun leaves an existing Blueprint, a designer's graph, alone)"""
    changed = False
    for name, parent_name in h13_specs()['blueprints']:
        if unreal.load_asset(f'{TEST_COMBAT_DIR}/{name}'):
            continue
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', getattr(unreal, parent_name).static_class())
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, TEST_COMBAT_DIR, unreal.Blueprint, factory)
        if bp is None:
            log(f'ERROR: could not create {name}')
            continue
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        log(f'created {TEST_COMBAT_DIR}/{name} (parent {parent_name})')
        changed = True
    return changed


def setup_physical_materials():
    """PM_Test_Wall and PM_Test_Glass under TEST_COMBAT_DIR, the materials the L_Test_Combat map puts on its meshes"""
    for name in h13_specs()['physical_materials']:
        _get_or_create(name, TEST_COMBAT_DIR, unreal.PhysicalMaterial, unreal.PhysicalMaterialFactoryNew())
        unreal.EditorAssetLibrary.save_asset(f'{TEST_COMBAT_DIR}/{name}')
    return True


def setup_h13_content():
    """Rules asset, weight hurtbox sets, move timelines, extension Blueprints, physical materials. Never edits a shared Blueprint"""
    ok = setup_standard_rules()
    ok = setup_weight_hurtbox_sets() and ok
    ok = setup_move_timelines() and ok
    ok = setup_example_blueprints() or ok
    ok = setup_physical_materials() and ok
    return ok


def run(wall_transition=False, combat_input=False, test_character=False, combat_content=False, hits_content=False, h13_content=False):
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
    if combat_content:
        log(f'combat content changed: {setup_combat_content()}')
        log(f'C18 combat content changed: {setup_combat_content_c18()}')
    if hits_content:
        log(f'hits content changed: {setup_hits_content()}')
    if h13_content:
        log(f'H13 content changed: {setup_h13_content()}')


if __name__ == '__main__':
    import sys
    run(wall_transition='--wall-transition' in sys.argv, combat_input='--combat-input' in sys.argv,
        test_character='--test-character' in sys.argv, combat_content='--combat-content' in sys.argv,
        hits_content='--hits-content' in sys.argv, h13_content='--h13-content' in sys.argv)
