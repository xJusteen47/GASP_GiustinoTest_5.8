"""Editor side of animpipe/samplecache.py: is the clip's package dirty (edited in memory, not saved)? Then its .uasset on disk
does not describe what the editor would sample, and the cache must not be used for it. Called once per process and clip."""
import unreal


def dirty(asset):
    pkg = asset.split(".", 1)[0]
    names = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    return "dirty" if pkg in names else "clean"
