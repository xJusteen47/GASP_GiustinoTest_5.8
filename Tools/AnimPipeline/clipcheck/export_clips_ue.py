"""Export the clips listed in clips.json from the project to FBX + a sidecar JSON, so clip_check.py can judge them offline.

Desktop or laptop, editor CLOSED on that checkout (headless, one editor per machine), read-only on assets:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file> <clips.json> <outdir>" -unattended -nullrhi
Or from the editor's Python console: exec with sys.argv set the same way.

Per clip entry with an "asset" field (an object path like /Game/GSTN/Animation/Crouch/AS_GSTN_Crouch_Enter, or a bare asset
name, resolved through the asset registry, /Game/GSTN first):
  <outdir>/<clip>.fbx         AnimSequenceExporterFBX, no preview mesh
  <outdir>/<clip>.meta.json   fps, frames, length, rate scale, root motion flags, float curves sampled per frame,
                              notifies (name, time), sync markers (name, time), bone track count
Then point clip_check at them: "fbx": "<outdir>/<clip>.fbx", "meta": "<outdir>/<clip>.meta.json".
Writes <outdir>/export.log; prints max 5 lines. Nothing in the project is saved or changed.
"""
import json, os, sys

import unreal

AL = unreal.AnimationLibrary


def log(lines, s):
    lines.append(s)


def resolve(asset):
    if asset.startswith('/'):
        return unreal.load_asset(asset)
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence')], package_paths=['/Game'],
                          recursive_paths=True)
    hits = [a for a in ar.get_assets(flt) if str(a.asset_name) == asset]
    hits.sort(key=lambda a: (not str(a.package_path).startswith('/Game/GSTN'), str(a.package_path)))
    return hits[0].get_asset() if hits else None


def curves(anim, fps, n):
    out = {}
    try:
        names = AL.get_animation_curve_names(anim, unreal.RawCurveTrackTypes.RCT_FLOAT)
    except Exception:
        names = []
    for nm in names:
        try:
            times, values = AL.get_float_keys(anim, nm)
        except Exception:
            continue
        times, values = list(times), list(values)
        smp = []
        for i in range(n):  # linear between keys, held at the ends
            t = i / fps
            if not times:
                smp.append(0.0)
            elif t <= times[0]:
                smp.append(values[0])
            elif t >= times[-1]:
                smp.append(values[-1])
            else:
                k = max(j for j in range(len(times)) if times[j] <= t)
                a = (t - times[k]) / max(times[k + 1] - times[k], 1e-6)
                smp.append(values[k] + (values[k + 1] - values[k]) * a)
        out[str(nm)] = [round(v, 4) for v in smp]
    return out


def notifies(anim):
    out = []
    try:
        for ev in AL.get_animation_notify_events(anim):
            name, t = None, None
            for key in ('notify_name',):
                try:
                    name = str(ev.get_editor_property(key))
                except Exception:
                    pass
            try:
                name = name if name and name != 'None' else ev.get_editor_property('notify').get_class().get_name()
            except Exception:
                pass
            try:
                t = float(AL.get_animation_notify_event_trigger_time(anim, ev)) if hasattr(AL, 'get_animation_notify_event_trigger_time') \
                    else float(ev.get_editor_property('trigger_time_offset'))
            except Exception:
                t = None
            out.append({'name': name, 'time': t})
    except Exception as e:
        out.append({'error': str(e)})
    return out


def markers(anim):
    try:
        return [{'name': str(m.marker_name), 'time': float(m.time)} for m in AL.get_anim_sync_markers(anim)]
    except Exception:
        return []


def main():
    # no args = defaults (the commandlet splits -script args on spaces, so paths with spaces cannot be passed)
    here = os.path.dirname(os.path.abspath(__file__))
    cj = sys.argv[1] if len(sys.argv) > 2 else os.path.join(here, 'clips.json')
    outdir = sys.argv[2] if len(sys.argv) > 2 else os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'ClipCheck', 'export')
    os.makedirs(outdir, exist_ok=True)
    C = json.load(open(cj))
    lines, ok, bad = [], 0, 0
    # headless: the asset registry is empty until scanned (load_asset and get_assets miss existing clips)
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game'], True)
    for clip, cfg in C['clips'].items():
        if clip.startswith('_') or 'asset' not in cfg:
            continue
        anim = resolve(cfg['asset'])
        if anim is None:
            log(lines, f'MISSING {clip}: {cfg["asset"]}'); bad += 1; continue
        try:
            fr = AL.get_frame_rate(anim) if hasattr(AL, 'get_frame_rate') else 30.0
        except Exception:
            fr = 30.0
        fps = float(fr.numerator) / float(fr.denominator) if hasattr(fr, 'numerator') else float(fr)
        n = int(AL.get_num_frames(anim)) + 1
        fbx = os.path.join(outdir, clip + '.fbx')
        task = unreal.AssetExportTask()
        task.object, task.filename, task.automated, task.replace_identical, task.prompt = anim, fbx, True, True, False
        task.exporter = unreal.AnimSequenceExporterFBX()
        opt = unreal.FbxExportOption()
        for k, v in (('export_preview_mesh', False), ('export_morph_targets', False), ('ascii', False)):
            try:
                opt.set_editor_property(k, v)
            except Exception:
                pass
        task.options = opt
        if not unreal.Exporter.run_asset_export_task(task):
            log(lines, f'EXPORT FAILED {clip}'); bad += 1; continue
        meta = {'asset': anim.get_path_name(), 'fps': fps, 'frames': n, 'length_s': float(AL.get_sequence_length(anim))}
        for k in ('rate_scale', 'enable_root_motion', 'root_motion_root_lock', 'force_root_lock'):
            try:
                v = anim.get_editor_property(k)
                meta[k] = v if isinstance(v, (bool, int, float)) else str(v)
            except Exception:
                pass
        try:
            meta['bone_tracks'] = len(AL.get_animation_track_names(anim))
        except Exception:
            pass
        meta['curves'] = curves(anim, fps, n)
        meta['notifies'] = notifies(anim)
        meta['sync_markers'] = markers(anim)
        json.dump(meta, open(os.path.join(outdir, clip + '.meta.json'), 'w'), indent=1)
        log(lines, f'OK {clip} {meta["asset"]} {n}f {fps:g}fps curves={len(meta["curves"])} notifies={len(meta["notifies"])} '
                   f'tracks={meta.get("bone_tracks")}')
        ok += 1
    open(os.path.join(outdir, 'export.log'), 'w').write('\n'.join(lines) + '\n')
    print(f'clipcheck export: {ok} ok, {bad} failed, log {os.path.join(outdir, "export.log")}')
    for s in [x for x in lines if not x.startswith('OK')][:4]:
        print(s)


main()
