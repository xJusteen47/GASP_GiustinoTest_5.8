# Read-only probe (skid-walkturn-1): which IK Rig / Retargeter Python API UE 5.9 exposes, and which UEFN rigs exist.
# Writes Saved/Mixamo/probe.txt. Saves nothing.
import os
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), 'Mixamo', 'probe.txt')
os.makedirs(os.path.dirname(OUT), exist_ok=True)
f = open(OUT, 'w', encoding='utf-8')
for cls in ('IKRigController', 'IKRetargeterController', 'IKRetargetBatchOperation', 'IKRigDefinition', 'IKRetargeter',
            'RetargetChainSettings', 'AutoCharacterizeResults'):
    c = getattr(unreal, cls, None)
    f.write(f'== {cls}: {"MISSING" if c is None else ""}\n')
    if c is not None:
        f.write(', '.join(n for n in dir(c) if not n.startswith('_')) + '\n')
f.write('== classes with Retarget/IKRig in name\n')
f.write(', '.join(n for n in dir(unreal) if 'Retarget' in n or 'IKRig' in n) + '\n')
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Game/Characters/UEFN_Mannequin'], True)
for a in ar.get_assets_by_path('/Game/Characters/UEFN_Mannequin', recursive=True):
    cn = str(a.asset_class_path.asset_name)
    if cn in ('IKRigDefinition', 'IKRetargeter', 'SkeletalMesh', 'Skeleton'):
        f.write(f'asset {cn} {a.package_name}\n')
f.close()
print('ESITO: OK probe')
