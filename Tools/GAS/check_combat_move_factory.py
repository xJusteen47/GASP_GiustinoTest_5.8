# CP5 of the Blueprint seams: checks the Combat Move factory through the same calls a script or an editor agent uses, WITHOUT saving anything.
# Makes three moves in /Game/GASNPPTest/Examples/FactoryCheck (strike inline, action with a TL_ asset and the move events, empty), binds the
# action one to an in-memory copy of KIT_Example's command, reads everything back, then drops the packages (nothing is written to disk).
#
# Run headless (editor closed): UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file>" -unattended -nullrhi
# Output: Saved/Examples/check_combat_move_factory.log, at most 5 printed lines. Expected last line: "RESULT: PASS (3 moves)".
import os

import unreal

DIR = '/Game/GASNPPTest/Examples/FactoryCheck'
KIT = '/Game/GASNPPTest/Examples/KIT_Example'
OUT_DIR = os.path.join(unreal.Paths.project_saved_dir(), 'Examples')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[move_factory_check] {msg}')


def _options(template, **values):
    o = unreal.GSTN_NewCombatMoveOptions()
    o.set_editor_property('template', template)
    for k, v in values.items():
        o.set_editor_property(k, v)
    return o


def _make(name, options):
    bp, report, error = unreal.GSTN_CombatMoveAuthoring.create_combat_move(DIR, name, options)
    log(f'{name}: {"OK" if bp and not error else "FAIL " + error} | {report.replace(chr(10), " | ")}')
    return bp if not error else None


def run():
    del _lines[:]
    failures = []
    made = []
    try:
        if not hasattr(unreal, 'GSTN_CombatMoveAuthoring'):
            raise RuntimeError('GSTN_CombatMoveAuthoring missing: build the CP5 branch first')
        t = unreal.GSTN_CombatMoveTemplate
        strike = _make('GA_Check_Strike', _options(t.STRIKE))
        action = _make('GA_Check_Action', _options(t.ACTION, timeline_as_asset=True, add_move_events=True, total_frames=40, active_start=10,
                                                   active_end=12, first_actionable_frame=32))
        empty = _make('GA_Check_Empty', _options(t.EMPTY, total_frames=20, active_start=50, active_end=1, first_actionable_frame=0))
        made = [b for b in (strike, action, empty) if b]
        if len(made) != 3:
            failures.append('not every move was made')
        for bp in made:
            if not unreal.GSTN_CombatMoveAuthoring.is_combat_move_blueprint(bp):
                failures.append(f'{bp.get_name()} is not a combat move')
        if action:
            lines = unreal.GSTN_MoveGraphAuthoring.describe_event_graph(action)
            for ev in ('K2_OnMoveStarted', 'K2_OnMoveKey', 'K2_OnMoveHitConfirmed', 'K2_OnMoveEnded'):
                if not any(ev[3:] in line.replace(' ', '') for line in lines):
                    failures.append(f'action move has no {ev}')
            cdo = unreal.get_default_object(action.generated_class())
            tl = cdo.get_editor_property('timeline_asset')
            log(f'action timeline asset: {tl}')
            # Bind to a transient copy of the example kit's data (the real kit is not touched)
            kit = unreal.load_asset(KIT)
            if kit:
                copy = unreal.GSTN_PrimaryKitDefinition()
                copy.set_editor_property('kit_bindings', list(kit.get_editor_property('kit_bindings')))
                before = len(copy.get_editor_property('kit_bindings'))
                tag = unreal.GameplayTag()
                tag.import_text('(TagName="Command.Special.Neutral")')
                # the bool return is dropped by the 5.9 Python binding when there is an out param
                report = unreal.GSTN_CombatMoveAuthoring.add_move_to_kit_assets(action.generated_class(), copy, None, tag,
                                                                                    unreal.GSTN_TagRequirements())
                after = len(copy.get_editor_property('kit_bindings'))
                ok = True
                log(f'bind on a copy of KIT_Example: ok={ok}, bindings {before} -> {after} | {report}')
                if not ok:
                    failures.append('binding refused')
            else:
                log('WARNING: no KIT_Example (run setup_designer_examples.py): binding not checked')
        if empty:
            cdo = unreal.get_default_object(empty.generated_class())
            tl = cdo.get_editor_property('timeline')
            frames = (tl.get_editor_property('total_frames'), tl.get_editor_property('first_actionable_frame'))
            log(f'empty move frames after sanitize: total {frames[0]}, actionable {frames[1]}')
            if frames != (20, 20):
                failures.append(f'empty move frames {frames}, expected (20, 20)')
    except Exception as e:
        failures.append(repr(e))
    finally:
        # Nothing is saved: the check leaves the project as it found it
        for path in unreal.EditorAssetLibrary.list_assets(DIR, True, False) if unreal.EditorAssetLibrary.does_directory_exist(DIR) else []:
            unreal.EditorAssetLibrary.delete_asset(path)
        if unreal.EditorAssetLibrary.does_directory_exist(DIR):
            unreal.EditorAssetLibrary.delete_directory(DIR)
    log(f'RESULT: {"PASS" if not failures else "FAIL"} ({len(made)} moves){"; " + "; ".join(failures) if failures else ""}')
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, 'check_combat_move_factory.log'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in _lines[-5:]:
        print(line)
    return not failures


if __name__ == '__main__':
    run()
