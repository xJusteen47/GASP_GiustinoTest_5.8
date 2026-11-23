# Read-only: float curve names of a few clips (env CLIP_NAMES, comma separated asset names) -> Saved/Routing/clip_curves.txt
import os
import unreal

names = [n.strip() for n in os.environ.get('CLIP_NAMES', '').split(',') if n.strip()]
reg = unreal.AssetRegistryHelpers.get_asset_registry()
out = []
for n in names:
    hits = [d for d in reg.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine', 'AnimSequence')) if str(d.asset_name) == n]
    if not hits:
        out.append(f'{n}: not found')
        continue
    seq = hits[0].get_asset()
    curves = unreal.AnimationLibrary.get_animation_curve_names(seq, unreal.RawCurveTrackTypes.RCT_FLOAT)
    try:
        notifies = [str(e.notify.get_class().get_name()) if e.notify else str(e.notify_name) for e in unreal.AnimationLibrary.get_animation_notify_events(seq)]
    except Exception as ex:  # noqa: BLE001
        notifies = [repr(ex)[:80]]
    out.append(f'{n} ({hits[0].package_name}): {len(curves)} curves: ' + ', '.join(sorted(str(c) for c in curves)))
    out.append(f'  notifies: ' + ', '.join(notifies))
p = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Routing', 'clip_curves.txt')
os.makedirs(os.path.dirname(p), exist_ok=True)
open(p, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
