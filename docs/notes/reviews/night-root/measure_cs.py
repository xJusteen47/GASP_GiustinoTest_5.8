# NIGHT-ROOT: component-space xy of pelvis and feet (root * chain) at the start and end of each pipeline clip, and of a GASP idle
import unreal
OUT = r"C:/Users/jacop/Documents/Unreal Projects/GASP_Giustino2_ue59/Saved/nightroot/cs.txt"
CH = {'pelvis': ['root','pelvis'], 'foot_l': ['root','pelvis','thigh_l','calf_l','foot_l'], 'foot_r': ['root','pelvis','thigh_r','calf_r','foot_r']}
def cs(seq, t, chain):
    x = unreal.Transform()
    for b in chain:
        x = unreal.MathLibrary.compose_transforms(unreal.AnimationLibrary.get_bone_pose_for_time(seq, b, t, False), x)
    return x.translation
reg = unreal.AssetRegistryHelpers.get_asset_registry()
paths = [str(a.package_name) for a in reg.get_assets_by_path('/Game/GASNPPTest/Combat', recursive=True) if str(a.asset_class_path.asset_name) == 'AnimSequence']
for a in reg.get_assets_by_path('/Game/Characters/UEFN_Mannequin/Animations', recursive=True):
    n = str(a.package_name)
    if str(a.asset_class_path.asset_name) == 'AnimSequence' and 'Idle' in n and len(paths) < 40:
        paths.append(n)
lines = []
for p in sorted(paths):
    s = unreal.load_asset(p); L = s.get_play_length()
    row = p.split('/')[-1]
    for t in (0.0, L):
        row += f" | t={t:.2f} " + ' '.join(f"{k}({v.x:.1f},{v.y:.1f})" for k, v in ((k, cs(s, t, c)) for k, c in CH.items()))
    lines.append(row)
open(OUT, 'w').write('\n'.join(lines) + '\n')
