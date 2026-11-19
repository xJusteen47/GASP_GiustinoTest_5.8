# Movement thread (2026-11-06): the Xbox pad on the test input, the strong and shield buttons and the bounce pad Blueprint. Already run once:
# the assets are saved, nobody needs to run it again. (Removing the GASP slide from the test character's inherited mover could not be done from
# Python: the override does not persist. See docs/notes/handovers/movimento.md.) Idempotent: run it again and it changes nothing that already matches. It SAVES the assets it changes (not maps).
#   import movement_setup as m; m.run()          # editor, Play stopped; writes Saved/MOV/movement_setup.txt
# After it, the designer tunes everything in the editor: the keys in IMC_GASNPP_Test, the pad on BP_Test_BouncePad (Class Defaults, Launch).
import os
import traceback

import unreal

INPUT_DIR = '/Game/GASNPPTest/Input'
TEST_IMC = INPUT_DIR + '/IMC_GASNPP_Test'
ABILITY_IMC = '/Game/GAS/Input/IMC_GAS_Abilities'
INPUT_CONFIG = INPUT_DIR + '/IC_GASNPP_Test'
BOUNCE_PAD = '/Game/GASNPPTest/BP_Test_BouncePad'

# (action path, keyboard keys, pad keys). IMC_GASNPP_Test has priority 5 (UGSTN_AbilityInputComponent), above IMC_Sandbox, and its actions
# consume the key: the GASP pad bindings on the same buttons (X ragdoll, Y interact, B crouch, RT takedown, LT aim, RB walk, LB crouch) stop firing
# for this character
MAPPINGS = [
    (INPUT_DIR + '/IA_GASNPP_Attack', ['LeftMouseButton'], ['Gamepad_FaceButton_Left']),        # X light
    (INPUT_DIR + '/IA_GASNPP_Jump', ['Y'], ['Gamepad_FaceButton_Bottom']),                     # A jump (the kit's variable jump)
    (INPUT_DIR + '/IA_GASNPP_Special', ['MiddleMouseButton'], ['Gamepad_FaceButton_Top']),     # Y special
    (INPUT_DIR + '/IA_GASNPP_Interact', ['O'], ['Gamepad_FaceButton_Right']),                  # B grab (interact tap = grab, hold = pick up / kit swap)
    (INPUT_DIR + '/IA_GASNPP_Strong', ['K'], ['Gamepad_RightTrigger']),                        # RT strong
    (INPUT_DIR + '/IA_GASNPP_Shield', ['I'], ['Gamepad_LeftShoulder']),                        # LB shield (owner layout 2026-11-13)
    (INPUT_DIR + '/IA_GASNPP_Dodge', ['U'], ['Gamepad_RightShoulder']),                        # RB dodge
    (INPUT_DIR + '/IA_GASNPP_Crouch', ['C'], ['Gamepad_LeftTrigger']),                         # LT crouch, own action (GASP's IA_Crouch fired its slide)
    (INPUT_DIR + '/IA_GASNPP_Sprint', ['L'], []),
]
NEW_ACTIONS = ['IA_GASNPP_Strong', 'IA_GASNPP_Shield']
# Appended after the committed buttons, so the bits of the existing buttons stay where they are
NEW_BUTTONS = [('Input.Button.Strong', INPUT_DIR + '/IA_GASNPP_Strong'), ('Input.Button.Shield', INPUT_DIR + '/IA_GASNPP_Shield')]
# The project jump ability also listens to A in IMC_GAS_Abilities (same priority as the test context): A would fire both jumps
UNMAP_ABILITY = [('/Game/Input/IA_Jump', 'Gamepad_FaceButton_Bottom')]
# Placeholder bounce pad: fixed launch, hold jump on landing = HeldJumpSpeedScale x the speed
PAD_DEFAULTS = {'size': (250.0, 250.0, 40.0), 'speed': 1300.0, 'held_scale': 1.5}

_log = []


def log(text):
    _log.append(text)
    unreal.log(f'[movement_setup] {text}')


def _key(name):
    key = unreal.Key()
    key.set_editor_property('key_name', name)
    return key


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return tag


def _save(asset):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def setup_actions():
    for name in NEW_ACTIONS:
        path = f'{INPUT_DIR}/{name}'
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            continue
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, INPUT_DIR, unreal.InputAction, unreal.InputAction_Factory())
        action.set_editor_property('value_type', unreal.InputActionValueType.BOOLEAN)
        _save(action)
        log(f'created {path}')


def setup_mappings():
    imc = unreal.load_asset(TEST_IMC)
    have = {(m.action.get_path_name().split('.')[0] if m.action else '', str(m.key.get_editor_property('key_name')))
            for m in imc.get_editor_property('default_key_mappings').mappings}
    added = []
    for path, keys, pads in MAPPINGS:
        action = unreal.load_asset(path)
        if not action:
            log(f'WARNING missing action {path}')
            continue
        for k in keys + pads:
            if (path, k) not in have:
                imc.map_key(action, _key(k))
                added.append(f'{path.rsplit("/", 1)[1]}={k}')
    if added:
        _save(imc)
    log(f'IMC_GASNPP_Test: added {added}')
    imc2 = unreal.load_asset(ABILITY_IMC)
    removed = []
    for path, k in UNMAP_ABILITY:
        action = unreal.load_asset(path)
        if any(m.action == action and str(m.key.get_editor_property('key_name')) == k for m in imc2.get_editor_property('default_key_mappings').mappings):
            imc2.unmap_key(action, _key(k))
            removed.append(k)
    if removed:
        _save(imc2)
    log(f'IMC_GAS_Abilities: removed {removed}')


def setup_buttons():
    cfg = unreal.load_asset(INPUT_CONFIG)
    buttons = list(cfg.get_editor_property('buttons'))
    names = [b.get_editor_property('tag').export_text() for b in buttons]
    changed = []
    for tag, action in NEW_BUTTONS:
        hit = [i for i, n in enumerate(names) if f'"{tag}"' in n]
        if hit:
            b = buttons[hit[0]]
            if '/Game/' not in str(b.get_editor_property('action')):
                b.set_editor_property('action', unreal.load_asset(action))
                buttons[hit[0]] = b
                changed.append(f'{tag} action')
            continue
        b = unreal.GSTN_InputButtonDef()
        b.set_editor_property('tag', _tag(tag))
        b.set_editor_property('action', unreal.load_asset(action))
        buttons.append(b)
        changed.append(tag)
    if changed:
        cfg.set_editor_property('buttons', buttons)
        _save(cfg)
    log(f'IC_GASNPP_Test: {changed} ({len(buttons)} buttons)')


def setup_bounce_pad():
    if unreal.EditorAssetLibrary.does_asset_exist(BOUNCE_PAD):
        log('BP_Test_BouncePad exists (tune it in its Class Defaults)')
        return
    folder, name = BOUNCE_PAD.rsplit('/', 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.GSTN_Trampoline)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('size', unreal.Vector(*PAD_DEFAULTS['size']))
    launch = cdo.get_editor_property('launch')
    launch.set_editor_property('speed_mode', unreal.GSTN_SurfaceLaunchSpeed.FIXED)
    launch.set_editor_property('speed', PAD_DEFAULTS['speed'])
    launch.set_editor_property('held_jump_speed_scale', PAD_DEFAULTS['held_scale'])
    cdo.set_editor_property('launch', launch)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    _save(bp)
    log(f'created {BOUNCE_PAD}: speed {PAD_DEFAULTS["speed"]}, held x{PAD_DEFAULTS["held_scale"]}')


def run():
    _log.clear()
    for step in (setup_actions, setup_mappings, setup_buttons, setup_bounce_pad):
        try:
            step()
        except Exception:
            log(f'{step.__name__} FAILED {traceback.format_exc()}')
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'MOV')
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, 'movement_setup.txt')
    with open(path, 'w') as f:
        f.write('\n'.join(_log))
    return path
