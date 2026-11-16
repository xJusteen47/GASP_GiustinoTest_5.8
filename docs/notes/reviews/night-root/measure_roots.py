# NIGHT-ROOT: root bone of every anim sequence under the combat folders (reaction + moves), no root motion extraction.
# Writes Saved/nightroot/roots.txt: name, root motion flags, root translation at t0 / min / max (xy, z), yaw range.
import math
import unreal

OUT = r"C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59/Saved/nightroot/roots.txt"
ROOTS = ['/Game/GASNPPTest/Combat', '/Game/Characters']
lines = []
reg = unreal.AssetRegistryHelpers.get_asset_registry()
seen = set()
for base in ROOTS:
    for ad in reg.get_assets_by_path(base, recursive=True):
        if str(ad.asset_class_path.asset_name) != 'AnimSequence':
            continue
        path = str(ad.package_name)
        if base == '/Game/Characters' and 'React' not in path and 'Combat' not in path:
            continue
        if path in seen:
            continue
        seen.add(path)
        try:
            seq = unreal.load_asset(path)
            length = seq.get_play_length()
            n = max(2, int(length * 30) + 1)
            xs = []
            for i in range(n):
                t = min(length, i / 30.0)
                tr = unreal.AnimationLibrary.get_bone_pose_for_time(seq, 'root', t, False)
                loc = tr.translation
                yaw = tr.rotation.rotator().yaw
                xs.append((math.hypot(loc.x, loc.y), loc.z, yaw, loc.x, loc.y))
            h = [a[0] for a in xs]
            z = [a[1] for a in xs]
            y = [a[2] for a in xs]
            lines.append(f"{path} rm={seq.get_editor_property('enable_root_motion')} lock={seq.get_editor_property('force_root_lock')} "
                         f"len={length:.2f} xy0=({xs[0][3]:.1f},{xs[0][4]:.1f}) xyEnd=({xs[-1][3]:.1f},{xs[-1][4]:.1f}) h[min {min(h):.1f} max {max(h):.1f}] "
                         f"z[{min(z):.1f},{max(z):.1f}] yaw[{min(y):.1f},{max(y):.1f}]")
        except Exception as e:
            lines.append(f"{path} ERROR {e}")
with open(OUT, 'w') as f:
    f.write('\n'.join(sorted(lines)) + '\n')
unreal.log(f"NIGHTROOT wrote {len(lines)} rows")
