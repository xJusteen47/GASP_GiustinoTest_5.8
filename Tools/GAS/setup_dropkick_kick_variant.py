# Dropkick KICK variant (dropkick-k5, 2026-11-21): the owner's dropkick chain with new startup + loop clips, next to the originals.
# Owner on the r4 videos: the kick has no oomph and the arms are not crossed; while falling he must look smug. The tech bounce (R4) and the
# land clip stay as they are. Nothing the owner plays is touched: every asset below is a copy with the K5 suffix.
#   AS_OverlordDropkick_{Startup,Loop}K5   game copies of the animpipe builds (cards overlord_dropkick_{startup,loop}_k5.json)
#   AM_OverlordDropkickK5                  same segments as AM_OverlordDropkick (startup, loop x LOOP_REPS, land), K5 clips
#   TL_Overlord_DropkickTBK5               duplicate of TL_Overlord_DropkickTBR4 (hitbox, KnockdownOnLanding, R4 bounce) with the K5 montage
#   GA_Overlord_DropkickTBK5               duplicate of GA_Overlord_DropkickTBR4 pointing at TL_Overlord_DropkickTBK5
#   KIT_Test_PlaytestTBK5 (+ WPN copy)     duplicate of KIT_Test_PlaytestTBR4 with the dropkick binding on GA_Overlord_DropkickTBK5
# Gameplay data (hitbox window, damage, hitstop, timing) is NOT changed: startup and loop keep their lengths (16 / 12 frames).
# Run in the editor (bridge or -ExecCmds). Writes Saved/DropkickK5/setup_dropkick_kick_variant.txt, prints at most 5 lines.
import os
import re
import runpy

import unreal

R = os.environ.get('DROPKICK_KICK_ROUND', 'K5')
BASE = os.environ.get('DROPKICK_KICK_BASE', 'R4')          # the tech bounce round the variant sits on
# K6 (2026-11-21, owner yes to an overshoot after the strike): a one-shot SETTLE clip (loop length) between the startup and the loop,
# in place of the first loop repetition, so the montage length and every timeline frame stay the same. The loop can come from an
# earlier round (K6 reuses K5's loop: DROPKICK_KICK_LOOP_ROUND=K5); that copy is never refreshed (the K5 montage plays it).
LOOP_R = os.environ.get('DROPKICK_KICK_LOOP_ROUND', R)
SETTLE = os.environ.get('DROPKICK_KICK_SETTLE', '')         # e.g. 'K6' -> AS_OverlordDropkick_SettleK6
SRC = '/Game/AnimPipeline/Rig/Overlord/Dropkick'
DIR = '/Game/GSTN/Characters/Overlord/Moves/Dropkick'
ROOT = '/Game/GSTN/Characters/Overlord/Kits'  # playtest variant kits
LOOP_REPS = runpy.run_path(os.path.join(unreal.Paths.project_dir(), 'Tools/AnimPipeline/dropkick_loop_reps.py'))['LOOP_REPS']
BLEND_IN_SECONDS, BLEND_OUT_SECONDS = 2.0 / 60.0, 4.0 / 60.0   # as setup_overlord_dropkick_kit.py
ANIM_FPS = 60.0
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'DropkickK5', 'setup_dropkick_kick_variant.txt')
lib = unreal.EditorAssetLibrary
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[dropkick_kick_variant] {msg}')


def _dup(src, dst, refresh=False):
    if refresh and lib.does_asset_exist(dst):
        lib.delete_asset(dst)
    if not lib.does_asset_exist(dst):
        if not lib.does_asset_exist(src) or not lib.duplicate_asset(src, dst):
            raise RuntimeError(f'could not duplicate {src} -> {dst}')
        log(f'created {dst} (copy of {src})')
    return unreal.load_asset(dst)


def _segment(anim, start, reps):
    seg = unreal.AnimSegment()
    for prop, value in (('anim_reference', anim), ('anim_start_time', 0.0), ('anim_end_time', anim.get_play_length()),
                        ('anim_play_rate', 1.0), ('looping_count', reps)):
        seg.set_editor_property(prop, value)
    out = seg.copy()   # StartPos is read-only from Python: import_text on a copy (as setup_overlord_dropkick_kit.py)
    out.import_text(re.sub(r'StartPos=[-0-9.e]+', 'StartPos=%f' % start, seg.export_text()))
    return out


def _montage(refresh):
    clips = {'Startup': _dup(f'{SRC}/Startup/AS_OverlordDropkick_Startup{R}', f'{DIR}/AS_OverlordDropkick_Startup{R}', refresh),
             'Loop': _dup(f'{SRC}/Loop/AS_OverlordDropkick_Loop{LOOP_R}', f'{DIR}/AS_OverlordDropkick_Loop{LOOP_R}', refresh and LOOP_R == R),
             'Land': unreal.load_asset(f'{DIR}/AS_OverlordDropkick_Land')}
    if SETTLE:
        clips['Settle'] = _dup(f'{SRC}/Loop/AS_OverlordDropkick_Settle{SETTLE}', f'{DIR}/AS_OverlordDropkick_Settle{SETTLE}', refresh)
    lens = {k: v.get_play_length() for k, v in clips.items()}
    if SETTLE and abs(lens['Settle'] - lens['Loop']) > 0.5 / ANIM_FPS:
        raise RuntimeError(f"settle length {lens['Settle']:.4f} s != loop {lens['Loop']:.4f} s: it would shift the timeline, stop")
    for name in ('Startup', 'Loop'):
        ref = unreal.load_asset(f'{DIR}/AS_OverlordDropkick_{name}').get_play_length()
        if abs(lens[name] - ref) > 0.5 / ANIM_FPS:
            raise RuntimeError(f'{name} length {lens[name]:.4f} s != original {ref:.4f} s: the timeline frames would no longer match, stop')
    path = f'{DIR}/AM_OverlordDropkick{R}'
    if lib.does_asset_exist(path):
        lib.delete_asset(path)            # this script's own copy: always rebuilt
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property('source_animation', clips['Startup'])
    factory.set_editor_property('target_skeleton', clips['Startup'].get_editor_property('skeleton'))
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(f'AM_OverlordDropkick{R}', DIR, unreal.AnimMontage, factory)
    if SETTLE:
        segs = [_segment(clips['Startup'], 0.0, 1), _segment(clips['Settle'], lens['Startup'], 1),
                _segment(clips['Loop'], lens['Startup'] + lens['Settle'], LOOP_REPS - 1),
                _segment(clips['Land'], lens['Startup'] + lens['Settle'] + (LOOP_REPS - 1) * lens['Loop'], 1)]
    else:
        segs = [_segment(clips['Startup'], 0.0, 1), _segment(clips['Loop'], lens['Startup'], LOOP_REPS),
                _segment(clips['Land'], lens['Startup'] + LOOP_REPS * lens['Loop'], 1)]
    track = unreal.AnimTrack()
    track.set_editor_property('anim_segments', segs)
    slot = unreal.SlotAnimationTrack()
    slot.set_editor_property('slot_name', 'DefaultSlot')
    slot.set_editor_property('anim_track', track)
    montage.set_editor_property('slot_anim_tracks', [slot])
    for prop, seconds in (('blend_in', BLEND_IN_SECONDS), ('blend_out', BLEND_OUT_SECONDS)):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property('blend_time', seconds)
        montage.set_editor_property(prop, blend)
    lib.save_loaded_asset(montage, False)
    unreal.EditorLoadingAndSavingUtils.reload_packages([montage.get_outermost()], unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    montage = unreal.load_asset(path)
    original = unreal.load_asset(f'{DIR}/AM_OverlordDropkick')
    if abs(montage.get_play_length() - original.get_play_length()) > 0.5 / ANIM_FPS:
        raise RuntimeError(f'montage length {montage.get_play_length():.4f} s differs from AM_OverlordDropkick {original.get_play_length():.4f} s')
    log(f'AM_OverlordDropkick{R}: {montage.get_play_length() * ANIM_FPS:.1f} frames (= original), segments '
        + ', '.join(f"{sg.get_editor_property('anim_reference').get_name()} x{sg.get_editor_property('looping_count')}" for sg in segs))
    return [clips[k] for k in ('Startup', 'Loop', 'Settle') if k in clips], montage


def _swap(bindings, old_cls, new_cls):
    out, n = [], 0
    for b in bindings:
        a = b.get_editor_property('ability')
        if a and a.get_path_name() == old_cls.get_path_name():
            b.set_editor_property('ability', new_cls)
            n += 1
        out.append(b)
    return out, n


def run(refresh=False):
    del _lines[:]
    ok = False
    try:
        clips, montage = _montage(refresh)
        tl = _dup(f'{DIR}/TL_Overlord_DropkickTB{BASE}', f'{DIR}/TL_Overlord_DropkickTB{R}')
        data = tl.get_editor_property('data')
        data.set_editor_property('montage', montage)
        tl.set_editor_property('data', data)
        if f'AM_OverlordDropkick{R}' not in tl.get_editor_property('data').export_text():
            raise RuntimeError('the timeline did not take the K5 montage')
        bp = _dup(f'{DIR}/GA_Overlord_DropkickTB{BASE}', f'{DIR}/GA_Overlord_DropkickTB{R}')
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cls = lib.load_blueprint_class(f'{DIR}/GA_Overlord_DropkickTB{R}')
        unreal.get_default_object(cls).set_editor_property('timeline_asset', tl)
        old_cls = lib.load_blueprint_class(f'{DIR}/GA_Overlord_DropkickTB{BASE}')
        kit = _dup(f'{ROOT}/KIT_Test_PlaytestTB{BASE}', f'{ROOT}/KIT_Test_PlaytestTB{R}')
        kit.set_editor_property('display_name', f'PLAYTEST + DROPKICK {R}')
        kb, n_kit = _swap(list(kit.get_editor_property('kit_bindings')), old_cls, cls)
        kit.set_editor_property('kit_bindings', kb)
        saves, n_wpn = [tl, bp, kit], 0
        weapon = kit.get_editor_property('primary_weapon')
        if weapon and f'TB{R}' not in weapon.get_path_name():
            wcopy = _dup(weapon.get_path_name().split('.')[0], f'{ROOT}/WPN_Test_PlaytestTB{R}')
            wb, n_wpn = _swap(list(wcopy.get_editor_property('bindings')), old_cls, cls)
            wcopy.set_editor_property('bindings', wb)
            kit.set_editor_property('primary_weapon', wcopy)
            saves.append(wcopy)
        log(f'KIT_Test_PlaytestTB{R}: dropkick bindings swapped kit {n_kit} weapon {n_wpn}')
        if n_kit + n_wpn == 0 and not lib.does_asset_exist(f'{ROOT}/WPN_Test_PlaytestTB{R}'):
            raise RuntimeError(f'no GA_Overlord_DropkickTB{BASE} binding on KIT_Test_PlaytestTB{BASE} or its weapon')
        for a in clips + saves:
            lib.save_loaded_asset(a, False)
        log('saved: ' + ', '.join(a.get_name() for a in clips + saves + [montage]))
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'ERROR' in l][:4] + [_lines[-1][:300] if _lines else '']:
        print(line)
    return ok


if __name__ == '__main__':
    run(refresh=os.environ.get('DROPKICK_KICK_REFRESH') == '1')
