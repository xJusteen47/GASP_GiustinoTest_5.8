import unreal

def run():
    idle_path = "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop"
    rig_path = "/Game/Characters/UE5_Mannequins/Rigs/CR_Mannequin_Body"
    mesh_path = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
    
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset("LS_TempIdleProbe", "/Engine/Transient", unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    rate = unreal.FrameRate(60, 1)
    seq.set_display_rate(rate)
    seq.set_playback_start(0); seq.set_playback_end(1)
    
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0,0,0))
    actor.skeletal_mesh_component.set_skinned_asset_and_update(unreal.load_asset(mesh_path))
    binding = seq.add_spawnable_from_instance(actor)
    actor.destroy_actor()
    
    tr = binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    sec = tr.add_section()
    sec.set_range(0, 2)
    p = sec.get_editor_property("params")
    p.set_editor_property("animation", unreal.load_asset(idle_path))
    sec.set_editor_property("params", p)
    
    unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)
    unreal.LevelSequenceEditorBlueprintLibrary.refresh_current_level_sequence()
    unreal.LevelSequenceEditorBlueprintLibrary.set_current_time(0)
    
    rig_cls = unreal.load_asset(rig_path)
    if hasattr(rig_cls, "generated_class"):
        rig_cls = rig_cls.generated_class()
    unreal.ControlRigSequencerLibrary.bake_to_control_rig(unreal.EditorLevelLibrary.get_editor_world(), seq, rig_cls, unreal.AnimSeqExportOption(), False, 0.001, binding)
    
    for t in binding.get_tracks():
        if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
            rig_sec = t.get_sections()[0]
            vals = {}
            for c in rig_sec.get_all_channels():
                cn = str(c.channel_name)
                keys = c.get_keys()
                if keys:
                    v = keys[0].get_value()
                    if abs(v) > 0.01:
                        vals[cn] = round(v, 2)
            unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
            unreal.EditorAssetLibrary.delete_asset("/Engine/Transient/LS_TempIdleProbe")
            return vals
    return {}

if __name__ == "__main__":
    run()
