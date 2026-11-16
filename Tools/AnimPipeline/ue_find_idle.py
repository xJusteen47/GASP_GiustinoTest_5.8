"""Editor-side, read-only: list AnimSequences under /Game whose name contains 'idle' (the project's idle clips)."""
import unreal, json
def main():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    f = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "AnimSequence")], recursive_paths=True, package_paths=["/Game"])
    out = []
    for a in ar.get_assets(f):
        n = str(a.asset_name)
        if "idle" in n.lower():
            out.append(str(a.package_name))
    return json.dumps(sorted(out)[:80])
