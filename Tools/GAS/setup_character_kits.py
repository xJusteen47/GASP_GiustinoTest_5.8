# The kits of the other three characters of the roster, and the first movement tuning of the Voyager (owner 2026-11-24 22:51Z:
# "predisporre gli altri kit ... e iniziare con il kit del voyager a regolarlo"; estimates calibrated on a "medium" character).
#
#   KIT_Test_BirdKnight / KIT_Test_FlameFury / KIT_Test_Voyager  (Characters/<Name>/Kits, gstn_content.ROSTER)
#       Copies of KIT_Test_Playtest: every shared system (locomotion, air crouch, defence, get-ups, dodge...) and, until each
#       character has its own moves, the playtest moves as placeholders. The "KIT_Test_" prefix is kept on purpose: the shared-system
#       scripts (align_kits_movement_1118, setup_kit_visuals...) reach every KIT_Test_* kit (owner rule 2026-11-17). No greatsword:
#       the greybox gives it only to kits named "Overlord" (gstn.Greybox.SwordKits).
#   DA_Locomotion_BirdKnight / _FlameFury / _Voyager  (Data/Locomotion)
#       Each kit gets its OWN profile, a copy of DA_Locomotion_Default as it is on the machine that runs the script (the owner's local
#       tuning on the desktop: run it there, not on a machine with the git version of the Default). BirdKnight and FlameFury stay
#       equal to the Default until they are tuned; the Voyager gets VOYAGER below. DA_Locomotion_Default itself is never written.
#
# Idempotent and safe on owner tuning: an existing kit or profile is kept as it is (only the kit -> profile link is checked), and the
# Voyager values are written only when its profile is created. CHARKITS_VOYAGER=1 writes them again over the existing Voyager profile
# (its old values go to the report first). CHARKITS_MODE=dump only writes the report. Saves .uasset only, never a map.
# Headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_character_kits.py" -unattended -nullrhi
# Writes Saved/CharacterKits/setup_character_kits.txt and prints one line.
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gstn_content as gc  # noqa: E402

SOURCE_KIT = 'KIT_Test_Playtest'
DEFAULT_PROFILE = 'DA_Locomotion_Default'
LABELS = {'BirdKnight': 'BIRDKNIGHT', 'FlameFury': 'FLAMEFURY', 'Voyager': 'VOYAGER'}

# Voyager = the "medium" character (agile Link OOT build: not Toad-light, not Ganon-heavy). Frames at the 62.5 Hz fixed tick.
# Reference = the Default as last dumped (runstart-accel-1, runstart-v8): stage 1 470 cm/s 2000 cm/s^2 ramp 14 f/0.15, stage 2 610/1400
# ramp 30 f/0.25, stage 3 850/1200, 40 f to stage 2, stamp off. The Voyager answers sooner and climbs the stages faster (concave feel:
# strong first frames, the last speed earned), with the same top speed: the kits differ by how they get there, not by how fast they are.
# Per stage: field -> value; only the fields listed are written, the rest stays as copied from the Default.
VOYAGER_STAGES = {
    0: {'max_speed': 500.0, 'acceleration': 2600.0, 'accel_ramp_frames': 10, 'ramp_start_fraction': 0.30, 'frames_to_next_stage': 32},
    1: {'max_speed': 650.0, 'acceleration': 1700.0, 'accel_ramp_frames': 20, 'ramp_start_fraction': 0.30,
        'entry_burst_frames': 3, 'entry_accel_multiplier': 6.0},
    2: {'max_speed': 850.0, 'acceleration': 1400.0, 'entry_burst_frames': 2, 'entry_accel_multiplier': 8.0},
}
# entry_burst_*: the climb into stage 2 and 3 is almost a step, not a ramp (owner, readable stages S1; same values as
# setup_run_stage_steps.py writes on the Default, seen with gstn.RunStage.Readable 1)
# Run start stamp, option A of the advice thread (2026-11-24 22:45Z): short, from rest only (MaxStartSpeed), a little creep instead of
# Toad's 21 f almost in place. Jump, dodge and attacks are not held by it (it only scales the walking acceleration); L3 skips it.
VOYAGER_STAMP = {'frames': 10, 'accel_fraction': 0.15}
STAGE_FIELDS = ('max_speed', 'acceleration', 'turning_strength', 'release_brake', 'frames_to_next_stage', 'next_stage_speed_fraction',
                'accel_ramp_frames', 'ramp_start_fraction', 'entry_burst_frames', 'entry_accel_multiplier')
L = unreal.GSTN_PropertyTextLibrary
EAL = unreal.EditorAssetLibrary


def _dump(profile, lines, tag):
    rs = profile.get_editor_property('run_stages')
    stamp = rs.get_editor_property('start_stamp')
    lines.append('  %s stamp frames=%d accel_fraction=%.3f max_start_speed=%.0f' % (
        tag, stamp.get_editor_property('frames'), stamp.get_editor_property('accel_fraction'), stamp.get_editor_property('max_start_speed')))
    for i, st in enumerate(rs.get_editor_property('stages')):
        lines.append('  %s stage %d %s' % (tag, i + 1, ' '.join('%s=%s' % (k, round(st.get_editor_property(k), 3)) for k in STAGE_FIELDS)))
    lines.append('  %s jump %s' % (tag, L.get_property_text(profile, 'JumpDesigner')[:160]))


def _profile(name, lines):
    path = gc.path('DA_Locomotion_' + name)
    created = False
    if not EAL.does_asset_exist(path):
        if not EAL.duplicate_asset(gc.path(DEFAULT_PROFILE), path):
            raise RuntimeError('cannot copy %s to %s' % (DEFAULT_PROFILE, path))
        created = True
    lines.append('%s: %s' % (path, 'copied from ' + DEFAULT_PROFILE if created else 'exists, kept'))
    return unreal.load_asset(path), created


def _apply_voyager(profile, lines):
    _dump(profile, lines, 'before')
    rs = profile.get_editor_property('run_stages')
    stages = list(rs.get_editor_property('stages'))
    for i, fields in VOYAGER_STAGES.items():
        if i >= len(stages):
            lines.append('  stage %d missing (%d stages), skipped' % (i + 1, len(stages)))
            continue
        for k, v in fields.items():
            stages[i].set_editor_property(k, v)
    rs.set_editor_property('stages', stages)
    stamp = rs.get_editor_property('start_stamp')
    for k, v in VOYAGER_STAMP.items():
        stamp.set_editor_property(k, v)
    rs.set_editor_property('start_stamp', stamp)
    profile.set_editor_property('run_stages', rs)
    EAL.save_loaded_asset(profile, False)
    _dump(profile, lines, 'after')


def _kit(name, lines):
    path = gc.path('KIT_Test_' + name)
    if not EAL.does_asset_exist(path):
        if not EAL.duplicate_asset(gc.path(SOURCE_KIT), path):
            raise RuntimeError('cannot copy %s to %s' % (SOURCE_KIT, path))
        kit = unreal.load_asset(path)
        kit.set_editor_property('display_name', LABELS[name])
        lines.append('%s: copied from %s' % (path, SOURCE_KIT))
    else:
        kit = unreal.load_asset(path)
        lines.append('%s: exists, kept' % path)
    # The kit's locomotion entry points at its own profile (the copy pointed at the Default)
    ext = L.get_property_text(kit, 'Extensions')
    own = 'DA_Locomotion_' + name
    if own in ext:
        lines.append('  locomotion -> %s (already)' % own)
    elif DEFAULT_PROFILE in ext:
        L.set_property_text(kit, 'Extensions', ext.replace(DEFAULT_PROFILE, own))
        lines.append('  locomotion -> %s' % own)
    else:
        lines.append('  WARNING no locomotion entry on %s pointing at %s: link it by hand' % (SOURCE_KIT, DEFAULT_PROFILE))
    EAL.save_loaded_asset(kit, False)


def run():
    lines = []
    dump_only = os.environ.get('CHARKITS_MODE', '') == 'dump'
    reapply = os.environ.get('CHARKITS_VOYAGER') == '1'
    done = []
    for name in gc.ROSTER:
        try:
            if dump_only:
                p = gc.path('DA_Locomotion_' + name)
                if EAL.does_asset_exist(p):
                    lines.append(p)
                    _dump(unreal.load_asset(p), lines, 'now')
                continue
            profile, created = _profile(name, lines)
            if name == 'Voyager' and (created or reapply):
                _apply_voyager(profile, lines)
            else:
                _dump(profile, lines, 'now')
            _kit(name, lines)
            done.append(name)
        except Exception as e:  # report and go on with the next character
            lines.append('%s: ERROR %s' % (name, e))
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'CharacterKits')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'setup_character_kits.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    errors = sum('ERROR' in l or 'WARNING' in l for l in lines)
    summary = 'setup_character_kits: %s%s, %d errors/warnings; Saved/CharacterKits/setup_character_kits.txt' % (
        'dump' if dump_only else 'kits ' + ', '.join(done), '', errors)
    unreal.log(summary)
    print(summary)


if __name__ == '__main__':
    run()
