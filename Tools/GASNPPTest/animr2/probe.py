import unreal
p="/Game/AnimPipeline/Rig/Overlord/Dropkick/AM_OverlordDropkick"
unreal.EditorAssetLibrary.save_asset(p)
pk = unreal.EditorAssetLibrary.load_asset(p).get_outermost()
unreal.EditorLoadingAndSavingUtils.reload_packages([pk], unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
m = unreal.load_asset(p)
print('after reload', m.get_play_length(), m.get_num_sections() if hasattr(m,'get_num_sections') else '')
