# Player input contexts (pad playtest 2026-11-13): the test character's keys come from OUR data, not from GASP's IMC_Sandbox.
# The Game Animation Sample's character Blueprint adds IMC_Sandbox at BeginPlay. It maps the pad and the keyboard to GASP's own actions (DPad left
# = next pawn, X = ragdoll, RT = takedown, LT = aim, RB = walk, sticks' clicks = sprint/strafe, Space = GASP jump + traversal...), which fought with
# our layout (X light, A jump, Y special, B grab, RT strong, LT shield, LB dodge, RB crouch). Nothing in a Blueprint is changed: the input config
# lists IMC_Sandbox in ReplacedMappingContexts (the player input source takes it off the local player whenever it shows up) and adds
# IMC_GSTN_Locomotion, a copy of IMC_Sandbox that keeps only what the character still needs from it:
#   IA_Move (WASD, left stick: the stick of the simulation, UGSTN_InputConfig::MoveAction), IA_Look / IA_Look_Gamepad (the camera),
#   nothing else: crouch is our own IA_GASNPP_Crouch (below), because GASP's IA_Crouch also fires the GASP Blueprint's crouch/slide.
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
        ('IA_Look', 'Mouse2D'), ('IA_Look_Gamepad', 'Gamepad_Right2D')}


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


# Owner's pad layout (2026-11-13, final): X light, Y special, RT strong, RB dodge, LT crouch, A jump, B grab/interact; the shield goes to the free LB.
# Crouch gets its own action: the config's Crouch button used GASP's IA_Crouch, which the GASP character Blueprint also answers (its crouch and
# slide on RB, owner playtest). key name -> action asset path, in IMC_GASNPP_Test; the key's other mappings in that context are removed first.
COMBAT_CONTEXT = '/Game/GASNPPTest/Input/IMC_GASNPP_Test'
CROUCH_ACTION = '/Game/GASNPPTest/Input/IA_GASNPP_Crouch'
CROUCH_TEMPLATE = '/Game/GASNPPTest/Input/IA_GASNPP_Dodge'   # a plain consuming boolean action
GASP_CROUCH = '/Game/Input/IA_Crouch'
PAD_LAYOUT = {
    'Gamepad_LeftTrigger': CROUCH_ACTION,
    'C': CROUCH_ACTION,
    'Gamepad_RightShoulder': '/Game/GASNPPTest/Input/IA_GASNPP_Dodge',
    'Gamepad_LeftShoulder': '/Game/GASNPPTest/Input/IA_GASNPP_Shield',
}


def _crouch_action():
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(CROUCH_ACTION) and not eal.duplicate_asset(CROUCH_TEMPLATE, CROUCH_ACTION):
        raise RuntimeError(f'could not make {CROUCH_ACTION}')
    action = unreal.load_asset(CROUCH_ACTION)
    eal.save_loaded_asset(action, only_if_is_dirty=True)
    for path in CONFIGS:
        cfg = unreal.load_asset(path)
        buttons = list(cfg.get_editor_property('buttons'))
        hit = False
        for b in buttons:
            if 'Input.Button.Crouch' in str(b.get_editor_property('tag').export_text()) and b.get_editor_property('action') != action:
                b.set_editor_property('action', action)
                hit = True
        if hit:
            cfg.set_editor_property('buttons', buttons)
            eal.save_loaded_asset(cfg, only_if_is_dirty=False)
    return action


def _pad_layout():
    imc = unreal.load_asset(COMBAT_CONTEXT)
    changed = []
    gasp_crouch = unreal.load_asset(GASP_CROUCH)
    for m in list(imc.get_editor_property('default_key_mappings').get_editor_property('mappings')):
        if m.action == gasp_crouch:
            imc.unmap_key(m.action, m.key)
            changed.append(f'-{m.key.get_editor_property("key_name")}/IA_Crouch')
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
    out.append(f'crouch action: {_crouch_action().get_name()}')
    out.append(_pad_layout())
    return '\n'.join(out)


if __name__ == '__main__':
    print(run())
