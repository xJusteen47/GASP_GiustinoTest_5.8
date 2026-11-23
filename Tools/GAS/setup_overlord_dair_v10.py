# DAIR-V10 (2026-11-17, thread dair-dropkick-anim): the Overlord down-air V10 as a VARIANT next to V9 (V9 is never touched), plus the stomp
# landing lag of handoffs/stomp-atterraggio.md. Data only: no C++, no Blueprint graph, no input, no project setting, no map.
#   AS/AM_OverlordDAirV10  the V10 build (charge head fix, cards/overlord/dair/overlord_dair_v10.json) and its montage; blend in 2 f, blend out
#                          8 f (V9's), via setup_overlord_dair_kit.py's own helpers
#   TL_Overlord_DAirV10    a DUPLICATE of TL_Overlord_DAirV9 (hit data, hitbox, every track V9 has), montage -> AM_OverlordDAirV10; Body Capsule
#                          tracks re-measured on the V10 clip (setup_dair_body_capsule.run('V10')); three Landing windows, all LandingLag:
#                            designer f1-3   (internal 0..2)   4 f light clip    AM_OverlordDAirLandLight
#                            designer f4-31  (internal 3..30)  16 f heavy clip   AM_OverlordDAirLandHeavy
#                            designer f32-43 (internal 31..42) 4 f light clip    (the window runs to the end of the 43-frame timeline)
#                          AutoCancel unused, no landing hit (owner: "autocancel" = REDUCED lag, not zero)
#   GA/WPN/KIT_Test_OverlordDAirV10  as the V9 ones (setup_overlord_dair_kit.py helpers)
#   AS/AM_OverlordDAirLandHeavy / LandLight  copies of the animpipe builds (cards overlord_dair_land_heavy/light, Antigravity blocking + curve sheets)
# Editor:  import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_overlord_dair_v10 as s; s.run()
# Output:  Saved/DairV10/setup_overlord_dair_v10.txt, at most 5 printed lines.
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault('OVERLORD_DAIR_BLEND_OUT_FRAMES', '8')
import setup_guard  # noqa: E402
import setup_overlord_dair_kit as K  # noqa: E402
import setup_dair_body_capsule as CAP  # noqa: E402

DIR = K.DIR
# Review rounds (DAIR_V10_ROUND=R2, ...): new assets with the suffix next to round 1, never over it. R2: the V10 clip/montage of round 1 (charge
# PASS), the heavy clip AS_OverlordDAirLandHeavyR2 (review r1 notes), TL_Overlord_DAirV10R2 duplicated from TL_Overlord_DAirV10, its own GA/WPN/KIT
ROUND = os.environ.get('DAIR_V10_ROUND', '')
LABEL = 'V10' + ROUND
SRC = '/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirV10_Build'
BASE_TL = f'{DIR}/TL_Overlord_DAirV10' if ROUND else f'{DIR}/TL_Overlord_DAirV9'
_B = '/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirLand'
# Per round, the clips: kind -> (asset name suffix, animpipe build to copy or None = reuse the existing AS_OverlordDAirLand<suffix>, blend in f, out f)
CLIPS = {
    '': {'Heavy': ('Heavy', _B + 'Heavy', 3, 4), 'Light': ('Light', _B + 'Light', 2, 4)},
    # R2: the light montage gets its own asset with 1-frame blends: the 4-frame clip (0.05 s) was shorter than 2 f in + 4 f out and never showed (review r1)
    'R2': {'Heavy': ('HeavyR2', _B + 'HeavyR2', 3, 4), 'Light': ('LightR2', None, 1, 1)},
    # R3 (review r2): heavy-tucked for the charge window (ball on the floor start), light r3 (12 frames, covers the locomotion landing), heavy = R2
    'R3': {'HeavyTuck': ('HeavyTuckR3', _B + 'HeavyTuckR3', 1, 3), 'Heavy': ('HeavyR2', None, 3, 4), 'Light': ('LightR3', _B + 'LightR3', 1, 4)},
    # R4 (owner + cloud check of dair_land_early r3): the f1-3 window gets LightTuckR4 (starts from the tucked air pose at contact, arms crossed);
    # the other clips are R3's, untouched; f32+ keeps LightR3
    'R4': {'LightTuck': ('LightTuckR4', _B + 'LightTuckR4', 1, 4), 'HeavyTuck': ('HeavyTuckR3', None, 1, 3), 'Heavy': ('HeavyR2', None, 3, 4),
           'Light': ('LightR3', None, 1, 4)},
}[ROUND]
# (start, end exclusive, clip, lag frames): internal 0-based timeline frames (designer frame = internal + 1)
# R3: the tucked window runs past the charge (anim 0-12) by the 2-frame landing latency: the review's dair_land shows the ball (anim 12) and is
# decided on internal 13 (designer f14), so the window is internal 3-13 = designer f4-14
# F4 (2026-11-17): f4-12 (DAIR_V10_TUCK_END=12) was tried after the landing fixes (z9mi0s): dair_land touched down on anim 10 and still got
# HeavyR2 (pelvis 45 -> 73 pop, Saved/DairV10/f4_r4b/dair_land/frames.jsonl v89-90): the montage still trails the timeline. Default stays f4-14
_TUCK_END = int(os.environ.get('DAIR_V10_TUCK_END', '14'))
WINDOWS = ([(0, 3, 'LightTuck', 4), (3, _TUCK_END, 'HeavyTuck', 16), (_TUCK_END, 31, 'Heavy', 16), (31, 43, 'Light', 4)] if ROUND == 'R4'
           else [(0, 3, 'Light', 4), (3, 14, 'HeavyTuck', 16), (14, 31, 'Heavy', 16), (31, 43, 'Light', 4)] if ROUND == 'R3'
           else [(0, 3, 'Light', 4), (3, 31, 'Heavy', 16), (31, 43, 'Light', 4)])
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'DairV10', 'setup_overlord_dair_v10.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[dair_v10] {msg}')


def _land_clip(kind):
    lib = unreal.EditorAssetLibrary
    name, src, blend_in, blend_out = CLIPS[kind]
    montage_path = f'{DIR}/AM_OverlordDAirLand{name}'
    # R2's light montage plays round 1's light animation; every other clip has its own copy of its build
    anim_path = f'{DIR}/AS_OverlordDAirLandLight' if name == 'LightR2' else f'{DIR}/AS_OverlordDAirLand{name}'
    if not lib.does_asset_exist(anim_path):
        if not src or not lib.duplicate_asset(src, anim_path):
            raise RuntimeError(f'could not copy {src} to {anim_path}')
        log(f'created {anim_path}')
    anim = unreal.load_asset(anim_path)
    reused = lib.does_asset_exist(montage_path) and ROUND and src is None
    montage = unreal.load_asset(montage_path) if lib.does_asset_exist(montage_path) else None
    if not montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('source_animation', anim)
        factory.set_editor_property('target_skeleton', anim.get_editor_property('skeleton'))
        montage = K._tools().create_asset(f'AM_OverlordDAirLand{name}', DIR, unreal.AnimMontage, factory)
        log(f'created {montage_path}')
    if not reused:   # a clip reused from an earlier round keeps its montage as it is
        for prop, frames in (('blend_in', blend_in), ('blend_out', blend_out)):
            blend = montage.get_editor_property(prop)
            blend.set_editor_property('blend_time', frames / 60.0)
            montage.set_editor_property(prop, blend)
        montage.set_editor_property('rate_scale', K.MONTAGE_PLAY_RATE)   # one animation frame per simulation tick, as the move's montage
        # R3 tucked clip: its rise is its last 6 frames; with the automatic trigger the blend out began at f10 and mixed the rise with the base
        # layer's landing crouch (Saved/DairV10/variant_r3c/dair_land v100-103): play to the end, then blend out
        if 'Tuck' in name:   # (LightTuckR4 too: its rise is its last frames)
            montage.set_editor_property('blend_out_trigger_time', 0.0)
    log(f'{kind}: {anim.get_play_length() * 60:.1f} frames, montage {montage.get_play_length():.3f} s')
    return anim, montage


def _timeline(montage, land):
    lib, path = unreal.EditorAssetLibrary, f'{DIR}/TL_Overlord_DAir{LABEL}'
    if not lib.does_asset_exist(path):
        if not lib.duplicate_asset(BASE_TL, path):
            raise RuntimeError(f'could not duplicate {BASE_TL}')
        log(f'created {path} (duplicate of {BASE_TL})')
    asset = unreal.load_asset(path)
    data = asset.get_editor_property('data')
    data.set_editor_property('montage', montage)
    utils = unreal.GSTN_TimelineUtils
    data = utils.remove_landing_tracks(data)
    for start, end, kind, frames in WINDOWS:
        lag = unreal.GSTN_RicochetResponse()
        lag.set_editor_property('recoil_montage', land[kind])
        lag.set_editor_property('recoil_frames', frames)
        data = utils.add_landing_track(data, start, end, unreal.GSTN_LandingAction.LANDING_LAG, lag, None)
    asset.set_editor_property('data', data)
    log(f'{path}: montage {montage.get_name()}, landing windows ' + ', '.join(f'{s}-{e - 1} {k} {f} f' for s, e, k, f in WINDOWS)
        + f', total {data.get_editor_property("total_frames")}, FAF {data.get_editor_property("first_actionable_frame")}')
    return asset


def run():
    del _lines[:]
    ok = False
    try:
        K._guard = setup_guard.Guard.for_unreal('setup_overlord_dair_v10', force=False, log=log)
        K._lines = _lines
        if ROUND:   # the charge passed review: the round-1 V10 clip and montage, untouched
            anim, montage = unreal.load_asset(f'{DIR}/AS_OverlordDAirV10'), unreal.load_asset(f'{DIR}/AM_OverlordDAirV10')
        else:
            anim, montage = K._anim_and_montage(LABEL, SRC, False)
        if not montage:
            raise RuntimeError('V10 anim/montage not built')
        land = {}
        saves = [] if ROUND else [anim, montage]
        for kind, (name, src, _, _) in CLIPS.items():
            a, m = _land_clip(kind)
            land[kind] = m
            if not (ROUND and src is None):   # a clip reused from an earlier round is not saved again
                saves += [a, m]
        timeline = _timeline(montage, land)
        bp, cls = K._ability(LABEL, timeline)
        tail = K._weapon_and_kit(LABEL, cls)
        if not tail:
            raise RuntimeError('weapon or kit not built')
        saves += [timeline, bp] + tail
        for a in saves:
            unreal.EditorAssetLibrary.save_loaded_asset(a, False)
        log('saved: ' + ', '.join(a.get_name() for a in saves))
        cap_ok = True if ROUND else CAP.run(LABEL)   # R2 keeps the V10 tracks it was duplicated with (same clip)
        log(f'body capsule V10: {"ok" if cap_ok else "FAILED"} (Saved/AnimR2/setup_dair_body_capsule.txt)')
        ok = bool(cap_ok)
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'ERROR' in l or 'WARNING' in l][:4] + [_lines[-1][:300] if _lines else '']:
        print(line)
    return ok


if __name__ == '__main__':
    run()
