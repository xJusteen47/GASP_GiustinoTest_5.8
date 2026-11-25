# Camera zoom input (owner 2026-11-25: "an option to adjust the camera zoom"). Makes the 1D action IA_GSTN_CameraZoom and the context
# IMC_GSTN_Camera that binds it; the playtest camera (GSTN_PlaytestCamera.cpp) names both on the P1a camera profile, and the camera plugin adds
# the context to the local player (UGSTN_CameraLocalPlayerSubsystem::EnsureInputContext). The code never names a key: the keys live here.
# The zoom value is per frame like a mouse delta (positive = farther, one unit = x e^1 of the distance, profile DistancePresets.ZoomLogPerUnit):
#   mouse wheel   one notch = -0.12 (wheel up = closer, about 11% per notch)
#   d-pad up/down held = -/+ 0.9 per second (Scale by Delta Time): about 2.5% per frame at 60 fps, the 0.6-1.6 range end to end in ~1.1 s
# The d-pad is free in the owner's pad layout (setup_player_input_contexts.py; GASP's d-pad bindings were in IMC_Sandbox, which is replaced).
# Idempotent: rerun after an engine or content update, it rewrites the mappings to the table below. Saves only these two assets (never a map).
# Restart the editor after the first run: the game module checks once per editor session whether the two assets exist.
#   import camera_zoom_setup as z; print(z.run())        # editor, Play stopped
import unreal

INPUT_DIR = '/Game/GSTN/Input'
ACTION_NAME = 'IA_GSTN_CameraZoom'
CONTEXT_NAME = 'IMC_GSTN_Camera'
WHEEL_PER_NOTCH = -0.12
HELD_PER_SECOND = 0.9
# key name -> (scalar, scale by delta time)
MAPPINGS = {
    'MouseWheelAxis': (WHEEL_PER_NOTCH, False),
    'Gamepad_DPad_Up': (-HELD_PER_SECOND, True),
    'Gamepad_DPad_Down': (HELD_PER_SECOND, True),
}


def _key(name):
    key = unreal.Key()
    key.set_editor_property('key_name', name)
    return key


def _save(asset):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def _asset(name, cls, factory):
    path = f'{INPUT_DIR}/{name}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path), False
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, INPUT_DIR, cls, factory)
    if asset is None:
        raise RuntimeError(f'could not create {path}')
    return asset, True


def _action():
    action, created = _asset(ACTION_NAME, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property('value_type', unreal.InputActionValueType.AXIS1D)
    _save(action)
    return action, created


def _modifiers(owner, scalar, by_delta_time):
    mods = []
    if by_delta_time:
        mods.append(unreal.new_object(unreal.InputModifierScaleByDeltaTime, outer=owner))
    scale = unreal.new_object(unreal.InputModifierScalar, outer=owner)
    scale.set_editor_property('scalar', unreal.Vector(scalar, 1.0, 1.0))
    mods.append(scale)
    return mods


def _context(action):
    imc, created = _asset(CONTEXT_NAME, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    # Only this action's mappings are rewritten; anything a designer added for another action stays
    for m in list(imc.get_editor_property('default_key_mappings').get_editor_property('mappings')):
        if m.action == action:
            imc.unmap_key(m.action, m.key)
    for key_name in MAPPINGS:
        imc.map_key(action, _key(key_name))
    data = imc.get_editor_property('default_key_mappings')
    mappings = list(data.get_editor_property('mappings'))
    for m in mappings:
        key_name = str(m.key.get_editor_property('key_name'))
        if m.action == action and key_name in MAPPINGS:
            scalar, by_dt = MAPPINGS[key_name]
            m.set_editor_property('modifiers', _modifiers(imc, scalar, by_dt))
    data.set_editor_property('mappings', mappings)
    imc.set_editor_property('default_key_mappings', data)
    imc.modify()
    _save(imc)
    check = sorted(f'{m.key.get_editor_property("key_name")}x{len(m.modifiers)}'
                   for m in imc.get_editor_property('default_key_mappings').get_editor_property('mappings') if m.action == action)
    return created, check


def run():
    action, made_action = _action()
    made_context, check = _context(action)
    return (f'{ACTION_NAME}: {"created" if made_action else "updated"} (Axis1D)\n'
            f'{CONTEXT_NAME}: {"created" if made_context else "updated"}, mappings {" ".join(check)} (expect DPad_Down x2, DPad_Up x2, MouseWheelAxis x1)')


if __name__ == '__main__':
    print(run())
