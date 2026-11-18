# Read-only: lists the chooser tables the main Mover ABP depends on and dumps each one's columns and results to
# Saved/Logs/ci/chooser_dump.txt (run-start-stop-skid-1: plan the skid row before copying ABP and chooser).
import unreal

MAIN_ABP = '/Game/Blueprints/SandboxCharacter_Mover_ABP'
OUT = unreal.Paths.project_saved_dir() + 'Logs/ci/chooser_dump.txt'

reg = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
lines = []
deps = reg.get_dependencies(MAIN_ABP, opts) or []
for d in deps:
    d = str(d)
    data = reg.get_assets_by_package_name(d)
    for a in data:
        cls = str(a.asset_class_path.asset_name)
        if 'Chooser' in cls or 'PoseSearchDatabase' in cls:
            lines.append('DEP %s %s' % (cls, d))

for line in list(lines):
    if not line.startswith('DEP ChooserTable'):
        continue
    path = line.split(' ')[2]
    asset = unreal.load_asset(path)
    lines.append('==== ' + path)
    try:
        txt = unreal.EditorAssetLibrary.get_metadata_tag_values(asset)
    except Exception:
        txt = None
    for prop in ('result_type', 'output_object_type', 'context_data', 'columns_structs', 'result_structs', 'nested_choosers'):
        try:
            v = asset.get_editor_property(prop)
            lines.append('  %s: %s' % (prop, str(v)[:3000]))
        except Exception as e:
            lines.append('  %s: <%s>' % (prop, e))

with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
print('chooser dump: %d lines -> %s' % (len(lines), OUT))
