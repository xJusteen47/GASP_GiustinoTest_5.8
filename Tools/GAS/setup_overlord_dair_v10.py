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
LABEL = 'V10'
SRC = '/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirV10_Build'
BASE_TL = f'{DIR}/TL_Overlord_DAirV9'
LAND_SRC = {'Heavy': '/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirLandHeavy',
            'Light': '/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAirLandLight'}
LAND_BLEND_IN = {'Heavy': 3.0 / 60.0, 'Light': 2.0 / 60.0}
LAND_BLEND_OUT = 4.0 / 60.0
# (start, end exclusive, clip, lag frames): internal 0-based timeline frames
WINDOWS = [(0, 3, 'Light', 4), (3, 31, 'Heavy', 16), (31, 43, 'Light', 4)]
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'DairV10', 'setup_overlord_dair_v10.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[dair_v10] {msg}')


def _land_clip(kind):
    lib = unreal.EditorAssetLibrary
    anim_path, montage_path = f'{DIR}/AS_OverlordDAirLand{kind}', f'{DIR}/AM_OverlordDAirLand{kind}'
    if not lib.does_asset_exist(anim_path):
        if not lib.duplicate_asset(LAND_SRC[kind], anim_path):
            raise RuntimeError(f'could not copy {LAND_SRC[kind]}')
        log(f'created {anim_path}')
    anim = unreal.load_asset(anim_path)
    montage = unreal.load_asset(montage_path) if lib.does_asset_exist(montage_path) else None
    if not montage:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('source_animation', anim)
        factory.set_editor_property('target_skeleton', anim.get_editor_property('skeleton'))
        montage = K._tools().create_asset(f'AM_OverlordDAirLand{kind}', DIR, unreal.AnimMontage, factory)
        log(f'created {montage_path}')
    for prop, t in (('blend_in', LAND_BLEND_IN[kind]), ('blend_out', LAND_BLEND_OUT)):
        blend = montage.get_editor_property(prop)
        blend.set_editor_property('blend_time', t)
        montage.set_editor_property(prop, blend)
    montage.set_editor_property('rate_scale', K.MONTAGE_PLAY_RATE)   # one animation frame per simulation tick, as the move's montage
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
        anim, montage = K._anim_and_montage(LABEL, SRC, False)
        if not montage:
            raise RuntimeError('V10 anim/montage not built')
        land = {}
        saves = [anim, montage]
        for kind in ('Heavy', 'Light'):
            a, m = _land_clip(kind)
            land[kind] = m
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
        cap_ok = CAP.run(LABEL)
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
