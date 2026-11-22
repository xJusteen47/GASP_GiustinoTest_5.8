import unreal, json, os, math

MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
POS = unreal.Vector(0, 1000, 0)
CENTER = unreal.Vector(0, 1000, 95)
OUT = "C:/Users/jacop/.gemini/antigravity/worktrees/GASP_Giustino2_feature/animate_3d_moves_unreal/Tools/AnimPipeline/sheets/raw/"

def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

def _actor(label):
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None

def set_channel_key(channel, frame, val):
    fn = unreal.FrameNumber(int(frame))
    for k in channel.get_keys():
        if k.get_time().frame_number.value == int(frame):
            k.set_value(float(val))
            return
    # Not found, add it
    channel.add_key(fn, float(val), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE)

def author_move(seq_path, anim_path, poses, total_frames, **kwargs):
    seq = unreal.load_asset(seq_path)
    LSE = unreal.LevelSequenceEditorBlueprintLibrary
    LSE.open_level_sequence(seq)
    
    sec = None
    binding = seq.get_bindings()[0]
    for t in binding.get_tracks():
        if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
            sec = t.get_sections()[0]
            break
            
    if not sec:
        return {"error": "no rig section found"}
        
    by = {str(c.channel_name): c for c in sec.get_all_channels() if isinstance(c, unreal.MovieSceneScriptingFloatChannel)}
    axes = ["X", "Y", "Z"]
    
    # 1. Clear keys outside of defined frames for all modified controls
    target_frames = [int(f) for f in poses.keys()]
    
    # Apply keys for each frame
    modified_controls = set()
    for f_str, frame_data in sorted(poses.items(), key=lambda x: int(x[0])):
        frame = int(f_str)
        for ctrl, xforms in frame_data.items():
            modified_controls.add(ctrl)
            if "rot" in xforms:
                for i, ax in enumerate(axes):
                    c = by.get(f"{ctrl}.Rotation.{ax}")
                    if c:
                        set_channel_key(c, frame, xforms["rot"][i])
            if "loc" in xforms:
                for i, ax in enumerate(axes):
                    c = by.get(f"{ctrl}.Location.{ax}")
                    if c:
                        set_channel_key(c, frame, xforms["loc"][i])
                        
    # For all modified controls, remove keys not in target_frames
    for ctrl in modified_controls:
        for attr in ["Rotation", "Location"]:
            for ax in axes:
                c = by.get(f"{ctrl}.{attr}.{ax}")
                if c:
                    for k in list(c.get_keys()):
                        if k.get_time().frame_number.value not in target_frames:
                            c.remove_key(k)
                    # Set smooth cubic auto interpolation
                    for k in c.get_keys():
                        k.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_CUBIC)
                        k.set_tangent_mode(unreal.RichCurveTangentMode.RCTM_SMART_AUTO)

    LSE.refresh_current_level_sequence()
    seq.set_playback_start(0)
    seq.set_playback_end(int(total_frames))
    
    # Bake to AnimSequence
    dst = unreal.load_asset(anim_path)
    eo = unreal.AnimSeqExportOption()
    rate = seq.get_display_rate()
    eo.set_editor_property("use_custom_frame_rate", True)
    eo.set_editor_property("custom_frame_rate", rate)
    ok = unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(dst, eo, binding, False)
    
    return {
        "status": "ok",
        "baked": ok,
        "frames": total_frames,
        "target_frames": target_frames,
        "controls": sorted(list(modified_controls))
    }

def capture_poses(anim_path, frames, tag_prefix, **kwargs):
    char = _actor("AnimReviewChar")
    if not char:
        eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        char = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, POS, unreal.Rotator(0, 0, 0))
        char.set_actor_label("AnimReviewChar")
    char.set_actor_location(POS, False, False)
    c = char.skeletal_mesh_component
    mesh_asset = unreal.load_asset(MESH)
    c.set_skinned_asset_and_update(mesh_asset) if hasattr(c, "set_skinned_asset_and_update") else c.set_skeletal_mesh(mesh_asset)
    c.set_update_animation_in_editor(True)
    c.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    
    dst = unreal.load_asset(anim_path)
    c.set_animation(dst)
    
    cam = _actor("AnimReviewCam")
    if not cam:
        eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        cam = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 1300, 100), unreal.Rotator(0, -90, 0))
        cam.set_actor_label("AnimReviewCam")
        
    sc = cam.capture_component2d
    sc.set_editor_property("capture_every_frame", False)
    sc.set_editor_property("capture_on_movement", False)
    sc.set_editor_property("projection_type", unreal.CameraProjectionMode.PERSPECTIVE)
    sc.set_editor_property("fov_angle", 40.0)
    sc.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    
    os.makedirs(OUT, exist_ok=True)
    results = []
    
    for f in frames:
        c.override_animation_data(dst, False, False, f / 60.0, 0.0)
        c.set_position(f / 60.0, False)
        
        for az in [0, 315]:
            r = math.radians(az); e = math.radians(15)
            loc = unreal.Vector(CENTER.x + 280 * math.cos(e) * math.sin(r),
                                CENTER.y + 280 * math.cos(e) * math.cos(r),
                                CENTER.z + 280 * math.sin(e))
            cam.set_actor_location_and_rotation(loc, unreal.MathLibrary.find_look_at_rotation(loc, CENTER), False, True)
            rt = unreal.RenderingLibrary.create_render_target2d(_world(), 512, 512, unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(0,0,0,0))
            sc.set_editor_property("texture_target", rt)
            sc.capture_scene()
            px = unreal.RenderingLibrary.read_render_target(_world(), rt, False)
            buf = bytearray()
            for p in px:
                buf += bytes((p.r, p.g, p.b))
            out_path = os.path.join(OUT, f"{tag_prefix}_f{f}_az{az}.rgb")
            with open(out_path, "wb") as file:
                file.write(buf)
            results.append({"frame": f, "az": az, "path": out_path})
            
    return {"status": "ok", "captured": results}
def dump_keys(**kwargs):
    import unreal, json
    seq = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1')
    sec = None
    for t in seq.get_bindings()[0].get_tracks():
        if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
            sec = t.get_sections()[0]
            break
    res = {}
    for c in sec.get_all_channels():
        name = str(c.channel_name)
        if any(p in name for p in ['upperarm_l_fk_ctrl.Rotation', 'lowerarm_l_fk_ctrl.Rotation', 'upperarm_r_fk_ctrl.Rotation']):
            res[name] = [(k.get_time().frame_number.value, round(k.get_value(), 2)) for k in c.get_keys()]
    return json.dumps(res, indent=2)
def check_anim_asset(**kwargs):
    import unreal, json
    dst = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/AS_OverlordJab1')
    n = dst.get_editor_property('number_of_sampled_frames') if hasattr(dst, 'number_of_sampled_frames') else None
    length = dst.get_play_length()
    return json.dumps({'length': length, 'sampled_frames': n}, indent=2)
def dump_clavicles(**kwargs):
    import unreal, json
    seq = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1')
    sec = seq.get_bindings()[0].get_tracks()[0].get_sections()[0]
    res = {}
    for c in sec.get_all_channels():
        name = str(c.channel_name)
        if 'clavicle' in name:
            res[name] = [(k.get_time().frame_number.value, round(k.get_value(), 2)) for k in c.get_keys()]
    return json.dumps(res, indent=2)
def dump_arm_channels(**kwargs):
    import unreal, json
    seq = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1')
    sec = seq.get_bindings()[0].get_tracks()[0].get_sections()[0]
    names = []
    for c in sec.get_all_channels():
        name = str(c.channel_name)
        if any(w in name.lower() for w in ['clavicle', 'shoulder', 'local', 'stretch', 'tan', 'offset']):
            names.append(name)
    return json.dumps(names[:50], indent=2)
def dump_all_keyed_channels(**kwargs):
    import unreal, json
    seq = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1')
    sec = seq.get_bindings()[0].get_tracks()[0].get_sections()[0]
    res = []
    for c in sec.get_all_channels():
        keys = c.get_keys()
        if len(keys) > 0:
            res.append(str(c.channel_name))
    return json.dumps(res, indent=2)
def probe_axes(**kwargs):
    import unreal, json, os, math
    seq = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1')
    LSE = unreal.LevelSequenceEditorBlueprintLibrary
    LSE.open_level_sequence(seq)
    sec = seq.get_bindings()[0].get_tracks()[0].get_sections()[0]
    by = {str(c.channel_name): c for c in sec.get_all_channels() if isinstance(c, unreal.MovieSceneScriptingFloatChannel)}
    
    char = _actor("AnimReviewChar")
    char.set_actor_location(POS, False, False)
    c = char.skeletal_mesh_component
    cam = _actor("AnimReviewCam")
    sc = cam.capture_component2d
    
    probes = {
        "rest": [0, 0, 0],
        "X_plus45": [45, 0, 0],
        "Y_plus45": [0, 45, 0],
        "Z_plus45": [0, 0, 45],
        "Z_minus45": [0, 0, -45]
    }
    
    dst = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/AS_OverlordJab1')
    eo = unreal.AnimSeqExportOption()
    eo.set_editor_property("use_custom_frame_rate", True)
    eo.set_editor_property("custom_frame_rate", seq.get_display_rate())
    binding = seq.get_bindings()[0]
    
    out_files = {}
    for name, rot in probes.items():
        for i, ax in enumerate(["X", "Y", "Z"]):
            ch = by.get(f"upperarm_r_fk_ctrl.Rotation.{ax}")
            if ch:
                for k in ch.get_keys():
                    if k.get_time().frame_number.value == 0:
                        k.set_value(float(rot[i]))
                        break
        # zero out other arm controls at frame 0 for clean isolate
        for ctrl in ["lowerarm_r_fk_ctrl", "hand_r_fk_ctrl", "upperarm_l_fk_ctrl", "lowerarm_l_fk_ctrl", "hand_l_fk_ctrl"]:
            for ax in ["X", "Y", "Z"]:
                ch = by.get(f"{ctrl}.Rotation.{ax}")
                if ch:
                    for k in ch.get_keys():
                        if k.get_time().frame_number.value == 0:
                            k.set_value(0.0)
                            break
        unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(dst, eo, binding, False)
        c.set_animation(dst)
        c.override_animation_data(dst, False, False, 0.0, 0.0)
        c.set_position(0.0, False)
        
        # Capture from 315
        r = math.radians(315); e = math.radians(15)
        loc = unreal.Vector(CENTER.x + 280 * math.cos(e) * math.sin(r),
                            CENTER.y + 280 * math.cos(e) * math.cos(r),
                            CENTER.z + 280 * math.sin(e))
        cam.set_actor_location_and_rotation(loc, unreal.MathLibrary.find_look_at_rotation(loc, CENTER), False, True)
        rt = unreal.RenderingLibrary.create_render_target2d(_world(), 512, 512, unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(0,0,0,0))
        sc.set_editor_property("texture_target", rt)
        sc.capture_scene()
        px = unreal.RenderingLibrary.read_render_target(_world(), rt, False)
        buf = bytearray()
        for p in px:
            buf += bytes((p.r, p.g, p.b))
        out_path = os.path.join(OUT, f"probe_{name}.rgb")
        with open(out_path, "wb") as file:
            file.write(buf)
        out_files[name] = out_path
        
    return json.dumps(out_files, indent=2)
def test_refresh_anim(**kwargs):
    import unreal
    char = _actor("AnimReviewChar")
    c = char.skeletal_mesh_component
    dst = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/AS_OverlordJab1')
    c.set_animation(None)
    c.init_anim(True)
    c.set_animation(dst)
    c.init_anim(True)
    return "re-initialized"
def check_sequencer_spawnable(**kwargs):
    import unreal, json
    seq = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1')
    LSE = unreal.LevelSequenceEditorBlueprintLibrary
    LSE.open_level_sequence(seq)
    LSE.set_current_time(4)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = [a.get_actor_label() for a in eas.get_all_level_actors()]
    mannequins = [a for a in actors if 'mannequin' in a.lower() or 'skm' in a.lower()]
    return json.dumps({'current_time': 4, 'mannequins': mannequins}, indent=2)
def find_all_anims_in_game(**kwargs):
    import unreal, json
    eal = unreal.EditorAssetLibrary
    assets = eal.list_assets('/Game', recursive=True, include_folder=False)
    anims = []
    for a in assets:
        ad = eal.find_asset_data(a)
        if ad.asset_class_path.asset_name == 'AnimSequence':
            anims.append(str(a))
    return json.dumps(anims, indent=2)
def test_slap_options(**kwargs):
    import unreal, json, os, math
    seq = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1')
    LSE = unreal.LevelSequenceEditorBlueprintLibrary
    LSE.open_level_sequence(seq)
    sec = seq.get_bindings()[0].get_tracks()[0].get_sections()[0]
    by = {str(c.channel_name): c for c in sec.get_all_channels() if isinstance(c, unreal.MovieSceneScriptingFloatChannel)}
    
    char = _actor("AnimReviewChar")
    char.set_actor_location(POS, False, False)
    c = char.skeletal_mesh_component
    cam = _actor("AnimReviewCam")
    sc = cam.capture_component2d
    
    options = {
        "optA": {"up": [-20.0, -40.0, -10.0], "low": [0.0, 15.0, 0.0], "hand": [10.0, 0.0, -30.0]},
        "optB": {"up": [0.0, -65.0, 15.0], "low": [0.0, 15.0, 0.0], "hand": [10.0, 0.0, -30.0]},
        "optC": {"up": [20.0, -50.0, 35.0], "low": [0.0, 15.0, 0.0], "hand": [10.0, 0.0, -30.0]},
        "optD": {"up": [-35.0, -20.0, -40.0], "low": [0.0, 20.0, 0.0], "hand": [10.0, 0.0, -30.0]},
    }
    
    dst = unreal.load_asset('/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/AS_OverlordJab1')
    eo = unreal.AnimSeqExportOption()
    eo.set_editor_property("use_custom_frame_rate", True)
    eo.set_editor_property("custom_frame_rate", seq.get_display_rate())
    binding = seq.get_bindings()[0]
    
    out_files = {}
    for name, data in options.items():
        for i, ax in enumerate(["X", "Y", "Z"]):
            c_up = by.get(f"upperarm_r_fk_ctrl.Rotation.{ax}")
            if c_up:
                for k in c_up.get_keys():
                    if k.get_time().frame_number.value == 4:
                        k.set_value(float(data["up"][i]))
                        break
            c_low = by.get(f"lowerarm_r_fk_ctrl.Rotation.{ax}")
            if c_low:
                for k in c_low.get_keys():
                    if k.get_time().frame_number.value == 4:
                        k.set_value(float(data["low"][i]))
                        break
            c_h = by.get(f"hand_r_fk_ctrl.Rotation.{ax}")
            if c_h:
                for k in c_h.get_keys():
                    if k.get_time().frame_number.value == 4:
                        k.set_value(float(data["hand"][i]))
                        break
        unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(dst, eo, binding, False)
        c.set_animation(dst)
        c.override_animation_data(dst, False, False, 4.0 / 60.0, 0.0)
        c.set_position(4.0 / 60.0, False)
        
        # Capture from 315 (3/4 view) and 0 (front view)
        for az in [0, 315]:
            r = math.radians(az); e = math.radians(15)
            loc = unreal.Vector(CENTER.x + 280 * math.cos(e) * math.sin(r),
                                CENTER.y + 280 * math.cos(e) * math.cos(r),
                                CENTER.z + 280 * math.sin(e))
            cam.set_actor_location_and_rotation(loc, unreal.MathLibrary.find_look_at_rotation(loc, CENTER), False, True)
            rt = unreal.RenderingLibrary.create_render_target2d(_world(), 512, 512, unreal.TextureRenderTargetFormat.RTF_RGBA8, unreal.LinearColor(0,0,0,0))
            sc.set_editor_property("texture_target", rt)
            sc.capture_scene()
            px = unreal.RenderingLibrary.read_render_target(_world(), rt, False)
            buf = bytearray()
            for p in px:
                buf += bytes((p.r, p.g, p.b))
            out_path = os.path.join(OUT, f"slap_{name}_az{az}.rgb")
            with open(out_path, "wb") as file:
                file.write(buf)
            out_files[f"{name}_az{az}"] = out_path
            
    return json.dumps(out_files, indent=2)
def inspect_idle_base(**kwargs):
    import unreal, json, os, sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from animpipe import ue_rig
    rate = unreal.FrameRate(60, 1)
    anim_path = "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop"
    seq, binding, end, length = ue_rig._sequence(anim_path, "Idle_Inspect", rate)
    unreal.ControlRigSequencerLibrary.bake_to_control_rig(ue_rig._world(), seq, ue_rig._rig_class("/Game/Characters/UE5_Mannequins/Rigs/CR_Mannequin_Body"), unreal.AnimSeqExportOption(), False, 0.001, binding)
    track, sec = ue_rig._rig_section(binding)
    res = {}
    controls = ["upperarm_l_fk_ctrl", "lowerarm_l_fk_ctrl", "hand_l_fk_ctrl",
                "upperarm_r_fk_ctrl", "lowerarm_r_fk_ctrl", "hand_r_fk_ctrl",
                "body_ctrl", "chest_ctrl", "head_ctrl", "foot_l_ik_ctrl", "foot_r_ik_ctrl"]
    for c in sec.get_all_channels():
        name = str(c.channel_name)
        for ctrl in controls:
            if name.startswith(ctrl + "."):
                for k in c.get_keys():
                    if k.get_time().frame_number.value == 0:
                        res[name] = round(k.get_value(), 2)
                        break
    unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
    unreal.EditorAssetLibrary.delete_asset("/Game/AnimPipeline/Rig/LS_Idle_Inspect")
    return json.dumps(res, indent=2)
def make_clean_source_take(src_anim, dst_name, num_frames=14, **kwargs):
    import unreal
    eal = unreal.EditorAssetLibrary
    src = unreal.load_asset(src_anim)
    skel = src.get_editor_property("skeleton")
    fac = unreal.AnimSequenceFactory()
    fac.set_editor_property("target_skeleton", skel)
    folder = "/Game/AnimPipeline/Pilot"
    full_dst = f"{folder}/{dst_name}"
    if eal.does_asset_exist(full_dst):
        eal.delete_asset(full_dst)
    dst = unreal.AssetToolsHelpers.get_asset_tools().create_asset(dst_name, folder, unreal.AnimSequence, fac)
    
    # Create a level sequence with src, cut to num_frames, export to dst
    rate = unreal.FrameRate(60, 1)
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(f"LS_Tmp_{dst_name}", folder, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(rate)
    seq.set_playback_start(0)
    seq.set_playback_end(num_frames - 1)
    
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0,0,0))
    actor.skeletal_mesh_component.set_skinned_asset_and_update(unreal.load_asset(MESH)) if hasattr(actor.skeletal_mesh_component, "set_skinned_asset_and_update") else actor.skeletal_mesh_component.set_skeletal_mesh(unreal.load_asset(MESH))
    binding = seq.add_spawnable_from_instance(actor)
    actor.destroy_actor()
    
    tr = binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    sec = tr.add_section()
    sec.set_range(0, num_frames)
    p = sec.get_editor_property("params")
    p.set_editor_property("animation", src)
    sec.set_editor_property("params", p)
    
    LSE = unreal.LevelSequenceEditorBlueprintLibrary
    LSE.open_level_sequence(seq)
    LSE.refresh_current_level_sequence()
    
    eo = unreal.AnimSeqExportOption()
    eo.set_editor_property("use_custom_frame_rate", True)
    eo.set_editor_property("custom_frame_rate", rate)
    ok = unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(dst, eo, binding, False)
    
    LSE.close_level_sequence()
    eal.delete_asset(f"{folder}/LS_Tmp_{dst_name}")
    return {"created": full_dst, "ok": ok, "frames": num_frames}
def test_anim_lib(**kwargs):
    import unreal, json
    methods = [m for m in dir(unreal.AnimationLibrary) if not m.startswith("_")]
    return json.dumps(methods, indent=2)
def make_clean_idle_take(dst_asset, num_frames, **kwargs):
    import unreal
    eal = unreal.EditorAssetLibrary
    src = "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop"
    if eal.does_asset_exist(dst_asset):
        eal.delete_asset(dst_asset)
    eal.duplicate_asset(src, dst_asset)
    anim = eal.load_asset(dst_asset)
    ctrl = anim.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("Trim to move frames"), True)
    ctrl.set_number_of_frames(unreal.FrameNumber(num_frames - 1), True)
    ctrl.close_bracket(True)
    eal.save_asset(dst_asset, only_if_is_dirty=False)
    length = anim.get_play_length()
    return {"dst": dst_asset, "frames": num_frames, "play_length": length}
def set_take_frame_rate_and_length(dst_asset, target_frames_60fps, **kwargs):
    import unreal
    eal = unreal.EditorAssetLibrary
    anim = eal.load_asset(dst_asset)
    ctrl = anim.get_editor_property("controller")
    ctrl.open_bracket(unreal.Text("Set exact frame count"), True)
    # The source template is 30 fps, so 1 frame in 60fps corresponds to 2 ticks in the controller
    # We want target_frames_60fps sampled keys
    ctrl.set_number_of_frames(unreal.FrameNumber(target_frames_60fps * 2 - 1), True)
    ctrl.close_bracket(True)
    eal.save_asset(dst_asset, only_if_is_dirty=False)
    return {
        "asset": dst_asset,
        "play_length": anim.get_play_length(),
        "frames": target_frames_60fps
    }
def check_frame_count(asset, **kwargs):
    import unreal
    eal = unreal.EditorAssetLibrary
    anim = eal.load_asset(asset)
    dm = anim.get_data_model()
    return {
        "num_frames": dm.get_number_of_frames(),
        "num_keys": dm.get_number_of_keys(),
        "frame_rate": str(dm.get_frame_rate()),
        "length": anim.get_play_length()
    }
def check_frame_count2(asset, **kwargs):
    import unreal
    eal = unreal.EditorAssetLibrary
    anim = eal.load_asset(asset)
    n = anim.get_editor_property("number_of_sampled_frames") if hasattr(anim, "number_of_sampled_frames") else None
    l = anim.get_play_length()
    return {"sampled_frames": n, "play_length": l}
def list_pilot_assets(**kwargs):
    import unreal, json
    eal = unreal.EditorAssetLibrary
    assets = eal.list_assets('/Game/AnimPipeline/Pilot', recursive=True, include_folder=False)
    jabs = [str(a) for a in assets if 'jab' in str(a).lower()]
    return json.dumps(jabs, indent=2)
