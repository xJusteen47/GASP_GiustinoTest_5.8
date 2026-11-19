"""Apply the generic clip fixes to the editor assets of clips.json rows that list "fix" (headless UE, editor CLOSED on that checkout).

    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -nullrhi      (no args: paths with spaces)

Per row with "asset" and "fix" (a list):
  "filters"  every bone track through clip_filters.clean_track (deflick + ease_snaps = exactly the pops clip_check fails)
  "ik_feet"  ik_foot_l/r re-baked from the feet on every frame (GASP leg IK puts the feet on these bones; a stale or popping
             ik bone shows in game even when the FK leg is clean). Same code as the animpipe export (ue_rig._bake_ik_feet).
Only the changed bone tracks are rewritten (translation and scale keys kept); curves, notifies, sync markers and settings are
untouched. Saves the asset. Report: Saved/ClipCheck/fix_report.json; prints max 5 lines. Re-export + clip_check to verify.
Default = DRY RUN (report only, nothing saved). CLIPFIX_WRITE=1 writes and saves. CLIPFIX_ONLY="RowA,RowB" limits the rows.
"""
import importlib.util, json, os, sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import clip_filters  # noqa: E402

AL, EAL = unreal.AnimationLibrary, unreal.EditorAssetLibrary


def _ue_rig():
    p = os.path.join(HERE, '..', 'animpipe', 'ue_rig.py')
    spec = importlib.util.spec_from_file_location('ue_rig_clipfix', p)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    return m


def resolve(asset):
    if asset.startswith('/'):
        return unreal.load_asset(asset)
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence')], package_paths=['/Game'],
                          recursive_paths=True)
    hits = [a for a in ar.get_assets(flt) if str(a.asset_name) == asset]
    hits.sort(key=lambda a: (not str(a.package_path).startswith('/Game/GSTN'), str(a.package_path)))
    return hits[0].get_asset() if hits else None


def q2t(q):
    return (q.x, q.y, q.z, q.w)


def filters(anim, dry):
    n = int(anim.get_editor_property('number_of_sampled_keys'))
    fps = (n - 1) / max(anim.get_play_length(), 1e-6)
    ctrl = anim.get_editor_property('controller')
    rep = {}
    if not dry:
        ctrl.open_bracket(unreal.Text('clipfix filters'), False)
    for b in [str(x) for x in AL.get_animation_track_names(anim)]:
        tfs = [AL.get_bone_pose_for_frame(anim, b, f, False) for f in range(n)]
        new, r = clip_filters.clean_track([q2t(t.rotation) for t in tfs], fps)
        if r is None:
            continue
        rep[b] = r
        if not dry:
            ctrl.set_bone_track_keys(b, [t.translation for t in tfs], [unreal.Quat(*q) for q in new],
                                     [t.scale3d for t in tfs], False)
    if not dry:
        ctrl.close_bracket(False)
    return {'fps': round(fps, 2), 'frames': n, 'bones': rep}


def main():
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(['/Game'], True)
    C = json.load(open(os.path.join(HERE, 'clips.json')))['clips']
    only = [x for x in os.environ.get('CLIPFIX_ONLY', '').split(',') if x]
    dry = os.environ.get('CLIPFIX_WRITE') != '1'
    rig = None
    report, lines = {}, []
    for clip, cfg in C.items():
        if clip.startswith('_') or 'asset' not in cfg or not cfg.get('fix') or (only and clip not in only):
            continue
        anim = resolve(cfg['asset'])
        if anim is None:
            lines.append(f'MISSING {clip}'); continue
        r = {'asset': anim.get_path_name(), 'dry': dry}
        if 'filters' in cfg['fix']:
            r['filters'] = filters(anim, dry)
        if 'ik_feet' in cfg['fix'] and not dry:
            rig = rig or _ue_rig()
            r['ik_feet'] = rig._bake_ik_feet(anim)
        if not dry:
            r['saved'] = bool(EAL.save_asset(anim.get_path_name().split('.')[0], only_if_is_dirty=False))
        report[clip] = r
        nb = len(r.get('filters', {}).get('bones', {}))
        lines.append(f'{clip}: filters {nb} bones, ik_feet {"ik_feet" in r}, saved {r.get("saved")}')
    out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'ClipCheck', 'fix_report.json')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    json.dump(report, open(out, 'w'), indent=1, default=str)
    print(f'clipfix: {len(report)} clips, report {out}')
    for s in lines[:4]:
        print(s)


main()
