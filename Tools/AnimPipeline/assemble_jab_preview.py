"""Assemble Overlord Jab Preview Montage (AM_OverlordJab) and Level Sequence (LS_OverlordJab_Preview).

Chains Hit 1 (AS_OverlordJab1 - backhand slap with left arm across torso)
and Hit 2 (AS_OverlordJab2 - forward lunge with villain claw slap and stare)
for review in Unreal Engine.
"""
import unreal

MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
DIR = "/Game/AnimPipeline/Rig/Overlord/Jab"
EAL = unreal.EditorAssetLibrary

def run():
    as_jab1_path = f"{DIR}/Jab1/AS_OverlordJab1"
    as_jab2_path = f"{DIR}/Jab2/AS_OverlordJab2"
    
    if not EAL.does_asset_exist(as_jab1_path) or not EAL.does_asset_exist(as_jab2_path):
        print(f"ERROR: Both {as_jab1_path} and {as_jab2_path} must exist before assembling preview.")
        return False
        
    anim1 = unreal.load_asset(as_jab1_path)
    anim2 = unreal.load_asset(as_jab2_path)
    skel = anim1.get_editor_property("skeleton")
    rate = unreal.FrameRate(60, 1)
    
    len1_sec = anim1.get_play_length()
    len2_sec = anim2.get_play_length()
    frames1 = int(round(len1_sec * 60))
    frames2 = int(round(len2_sec * 60))
    total_frames = frames1 + frames2
    print(f"Assembling Jab Preview: Jab1={frames1}f ({len1_sec:.3f}s), Jab2={frames2}f ({len2_sec:.3f}s), Total={total_frames}f")
    
    # --- 1. Level Sequence Preview ---
    ls_name = "LS_OverlordJab_Preview"
    ls_full = f"{DIR}/{ls_name}"
    if EAL.does_asset_exist(ls_full):
        EAL.delete_asset(ls_full)
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ls_name, DIR, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(rate)
    seq.set_playback_start(0)
    seq.set_playback_end(total_frames)
    
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0))
    mesh_asset = unreal.load_asset(MESH)
    if hasattr(actor.skeletal_mesh_component, "set_skinned_asset_and_update"):
        actor.skeletal_mesh_component.set_skinned_asset_and_update(mesh_asset)
    else:
        actor.skeletal_mesh_component.set_skeletal_mesh(mesh_asset)
        
    binding = seq.add_spawnable_from_instance(actor)
    actor.destroy_actor()
    
    tr = binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    
    # Section 1: Jab 1 (Backhand slap)
    sec1 = tr.add_section()
    sec1.set_range(0, frames1)
    p1 = sec1.get_editor_property("params")
    p1.set_editor_property("animation", anim1)
    sec1.set_editor_property("params", p1)
    
    # Section 2: Jab 2 (Villain claw slap & stare)
    sec2 = tr.add_section()
    sec2.set_range(frames1, total_frames)
    p2 = sec2.get_editor_property("params")
    p2.set_editor_property("animation", anim2)
    sec2.set_editor_property("params", p2)
    
    EAL.save_asset(ls_full)
    print(f"Created LevelSequence: {ls_full}")
    
    # Open in sequencer for live inspection
    LSE = unreal.LevelSequenceEditorBlueprintLibrary
    LSE.open_level_sequence(seq)
    LSE.refresh_current_level_sequence()
    LSE.set_current_time(0)
    
    # --- 2. AnimMontage ---
    am_name = "AM_OverlordJab"
    am_full = f"{DIR}/{am_name}"
    if EAL.does_asset_exist(am_full):
        EAL.delete_asset(am_full)
        
    fac = unreal.AnimMontageFactory()
    fac.set_editor_property("source_animation", anim1)
    fac.set_editor_property("target_skeleton", skel)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(am_name, DIR, unreal.AnimMontage, fac)
    
    EAL.save_asset(am_full)
    print(f"Created AnimMontage: {am_full}")
    return True

if __name__ == "__main__":
    run()
