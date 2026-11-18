import unreal, json
out = r'C:\Users\jacop\gstn_bp\i1_fix.json'
path = '/Game/GASNPPTest/Input/IMC_GASNPP_Test'
imc = unreal.load_asset(path)
dkm = imc.get_editor_property('default_key_mappings')
maps = dkm.get_editor_property('mappings')
log = []
new_maps = []
for m in maps:
    new_maps.append(m)
    key = str(m.get_editor_property('key').get_editor_property('key_name'))
    if key not in ('Gamepad_RightTrigger', 'Gamepad_LeftTrigger'):
        continue
    mods = list(m.get_editor_property('modifiers') or [])
    if any(type(x).__name__ == 'InputModifierDeadZone' for x in mods):
        log.append(key + ': already has a dead zone'); continue
    dz = unreal.new_object(unreal.InputModifierDeadZone, outer=imc)
    dz.set_editor_property('lower_threshold', 0.25)
    dz.set_editor_property('upper_threshold', 1.0)
    dz.set_editor_property('type', unreal.DeadZoneType.AXIAL)
    mods.append(dz)
    m.set_editor_property('modifiers', mods)
    log.append(key + ': dead zone 0.25 added')
dkm.set_editor_property('mappings', new_maps)
imc.set_editor_property('default_key_mappings', dkm)
saved = unreal.EditorAssetLibrary.save_loaded_asset(imc, False)
check = []
imc2 = unreal.load_asset(path)
for m in imc2.get_editor_property('default_key_mappings').get_editor_property('mappings'):
    key = str(m.get_editor_property('key').get_editor_property('key_name'))
    if 'Trigger' in key:
        check.append([key, [(type(x).__name__, x.get_editor_property('lower_threshold')) for x in (m.get_editor_property('modifiers') or [])]])
json.dump({'log': log, 'saved': saved, 'check': check}, open(out, 'w'), indent=1)
