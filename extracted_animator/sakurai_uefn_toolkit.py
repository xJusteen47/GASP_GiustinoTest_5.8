import unreal
import math
import json
import os

# ==============================================================================
# SAKURAI UEFN TOOLKIT - 3D COMBAT ANIMATION
# ==============================================================================

def dump_rig_controls():
    seq = unreal.LevelSequenceEditorBlueprintLibrary.get_current_level_sequence()
    rig_proxies = unreal.ControlRigSequencerLibrary.get_control_rigs(seq)
    if not rig_proxies: return None
    rig = rig_proxies[0].control_rig
    controls = [str(k.name) for k in rig.get_hierarchy().get_controls()]
    out_path = os.path.join(unreal.Paths.project_saved_dir(), "uefn_rig_manifest.json")
    with open(out_path, "w") as f:
        json.dump({"rig_name": rig.get_name(), "controls": controls}, f, indent=2)
    unreal.log(f"[SakuraiToolkit] Manifest UEFN salvato: {out_path}")
    return controls

def apply_sakurai_sparse_animation(pose_dictionary):
    seq = unreal.LevelSequenceEditorBlueprintLibrary.get_current_level_sequence()
    rig_proxies = unreal.ControlRigSequencerLibrary.get_control_rigs(seq)
    if not rig_proxies: return
    rig_proxy = rig_proxies[0]
    rig = rig_proxy.control_rig

    with unreal.ScopedEditorTransaction("Sakurai Sparse Animation") as trans:
        for frame_num, frame_data in sorted(pose_dictionary.items()):
            f_num = unreal.FrameNumber(frame_num)
            for ctrl_name, xform in frame_data["controls"].items():
                loc = unreal.Vector(*xform.get("loc", [0.0, 0.0, 0.0]))
                rot = unreal.Rotator(*xform.get("rot", [0.0, 0.0, 0.0]))
                transform = unreal.Transform(loc, rot, [1.0, 1.0, 1.0])
                unreal.ControlRigSequencerLibrary.set_local_control_rig_transform(
                    seq, rig, ctrl_name, f_num, transform, set_key=True
                )

        track = rig_proxy.track
        if track and track.get_sections():
            section = track.get_sections()[0]
            for channel in section.get_all_channels():
                if isinstance(channel, unreal.MovieSceneScriptingFloatChannel):
                    for key in channel.get_keys():
                        key_frame = key.get_time().frame_number.value
                        if key_frame in pose_dictionary:
                            interp_str = pose_dictionary[key_frame].get("interp", "CUBIC")
                            if interp_str == "CONSTANT":
                                key.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_CONSTANT)
                            elif interp_str == "LINEAR":
                                key.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_LINEAR)
                            else:
                                key.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_CUBIC)
                                key.set_tangent_mode(unreal.RichCurveTangentMode.RCTM_AUTO)

    unreal.log("[SakuraiToolkit] Animazione e tangenti applicate.")

def build_uefn_spine_curve(total_roll, total_pitch, total_yaw):
    weights = {"spine_01_ctrl": 0.10, "spine_02_ctrl": 0.15, "spine_03_ctrl": 0.20, "spine_04_ctrl": 0.25, "spine_05_ctrl": 0.30}
    return {ctrl: {"rot": [total_roll * w, total_pitch * w, total_yaw * w]} for ctrl, w in weights.items()}

def audit_animation_3d_readability(anticipation_frame, hit_frame, attacking_hand="hand_r_fk_ctrl", off_hand="hand_l_fk_ctrl"):
    seq = unreal.LevelSequenceEditorBlueprintLibrary.get_current_level_sequence()
    rig = unreal.ControlRigSequencerLibrary.get_control_rigs(seq)[0].control_rig

    f_idle = unreal.FrameNumber(0)
    f_ant = unreal.FrameNumber(anticipation_frame)
    f_hit = unreal.FrameNumber(hit_frame)

    pelvis_idle = unreal.ControlRigSequencerLibrary.get_world_control_rig_transform(seq, rig, "pelvis_ctrl", f_idle).translation
    pelvis_ant = unreal.ControlRigSequencerLibrary.get_world_control_rig_transform(seq, rig, "pelvis_ctrl", f_ant).translation
    pelvis_hit = unreal.ControlRigSequencerLibrary.get_world_control_rig_transform(seq, rig, "pelvis_ctrl", f_hit).translation
    
    hand_atk = unreal.ControlRigSequencerLibrary.get_world_control_rig_transform(seq, rig, attacking_hand, f_hit).translation
    hand_off = unreal.ControlRigSequencerLibrary.get_world_control_rig_transform(seq, rig, off_hand, f_hit).translation

    reach_3d = math.sqrt((hand_atk.x - pelvis_hit.x)**2 + (hand_atk.y - pelvis_hit.y)**2 + (hand_atk.z - pelvis_hit.z)**2)
    hand_sep_3d = math.sqrt((hand_atk.x - hand_off.x)**2 + (hand_atk.y - hand_off.y)**2 + (hand_atk.z - hand_off.z)**2)
    depth_offset = abs(hand_atk.y - pelvis_hit.y)
    pelvis_drop_z = pelvis_ant.z - pelvis_idle.z

    unreal.log("================ SAKURAI 3D POSE AUDIT ================")
    if hand_atk.z < 0.0: unreal.log_error(f"[CLIPPING] Mano sotto il pavimento (Z={hand_atk.z:.1f})")
    if pelvis_drop_z > -8.0: unreal.log_warning(f"[PESO DEBOLE] Bacino sceso di soli {pelvis_drop_z:.1f} cm.")
    if reach_3d < 58.0: unreal.log_warning(f"[T-REX ARM] Reach 3D troppo corto ({reach_3d:.1f} cm).")
    if depth_offset < 15.0: unreal.log_warning(f"[PIATTO] Manca profondita 3D ({depth_offset:.1f} cm).")
    if hand_sep_3d < 75.0: unreal.log_warning(f"[LINE OF ACTION CHIUSA] Mani troppo vicine ({hand_sep_3d:.1f} cm).")
    unreal.log("=======================================================")
