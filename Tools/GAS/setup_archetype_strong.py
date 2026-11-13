# RT (strong) for the three M11 archetype kits (owner, 2026-11-13: "si, dallo"): LEGGERO, MEDIO and PESANTE had no Command.Strong binding, so RT
# did nothing with them. They get the designer example's strong move (GA_Example_Strong, Tools/GAS/setup_designer_examples.py), bound as
# KIT_Example's weapon binds it (neutral and directional, same requirements). Kit bindings, not weapon bindings: WPN_Test_Medium is shared with
# KIT_Test_Ranger, whose kit binds RT to the aimed shot, and two bindings of one command on one kit would not compile.
# Idempotent: a kit that already binds Command.Strong.* is left alone. Saves the three kits only. Output: at most 5 printed lines.
import unreal

SOURCE_WEAPON = '/Game/GASNPPTest/Examples/WPN_Example'
KITS = ['/Game/GASNPPTest/Combat/KIT_Test_Light', '/Game/GASNPPTest/Combat/KIT_Test_Medium', '/Game/GASNPPTest/Combat/KIT_Test_Heavy']


def _command(binding):
    return str(binding.get_editor_property('command').export_text()) if hasattr(binding.get_editor_property('command'), 'export_text') else str(binding.get_editor_property('command'))


def run():
    weapon = unreal.load_asset(SOURCE_WEAPON)
    strong = [b for b in weapon.get_editor_property('bindings') if 'Command.Strong.' in _command(b)]
    if not strong:
        raise RuntimeError(f'{SOURCE_WEAPON} has no Command.Strong binding (run setup_designer_examples first)')
    out = []
    for path in KITS:
        kit = unreal.load_asset(path)
        bindings = list(kit.get_editor_property('kit_bindings'))
        if any('Command.Strong.' in _command(b) for b in bindings):
            out.append(f'{path.rsplit("/", 1)[-1]}: already has RT')
            continue
        kit.set_editor_property('kit_bindings', bindings + [unreal.GSTN_AbilityBinding.cast(b) if hasattr(unreal.GSTN_AbilityBinding, 'cast') else b for b in strong])
        unreal.EditorAssetLibrary.save_loaded_asset(kit, only_if_is_dirty=False)
        out.append(f'{path.rsplit("/", 1)[-1]}: + {len(strong)} RT binding(s)')
    return '\n'.join(out)


if __name__ == '__main__':
    print(run())
