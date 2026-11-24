# Overlord F. throw variant (fthrow-overlord-1, 2026-11-23): the animpipe build of cards/overlord/fthrow/overlord_fthrow_v1.json as a
# playable throw, next to everything the owner plays (nothing existing is changed):
#   AS_OverlordFThrowV1   game copy of /Game/AnimPipeline/Rig/Overlord/FThrow/AS_OverlordFThrowV1_Build
#   AM_OverlordFThrowV1   one-segment montage, blend in 2 / out 4 anim frames (as the dropkick)
#   GA_Overlord_FThrowV1  Blueprint child of GSTN_GA_LinkThrowForward (keeps its aim policy: instant turn) with the owner's 1C frame data:
#                         throw key (impact) game frame 20, FAF 46, total = the clip (52 game frames), hitstop 8 on the victim
# Damage, knockback and angle stay the placeholders of the common F. throw (the owner sets them). The kit is NOT edited: the capture gives
# the class to the project's FallbackThrows.Forward and the matching HoldPointCm in memory only (Tools/GASNPPTest/fthrow_capture_pre.py).
# Run in the editor. Writes Saved/FThrow/setup_fthrow_variant.txt, prints at most 5 lines.
import os
import re

import unreal

V = os.environ.get('FTHROW_VERSION', 'V1')
SRC = f'/Game/AnimPipeline/Rig/Overlord/FThrow/AS_OverlordFThrow{V}_Build'
DIR = '/Game/GASNPPTest/Combat/Overlord'
IMPACT_G, FAF_G, HITSTOP = 20, 46, 8
GAME_HZ, ANIM_FPS = 62.5, 60.0
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'FThrow', 'setup_fthrow_variant.txt')
lib = unreal.EditorAssetLibrary
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[fthrow_variant] {msg}')


def _sub(text, key, value):
    new, n = re.subn(rf'\b{key}=[-0-9.]+', f'{key}={value}', text)
    if not n:
        raise RuntimeError(f'{key} not found in the timeline text')
    return new


def run():
    del _lines[:]
    try:
        clip_path = f'{DIR}/AS_OverlordFThrow{V}'
        if lib.does_asset_exist(clip_path):
            lib.delete_asset(clip_path)
        if not lib.duplicate_asset(SRC, clip_path):
            raise RuntimeError(f'could not copy {SRC}')
        clip = unreal.load_asset(clip_path)
        total_g = int(round(clip.get_play_length() * GAME_HZ))
        mpath = f'{DIR}/AM_OverlordFThrow{V}'
        if lib.does_asset_exist(mpath):
            lib.delete_asset(mpath)
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('source_animation', clip)
        factory.set_editor_property('target_skeleton', clip.get_editor_property('skeleton'))
        montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(f'AM_OverlordFThrow{V}', DIR, unreal.AnimMontage, factory)
        for prop, seconds in (('blend_in', 2.0 / ANIM_FPS), ('blend_out', 4.0 / ANIM_FPS)):
            blend = montage.get_editor_property(prop)
            blend.set_editor_property('blend_time', seconds)
            montage.set_editor_property(prop, blend)
        lib.save_loaded_asset(montage, False)
        bpath = f'{DIR}/GA_Overlord_FThrow{V}'
        if not lib.does_asset_exist(bpath):
            f = unreal.BlueprintFactory()
            f.set_editor_property('parent_class', unreal.GSTN_GA_LinkThrowForward)
            unreal.AssetToolsHelpers.get_asset_tools().create_asset(f'GA_Overlord_FThrow{V}', DIR, unreal.Blueprint, f)
            log(f'created {bpath}')
        bp = unreal.load_asset(bpath)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cls = lib.load_blueprint_class(bpath)
        cdo = unreal.get_default_object(cls)
        tl = cdo.get_editor_property('timeline')
        text = tl.export_text()
        text = _sub(text, 'TotalFrames', total_g)
        text = _sub(text, 'FirstActionableFrame', FAF_G)
        text = _sub(text, 'EndFrame', total_g)
        text = re.sub(r'\bFrame=8\b', f'Frame={IMPACT_G}', text)
        text = _sub(text, 'HitstopFramesOverride', HITSTOP)
        tl.import_text(text)
        tl.set_editor_property('montage', montage)
        cdo.set_editor_property('timeline', tl)
        back = cdo.get_editor_property('timeline').export_text()
        for need in (f'TotalFrames={total_g}', f'FirstActionableFrame={FAF_G}', f'Frame={IMPACT_G}', f'AM_OverlordFThrow{V}'):
            if need not in back:
                raise RuntimeError(f'timeline did not take {need}')
        lib.save_loaded_asset(bp, False)
        lib.save_loaded_asset(clip, False)
        log(f'GA_Overlord_FThrow{V}: total {total_g} g, FAF {FAF_G}, impact {IMPACT_G}, hitstop {HITSTOP}, montage {montage.get_play_length() * ANIM_FPS:.1f} anim frames')
        log('timeline: ' + re.sub(r'\s+', ' ', back)[:400])
        log('saved: ' + ', '.join(a.get_name() for a in (clip, montage, bp)))
    except Exception as e:  # noqa: BLE001
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as fh:
        fh.write('\n'.join(_lines) + '\n')
    for line in [l for l in _lines if 'ERROR' in l][:4] + [_lines[-1][:300] if _lines else '']:
        print(line)


if __name__ == '__main__':
    run()
