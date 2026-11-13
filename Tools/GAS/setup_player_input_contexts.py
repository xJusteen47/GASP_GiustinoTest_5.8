# Player input contexts (pad playtest 2026-11-13): the test character's keys come from OUR data, not from GASP's IMC_Sandbox.
# The Game Animation Sample's character Blueprint adds IMC_Sandbox at BeginPlay. It maps the pad and the keyboard to GASP's own actions (DPad left
# = next pawn, X = ragdoll, RT = takedown, LT = aim, RB = walk, sticks' clicks = sprint/strafe, Space = GASP jump + traversal...), which fought with
# our layout (X light, A jump, Y special, B grab, RT strong, LT shield, LB dodge, RB crouch). Nothing in a Blueprint is changed: the input config
# lists IMC_Sandbox in ReplacedMappingContexts (the player input source takes it off the local player whenever it shows up) and adds
# IMC_GSTN_Locomotion, a copy of IMC_Sandbox that keeps only what the character still needs from it:
#   IA_Move (WASD, left stick: the stick of the simulation, UGSTN_InputConfig::MoveAction), IA_Look / IA_Look_Gamepad (the camera),
#   IA_Crouch on C (RB is in IMC_GASNPP_Test; the config's Crouch button is IA_Crouch).
# Every combat button stays in IMC_GASNPP_Test (priority 10, added by the test component).
# Idempotent: rerun after an engine/content update. Saves only the two assets below (never a map). Output: at most 5 printed lines.
import unreal

SANDBOX = '/Game/Input/IMC_Sandbox'
LOCOMOTION_DIR, LOCOMOTION_NAME = '/Game/GASNPPTest/Input', 'IMC_GSTN_Locomotion'
LOCOMOTION = f'{LOCOMOTION_DIR}/{LOCOMOTION_NAME}'
CONFIGS = ['/Game/GASNPPTest/Input/IC_GASNPP_Test']
LOCOMOTION_PRIORITY = 0
# (action asset name, key name): what IMC_GSTN_Locomotion keeps of IMC_Sandbox
KEEP = {('IA_Move', 'W'), ('IA_Move', 'S'), ('IA_Move', 'A'), ('IA_Move', 'D'), ('IA_Move', 'Gamepad_Left2D'),
        ('IA_Look', 'Mouse2D'), ('IA_Look_Gamepad', 'Gamepad_Right2D'), ('IA_Crouch', 'C')}


def _locomotion():
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(LOCOMOTION):
        if not eal.duplicate_asset(SANDBOX, LOCOMOTION):
            raise RuntimeError(f'could not duplicate {SANDBOX}')
    imc = unreal.load_asset(LOCOMOTION)
    removed = 0
    for m in list(imc.get_editor_property('default_key_mappings').get_editor_property('mappings')):
        pair = (m.action.get_name() if m.action else '', str(m.key.get_editor_property('key_name')))
        if pair not in KEEP:
            imc.unmap_key(m.action, m.key)
            removed += 1
    kept = sorted((m.action.get_name(), str(m.key.get_editor_property('key_name')))
                  for m in imc.get_editor_property('default_key_mappings').get_editor_property('mappings'))
    missing = KEEP - set(kept)
    if missing:
        raise RuntimeError(f'IMC_Sandbox no longer has {sorted(missing)}')
    eal.save_loaded_asset(imc, only_if_is_dirty=False)
    return imc, removed, kept


def _config(path, imc):
    cfg = unreal.load_asset(path)
    entry = unreal.GSTN_InputContextEntry()
    entry.set_editor_property('context', imc)
    entry.set_editor_property('priority', LOCOMOTION_PRIORITY)
    contexts = [e for e in cfg.get_editor_property('mapping_contexts') if e.get_editor_property('context') != imc]
    cfg.set_editor_property('mapping_contexts', contexts + [entry])
    replaced = [c for c in cfg.get_editor_property('replaced_mapping_contexts') if c and c.get_path_name() != unreal.load_asset(SANDBOX).get_path_name()]
    cfg.set_editor_property('replaced_mapping_contexts', replaced + [unreal.load_asset(SANDBOX)])
    unreal.EditorAssetLibrary.save_loaded_asset(cfg, only_if_is_dirty=False)
    return f'{path.rsplit("/", 1)[-1]}: contexts {len(contexts) + 1}, replaced {len(replaced) + 1}'


# Pad layout of the combat buttons (owner, 2026-11-13): crouch on LT, dodge on LB; the shield, LT before, moves to the free RB.
# key name -> action asset path, in IMC_GASNPP_Test; the key's other mappings in that context are removed first
COMBAT_CONTEXT = '/Game/GASNPPTest/Input/IMC_GASNPP_Test'
PAD_LAYOUT = {
    'Gamepad_LeftTrigger': '/Game/Input/IA_Crouch',
    'Gamepad_LeftShoulder': '/Game/GASNPPTest/Input/IA_GASNPP_Dodge',
    'Gamepad_RightShoulder': '/Game/GASNPPTest/Input/IA_GASNPP_Shield',
}


def _pad_layout():
    imc = unreal.load_asset(COMBAT_CONTEXT)
    changed = []
    for key_name, action_path in PAD_LAYOUT.items():
        action = unreal.load_asset(action_path)
        maps = list(imc.get_editor_property('default_key_mappings').get_editor_property('mappings'))
        here = [m for m in maps if str(m.key.get_editor_property('key_name')) == key_name]
        if len(here) == 1 and here[0].action == action:
            continue
        for m in here:
            imc.unmap_key(m.action, m.key)
        key = unreal.Key()
        key.import_text(key_name)
        imc.map_key(action, key)
        changed.append(f'{key_name}->{action.get_name()}')
    if changed:
        unreal.EditorAssetLibrary.save_loaded_asset(imc, only_if_is_dirty=False)
    return 'IMC_GASNPP_Test pad: ' + (', '.join(changed) if changed else 'already set')


def run():
    imc, removed, kept = _locomotion()
    out = [f'{LOCOMOTION_NAME}: removed {removed}, kept {len(kept)}: ' + ' '.join(f'{a}/{k}' for a, k in kept)]
    out += [_config(p, imc) for p in CONFIGS]
    out.append(_pad_layout())
    return '\n'.join(out)


if __name__ == '__main__':
    print(run())
