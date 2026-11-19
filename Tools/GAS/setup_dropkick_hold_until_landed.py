# DROPKICK-LAND (2026-11-15): turns the Overlord dropkick's timeline into a "loop until landed" move with the new Hold Until Landed track
# (FGSTN_TimelineTrack_HoldUntilLanded, C++ on claude/project-thread-qxjy3w). Data only: no Blueprint graph, no input, no project setting.
# Needs the dropkick kit of claude/project-thread-anim-r2 (Tools/GAS/setup_overlord_dropkick_kit.py, Tools/AnimPipeline/dropkick_loop_reps.py):
# run it AFTER that script, and again after any rerun of it (the kit script rewrites the timeline without the track).
#
# What it writes into /Game/GASNPPTest/Combat/Overlord/TL_Overlord_Dropkick (nothing else):
#   Hold Until Landed track [startup, startup + one loop) frames, land frame = first frame of the land clip in the montage (after LOOP_REPS loops).
#   In the air the timeline stands on the loop's last frame (the hitbox, open to the land frame, stays open) and the montage repeats its FIRST
#   loop copy; on the first grounded frame the timeline jumps to the land frame and the montage to the land clip. The montage is not touched:
#   the extra loop copies are simply never played. LOOP_REPS can go back to 1 later (shorter montage), this script follows it.
#   Optional: DROPKICK_FOLLOW_UP=<Blueprint class path of a get-up timeline ability> sets FollowUpTimeline (started when the land clip ends by
#   itself). None exists yet: the get-up clip is anim work. Empty = no follow-up (the land clip's montage tail plays out as before).
#
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_dropkick_hold_until_landed as s; s.run()
# Output:    Saved/AnimR2/setup_dropkick_hold_until_landed.txt, at most 5 printed lines.
import os
import runpy

import unreal

DIR = '/Game/GASNPPTest/Combat/Overlord'
TIMELINE = f'{DIR}/TL_Overlord_Dropkick'
CLIPS = {name: f'{DIR}/AS_OverlordDropkick_{name}' for name in ('Startup', 'Loop', 'Land')}
ANIM_FPS = 60.0
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'AnimR2', 'setup_dropkick_hold_until_landed.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[dropkick_hold] {msg}')


def _loop_reps():
    path = os.path.join(unreal.Paths.project_dir(), 'Tools/AnimPipeline/dropkick_loop_reps.py')
    if not os.path.exists(path):
        raise RuntimeError(f'{path} missing: merge claude/project-thread-anim-r2 first')
    return int(runpy.run_path(path)['LOOP_REPS'])


def run():
    del _lines[:]
    ok = False
    try:
        lib = unreal.EditorAssetLibrary
        for path in [TIMELINE] + list(CLIPS.values()):
            if not lib.does_asset_exist(path):
                raise RuntimeError(f'{path} missing: run Tools/GAS/setup_overlord_dropkick_kit.py first')
        lens = {name: unreal.load_asset(path).get_play_length() for name, path in CLIPS.items()}
        reps = _loop_reps()
        start = int(round(lens['Startup'] * ANIM_FPS))
        end = int(round((lens['Startup'] + lens['Loop']) * ANIM_FPS))
        land = int(round((lens['Startup'] + reps * lens['Loop']) * ANIM_FPS))   # the kit script's loop_end: first land frame
        asset = unreal.load_asset(TIMELINE)
        data = asset.get_editor_property('data')
        total = data.get_editor_property('total_frames')
        if not (0 < start < end <= land < total):
            raise RuntimeError(f'frames do not fit: startup {start}, loop end {end}, land {land}, total {total}')
        data = unreal.GSTN_TimelineUtils.set_hold_until_landed_track(data, start, end, land)
        follow = os.environ.get('DROPKICK_FOLLOW_UP', '').strip()
        if follow:
            cls = lib.load_blueprint_class(follow)
            if not cls:
                raise RuntimeError(f'follow-up class {follow} does not load')
            data.set_editor_property('follow_up_timeline', cls)
            log(f'follow-up timeline {follow}')
        asset.set_editor_property('data', data)
        lib.save_loaded_asset(asset, False)
        log(f'TL_Overlord_Dropkick: hold [{start}, {end}) in the air, land frame {land} (loop x{reps} in the montage), total {total}, '
            f'{len(data.get_editor_property("tracks"))} tracks; saved')
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in _lines[-4:]:
        print(line[:300])
    return ok


if __name__ == '__main__':
    run()
