# Editor GUI driver (not headless: screenshots need a renderer). Copy it to a path without spaces and run
#   UnrealEditor.exe <project>.uproject -ExecCmds="py C:/Temp/m11_verify_pie.py"
# Needs KIT_Test_Modded, MOD_Test_LongBlade, BPK_Test_JetPack (Tools/GAS/setup_kit_mod_test.py). Report: Saved/M11Verify/pie_driver.txt,
# screenshots in %TEMP%/m11v_shot_*.png. Clears the PIE dirty flags and quits the editor 60 s after DONE (no save prompt).
# M11 desktop verify: PIE on L_Test_Hub. Kit pickup with KIT_Test_Modded + MOD_Test_LongBlade, taken through the kit swap subsystem;
# then a backpack pickup (BPK_Test_JetPack) taken the same way; screenshots at each step; then the mod editor preview tab. Never saves a map.
import unreal

OUT = unreal.Paths.project_saved_dir() + 'M11Verify/'
F = '/Game/GASNPPTest/Combat/'
log_lines = []
st = {'t': 0.0, 'step': 0}


def log(s):
    log_lines.append(s)
    unreal.log('M11V: ' + s)
    open(OUT + 'pie_driver.txt', 'w').write('\n'.join(log_lines) + '\n')


def pie_world():
    for w in unreal.EditorLevelLibrary.get_pie_worlds(False) or []:
        return w
    return None


def pawn_of(w):
    return unreal.GameplayStatics.get_player_pawn(w, 0)


def shot(w, name):
    unreal.SystemLibrary.execute_console_command(w, 'HighResShot 1600x900 filename=C:/Users/jacop/AppData/Local/Temp/m11v_shot_%s.png' % name)
    log('screenshot %s' % name)


def loadout_text(lc):
    try:
        return lc.get_editor_property('applied').export_text()[:900]
    except Exception as e:
        return 'applied unreadable: %s' % e


def spawn_pickup(w, kit, ahead):
    pawn = pawn_of(w)
    comp = pawn.get_component_by_class(unreal.GSTN_GASNPPTestComponent)
    loc, fwd = pawn.get_actor_location(), pawn.get_actor_forward_vector()
    where = unreal.Vector(loc.x + fwd.x * ahead, loc.y + fwd.y * ahead, loc.z - 70.0)
    name = comp.server_spawn_kit_pickup(kit, '', where, 0.0)
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.GSTN_KitPickup):
        if a.get_name() == name or name in a.get_path_name():
            return a
    log('pickup %s not found (returned %r)' % (kit, name))
    return None


def step_kit(w):
    pawn = pawn_of(w)
    lc = pawn.get_component_by_class(unreal.GSTN_LoadoutComponent)
    log('pawn %s, loadout before: %s' % (pawn.get_name(), loadout_text(lc)))
    pickup = spawn_pickup(w, 'KIT_Test_Modded', 150.0)
    ld = pickup.get_loadout()
    mods = unreal.GSTN_LoadoutModsExt()
    mods.import_text('(Configs=((Mods=((Slot=(TagName="Mod.Slot.Blade"),Mod="%sMOD_Test_LongBlade.MOD_Test_LongBlade")))))' % F)
    inst = unreal.InstancedStruct()
    inst.import_text('/Script/GSTNKitModsCore.GSTN_LoadoutModsExt' + mods.export_text())
    ld.set_editor_property('extensions', [inst])
    pickup.set_loadout(ld)
    log('kit pickup %s loadout: %s' % (pickup.get_name(), pickup.get_loadout().export_text()[:600]))
    st['pickup'] = pickup


def button_mask(name):
    """Bit of logical button Input.Button.<name> in the test input config (data, as the harness resolves it)"""
    names = []
    for b in unreal.load_asset('/Game/GASNPPTest/Input/IC_GASNPP_Test').get_editor_property('buttons'):
        names.append(str(b.get_editor_property('tag').get_editor_property('tag_name')))
    return 1 << names.index('Input.Button.' + name)


def buttons(w, mask):
    sim = pawn_of(w).get_component_by_class(unreal.GSTN_CombatSimComponent)
    if mask:
        sim.set_scripted_buttons(mask)
    else:
        sim.clear_scripted_buttons()
    log('buttons %s' % mask)


def step_take(w, what):
    pawn = pawn_of(w)
    sub = next((s for s in unreal.ObjectIterator(unreal.GSTN_KitSwapSubsystem) if s.get_outer() == w), None)
    found = sub.find_pickup_for(pawn) if sub else None
    res = 'swap by input only'
    lc = pawn.get_component_by_class(unreal.GSTN_LoadoutComponent)
    log('take %s: focus %s, try_swap -> %s; loadout after: %s' % (what, found.get_name() if found else None, res, loadout_text(lc)))


def step_backpack(w):
    if st.get('pickup'):
        log('left behind on the kit pickup: %s' % st['pickup'].get_loadout().export_text()[:300])
        st['pickup'].destroy_actor()
    pickup = spawn_pickup(w, 'KIT_Test_Hits', 150.0)
    payload = unreal.GSTN_BackpackPickupPayload()
    payload.import_text('(Backpack="%sBPK_Test_JetPack.BPK_Test_JetPack")' % F)
    inst = unreal.InstancedStruct()
    inst.import_text('/Script/GSTNBackpackPickups.GSTN_BackpackPickupPayload' + payload.export_text())
    pickup.set_payload(inst)
    log('backpack pickup %s payload %s' % (pickup.get_name(), pickup.get_payload().export_text()[:300]))


def preview():
    kit = unreal.load_asset(F + 'KIT_Test_Modded')
    mod = unreal.load_asset(F + 'MOD_Test_LongBlade')
    cmd = unreal.GameplayTag()
    cmd.import_text('(TagName="Command.Attack.Neutral")')
    log('open preview -> %s' % (unreal.GSTN_KitModsEditorUtils.open_preview(kit, [mod], cmd),))


PLAN = [
    (8.0, lambda w: shot(w, '0_start')),
    (1.0, step_kit),
    (2.0, lambda w: shot(w, '1_kit_pickup_on_ground')),
    (0.5, lambda w: buttons(w, button_mask('Interact'))),
    (1.0, lambda w: buttons(w, 0)),
    (1.0, lambda w: step_take(w, 'kit (after holding Interact 1 s)')),
    (1.0, lambda w: shot(w, '2_after_kit_swap')),
    (0.5, step_backpack),
    (2.0, lambda w: shot(w, '3_backpack_pickup_on_ground')),
    (0.5, lambda w: buttons(w, button_mask('Interact'))),
    (1.0, lambda w: buttons(w, 0)),
    (1.0, lambda w: step_take(w, 'backpack (after holding Interact 1 s)')),
    (1.0, lambda w: shot(w, '4_after_backpack')),
    (0.5, lambda w: buttons(w, button_mask('Attack'))),
    (0.12, lambda w: buttons(w, 0)),
    (0.05, lambda w: shot(w, '5_attack_a')),
    (0.15, lambda w: shot(w, '5_attack_b')),
    (0.15, lambda w: shot(w, '5_attack_c')),
    (2.0, None),
]


def tick(dt):
    st['t'] += dt
    try:
        if st['step'] == 0:
            if st['t'] > 5.0:
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
                st['step'], st['t'] = 1, 0.0
                log('PIE requested')
            return
        w = pie_world()
        i = st['step'] - 1
        if i < len(PLAN):
            if w is None:
                if st['t'] > 60.0:
                    log('no PIE world after 60 s'); st['step'] = 99
                return
            delay, fn = PLAN[i]
            if st['t'] >= delay:
                st['t'] = 0.0
                st['step'] += 1
                if fn:
                    try:
                        fn(w)
                    except Exception as e:
                        log('step %d failed: %s' % (i, e))
            return
        if st['step'] == len(PLAN) + 1:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            st['step'] += 1; st['t'] = 0.0
            return
        if st['step'] == len(PLAN) + 2 and st['t'] > 4.0:
            st['step'] += 1; st['t'] = 0.0
            preview()
            return
        if st['step'] == len(PLAN) + 3 and st['t'] > 6.0:
            st['step'] += 1
            log('DONE (editor left open for the window capture)')
        if st['step'] == len(PLAN) + 4 and st['t'] > 60.0:
            st['step'] += 1
            for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()) + list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()):
                unreal.GSTN_GASNPPTestComponent.clear_package_dirty_flag(p)
            log('dirty flags cleared, quitting')
            unreal.SystemLibrary.quit_editor()
    except Exception as e:
        log('tick error %s' % e)
        st['step'] = 999


unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level('/Game/Levels/Test/L_Test_Hub')
st['h'] = unreal.register_slate_post_tick_callback(tick)
log('driver armed')




