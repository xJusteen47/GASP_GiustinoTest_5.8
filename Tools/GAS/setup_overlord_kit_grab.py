# grab-B fix (owner playtest 2026-11-27): KIT_Test_Overlord (the hub's "OVERLORD JAB" kit pickup) gets the same grab as KIT_Test_Playtest, so B grabs.
# Cause: the kit never had link content. No Command.Link.Grab row, no Command.Link.* input slots and no FGSTN_LinkKitExtension, so the interact
# button had no grab command to fire and the dummies were never offered as grab targets (the offer needs a kit with a link extension, M7 G1).
# setup_grab_ganon_playtest.py gave the playtest kit its grab (PR #53); this script gives the Overlord kit the SAME moves, read from the playtest kit:
#   - the standard link slots and rows (UGSTN_LinkContentLibrary::WriteKitLinkContent, the call KIT_Test_Link's content uses), with the playtest
#     kit's grab ability (GA_Ganon_Playtest_Grab, Ganondorf's standing grab) and its release ability, written only when the kit has no grab row;
#   - the link extension's throw set (Ganondorf's throws GA_Ganon_Playtest_Throw*, no back throw) and the hold rule (90 + 1.7 x %, no mash), copied
#     from the playtest kit's values in setup_grab_ganon_playtest.py. The grab spin rows come from DefaultGSTNLink.ini on every grabbing kit.
# Rerun-safe: a second run changes nothing and saves nothing. Dry run unless GSTN_SAVE=1 (Tools/CI/Save-Assets.ps1 sets it).
#
#   & .\Tools\CI\Save-Assets.ps1 -Script Tools\GAS\setup_overlord_kit_grab.py -Tag overlord-grab      (in GASP_Giustino2_ue59, editor closed)
# Out: Saved/Logs/grab-ganon/overlord-kit.log
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gstn_content as gc  # noqa: E402
import setup_grab_ganon_playtest as ganon  # noqa: E402

KIT = gc.path('KIT_Test_Overlord')
SOURCE = gc.path('KIT_Test_Playtest')
LINK = unreal.GSTN_LinkContentLibrary
SAVE = os.environ.get('GSTN_SAVE') == '1'

_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[overlord_grab] {msg}')


def _cmd(binding):
    return str(binding.get_editor_property('command').get_editor_property('tag_name'))


def _rows(kit, command):
    return [b for b in kit.get_editor_property('kit_bindings') if _cmd(b) == command]


def _ability(kit, command, require=''):
    for b in _rows(kit, command):
        if require and require not in b.get_editor_property('requirements').export_text():
            continue
        ability = b.get_editor_property('ability')
        if ability:
            return _as_class(ability)
    return None


def _as_class(ability):
    # A kit row holds a soft class: Python may hand back the class or a path to it
    if isinstance(ability, unreal.Class):
        return ability
    path = str(ability.export_text() if hasattr(ability, 'export_text') else ability).strip('"')
    return unreal.load_class(None, path)


def _throw_class(key):
    return unreal.EditorAssetLibrary.load_blueprint_class(f'{ganon.CHILD_DIR}/GA_Ganon_Playtest_Throw{key}')


def link_rows(kit, source):
    """The standard link slots and rows, with the playtest kit's grab and release, when the kit has no grab row yet"""
    if any(b.get_editor_property('ability') for b in _rows(kit, 'Command.Link.Grab')):
        log('kit already binds Command.Link.Grab: rows left as they are')
        return False
    grab = _ability(source, 'Command.Link.Grab')
    release = _ability(source, 'Command.Link.Release', 'Combat.Link.Role.Grabber')
    if not grab or not release:
        log(f'ERROR: the playtest kit has no grab ({grab}) or release ({release}) to copy: run setup_grab_ganon_playtest.py first')
        return False
    changed = LINK.write_kit_link_content(kit, grab, None, release, None, None, None, None, None, ganon.MASH['base'])
    log(f'link rows: grab {grab.get_name()}, release {release.get_name()} changed={changed}')
    return changed


def throws_and_hold(kit):
    classes = {key: _throw_class(key) for key in ganon.THROWS}
    missing = [k for k, c in classes.items() if not c]
    if missing:
        log(f'ERROR: missing throw classes {missing}: run setup_grab_ganon_playtest.py first')
        return False
    changed = LINK.set_kit_throws(kit, classes['Forward'], None, classes['Up'], classes['Down'])
    for key, command in ganon.COMMANDS.items():
        changed |= LINK.set_kit_throw(kit, ganon._tag(command), classes[key])
    m = ganon.MASH
    mash = LINK.set_kit_grab_mash_rule(kit, m['base'], m['per_percent'], m['min'], m['max'], m['reduce_per_press'])
    log(f'throw set changed={changed}, hold rule {m} changed={mash}')
    return changed or mash


def report(kit, label):
    rows = [(_cmd(b), str(b.get_editor_property('ability')).rsplit('.', 1)[-1].strip("'\">") if b.get_editor_property('ability') else None)
            for b in kit.get_editor_property('kit_bindings') if _cmd(b).startswith('Command.Link')]
    slots = [str(s.get_editor_property('inline').get_editor_property('command').get_editor_property('tag_name'))
             for s in kit.get_editor_property('command_set').get_editor_property('added_slots')]
    slots = [s for s in slots if s.startswith('Command.Link')]
    got = LINK.get_kit_throws(kit)
    fwd, back, up, down = got[-4:] if isinstance(got, tuple) and len(got) >= 4 else (None,) * 4
    log(f'{label}: link rows {rows}; link slots {slots}; throws F {fwd and fwd.get_name()} B {back and back.get_name()} '
        f'U {up and up.get_name()} D {down and down.get_name()}')


def run():
    del _lines[:]
    kit, source = unreal.load_asset(KIT), unreal.load_asset(SOURCE)
    if not kit or not source:
        log(f'ERROR: kit {KIT} -> {kit}, source {SOURCE} -> {source}')
        return []
    report(source, 'KIT_Test_Playtest')
    report(kit, 'KIT_Test_Overlord before')
    changed = link_rows(kit, source)
    changed |= throws_and_hold(kit)
    report(kit, 'KIT_Test_Overlord after')
    saved = []
    if changed and SAVE:
        unreal.EditorAssetLibrary.save_loaded_asset(kit, False)
        saved.append(kit.get_path_name())
    log(f'changed={changed} save={SAVE} saved {len(saved)}: {saved}')
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Logs', 'grab-ganon', 'overlord-kit.log')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'ERROR' in l][:4] + [_lines[-1][:300]]:
        print(line)
    return saved


if __name__ == '__main__':
    run()
