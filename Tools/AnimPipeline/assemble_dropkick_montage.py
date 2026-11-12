import unreal

path = "/Game/AnimPipeline/Rig/Overlord/Dropkick/AM_OverlordDropkick"
anim_startup = unreal.load_asset("/Game/AnimPipeline/Rig/Overlord/Dropkick/Startup/AS_OverlordDropkick_Startup")
anim_loop = unreal.load_asset("/Game/AnimPipeline/Rig/Overlord/Dropkick/Loop/AS_OverlordDropkick_Loop")
anim_land = unreal.load_asset("/Game/AnimPipeline/Rig/Overlord/Dropkick/Land/AS_OverlordDropkick_Land")

if unreal.EditorAssetLibrary.does_asset_exist(path):
    montage = unreal.load_asset(path)
    print("Loaded existing montage:", montage)
else:
    fac = unreal.AnimMontageFactory()
    fac.set_editor_property("source_animation", anim_startup)
    fac.set_editor_property("target_skeleton", anim_startup.get_editor_property("skeleton"))
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset("AM_OverlordDropkick", "/Game/AnimPipeline/Rig/Overlord/Dropkick", unreal.AnimMontage, fac)
    print("Created new montage:", montage)

len_startup = anim_startup.get_play_length()
len_loop = anim_loop.get_play_length()
len_land = anim_land.get_play_length()

print(f"Clip lengths: Startup={len_startup:.3f}s ({len_startup*60:.1f}f), Loop={len_loop:.3f}s ({len_loop*60:.1f}f), Land={len_land:.3f}s ({len_land*60:.1f}f)")

# Segment 0: Startup (16 frames)
seg0 = unreal.AnimSegment()
seg0.set_editor_property("anim_reference", anim_startup)
seg0.set_editor_property("start_pos", 0.0)
seg0.set_editor_property("anim_start_time", 0.0)
seg0.set_editor_property("anim_end_time", len_startup)
seg0.set_editor_property("anim_play_rate", 1.0)
seg0.set_editor_property("looping_count", 1)

# Segment 1: Loop (e.g. falling missile; plays 3 cycles = ~0.55s falling before ground)
loop_reps = 3
seg1 = unreal.AnimSegment()
seg1.set_editor_property("anim_reference", anim_loop)
seg1.set_editor_property("start_pos", len_startup)
seg1.set_editor_property("anim_start_time", 0.0)
seg1.set_editor_property("anim_end_time", len_loop)
seg1.set_editor_property("anim_play_rate", 1.0)
seg1.set_editor_property("looping_count", loop_reps)

# Segment 2: Landing (crash, bounce +16cm, settle downed)
seg2 = unreal.AnimSegment()
seg2.set_editor_property("anim_reference", anim_land)
seg2.set_editor_property("start_pos", len_startup + (len_loop * loop_reps))
seg2.set_editor_property("anim_start_time", 0.0)
seg2.set_editor_property("anim_end_time", len_land)
seg2.set_editor_property("anim_play_rate", 1.0)
seg2.set_editor_property("looping_count", 1)

anim_track = unreal.AnimTrack()
anim_track.set_editor_property("anim_segments", [seg0, seg1, seg2])

slot_track = unreal.SlotAnimationTrack()
slot_track.set_editor_property("slot_name", "DefaultSlot")
slot_track.set_editor_property("anim_track", anim_track)

montage.set_editor_property("slot_anim_tracks", [slot_track])

total_len = len_startup + (len_loop * loop_reps) + len_land
print(f"Successfully configured AM_OverlordDropkick! Total duration: {total_len:.3f}s ({total_len*60:.1f} frames)")

unreal.EditorAssetLibrary.save_asset(path)
print("Saved asset successfully.")
