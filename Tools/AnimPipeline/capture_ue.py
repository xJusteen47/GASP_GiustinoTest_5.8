# Editor-side helpers for the visual check. Run through UE_MCP_Bridge run_python_file with an entryPoint.
# Works only in /Game/AnimPipeline/Pilot/L_PilotCapture; never touches DefaultLevel.
import unreal, json, os, array, math, time

LEVEL = "/Game/AnimPipeline/Pilot/L_PilotCapture"
MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
OUT = (os.environ.get("ANIMPIPELINE_DIR") or os.path.dirname(os.path.abspath(__file__))).replace("\\", "/").rstrip("/") + "/sheets/raw/"
CENTER = unreal.Vector(0, 15, 95)

def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

def _actor(label):
    for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if a.get_actor_label() == label:
            return a

def status():
    w = _world()
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    return {"world": w.get_path_name(), "dirty_maps": dirty}

def setup():
    w = _world()
    if not w.get_path_name().startswith(LEVEL):
        dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
        if dirty:
            return {"error": "dirty maps, not switching level", "dirty": dirty}
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
            les.load_level(LEVEL)
        else:
            les.new_level(LEVEL)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not _actor("PilotChar"):
        a = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        a.set_actor_label("PilotChar")
    c = _actor("PilotChar").skeletal_mesh_component
    if not c.get_skeletal_mesh_asset():
        c.set_skeletal_mesh_asset(unreal.load_asset(MESH))
    c.set_update_animation_in_editor(True)
    c.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    if not _actor("PilotKey"):
        l = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(-40, -130, 0))
        l.set_actor_label("PilotKey")
        l2 = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(-20, 60, 0))
        l2.set_actor_label("PilotFill")
        l2.light_component.set_intensity(1.5)
    if not _actor("PilotCam"):
        cam = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(0, 300, 100), unreal.Rotator(0, -90, 0))
        cam.set_actor_label("PilotCam")
    return status()

def _rt(name, size, fmt):
    key = "_rt_" + name
    rt = globals().get(key)
    if rt is None:
        rt = unreal.RenderingLibrary.create_render_target2d(_world(), size, size, fmt, unreal.LinearColor(0, 0, 0, 0))
        globals()[key] = rt
    return rt

HIDE_BONE = "clavicle_r"

def set_frame(anim_path, frame, hide_arm=False, bone=None):
    bone = bone or HIDE_BONE
    a = _actor("PilotChar"); c = a.skeletal_mesh_component
    anim = unreal.load_asset(anim_path)
    # override_animation_data does not swap the clip once one is set: switch explicitly, then scrub (no playback).
    if c.get_editor_property("animation_data").get_editor_property("anim_to_play") != anim:
        c.set_animation(anim)
    c.override_animation_data(anim, False, False, frame / 60.0, 0.0)
    c.set_position(frame / 60.0, False)
    if hide_arm:
        c.hide_bone_by_name(bone, unreal.PhysBodyOp.PBO_NONE)
    else:
        for b in ("clavicle_r", "clavicle_l"):
            c.un_hide_bone_by_name(b)
    return {"hand_r": [round(v, 1) for v in c.get_socket_location("hand_r").to_tuple()]}

def relight(az=315):
    # key from the camera side of the 3/4 view at azimuth az, fill from the mirrored front side, rim from behind
    yaw_key = -45 if az == 315 else -135
    k = _actor("PilotKey"); k.set_actor_rotation(unreal.Rotator(-35, yaw_key, 0), False)
    k.light_component.set_intensity(6.0)
    f = _actor("PilotFill"); f.set_actor_rotation(unreal.Rotator(-15, -180 - yaw_key, 0), False)
    f.light_component.set_intensity(3.0)
    if not _actor("PilotRim"):
        r = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 300), unreal.Rotator(-20, 90, 0))
        r.set_actor_label("PilotRim")
        r.light_component.set_intensity(2.0)
    sc = _actor("PilotCam").capture_component2d
    pp = sc.get_editor_property("post_process_settings")
    pp.set_editor_property("override_auto_exposure_method", True)
    pp.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
    pp.set_editor_property("override_auto_exposure_bias", True)
    pp.set_editor_property("auto_exposure_bias", 12.0)
    sc.set_editor_property("post_process_settings", pp)
    sc.set_editor_property("post_process_blend_weight", 1.0)
    return "ok"

def hide(flag=True):
    c = _actor("PilotChar").skeletal_mesh_component
    if flag:
        c.hide_bone_by_name("clavicle_r", unreal.PhysBodyOp.PBO_NONE)
    else:
        c.un_hide_bone_by_name("clavicle_r")
    return {"hidden": c.is_bone_hidden_by_name("clavicle_r")}

def _place(cam, az, el, dist, center=None):
    c = unreal.Vector(*center) if center else CENTER
    r = math.radians(az); e = math.radians(el)
    # az 0 = in front of the character (it faces +Y), positive az turns toward the character's left (+X)
    loc = unreal.Vector(c.x + dist * math.cos(e) * math.sin(r), c.y + dist * math.cos(e) * math.cos(r), c.z + dist * math.sin(e))
    cam.set_actor_location_and_rotation(loc, unreal.MathLibrary.find_look_at_rotation(loc, c), False, True)

def capture(kind, az, el, tag, size=512, dist=None, fov=None, center=None):
    """kind: 'depth' (ortho silhouette, raw float file) or 'lit' (perspective, raw RGB; size x size px)."""
    t0 = time.time()
    cam = _actor("PilotCam"); sc = cam.capture_component2d
    char = _actor("PilotChar")
    sc.set_editor_property("capture_every_frame", False)
    sc.set_editor_property("capture_on_movement", False)
    os.makedirs(OUT, exist_ok=True)
    if kind == "depth":
        rt = _rt("depth", 256, unreal.TextureRenderTargetFormat.RTF_RGBA16F)
        sc.set_editor_property("projection_type", unreal.CameraProjectionMode.ORTHOGRAPHIC)
        sc.set_editor_property("ortho_width", 200.0)
        sc.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_SCENE_DEPTH)
        sc.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        sc.clear_show_only_components()
        sc.show_only_actor_components(char)
        _place(cam, az, el, 400)
        sc.set_editor_property("texture_target", rt)
        sc.capture_scene()
        px = unreal.RenderingLibrary.read_render_target_raw(_world(), rt, False)
        buf = array.array("f", [p.r for p in px])
        with open(OUT + tag + ".f32", "wb") as f:
            buf.tofile(f)
        return {"file": tag + ".f32", "n": len(buf), "min": min(buf), "sec": round(time.time() - t0, 3)}
    rt = _rt("lit" if size == 512 else f"lit{size}", size, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    sc.set_editor_property("projection_type", unreal.CameraProjectionMode.PERSPECTIVE)
    sc.set_editor_property("fov_angle", float(fov or 40.0))
    sc.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    sc.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_RENDER_SCENE_PRIMITIVES)
    _place(cam, az, el, float(dist or 320), center)
    sc.set_editor_property("texture_target", rt)
    sc.capture_scene()
    px = unreal.RenderingLibrary.read_render_target(_world(), rt, False)
    buf = bytearray()
    for p in px:
        buf += bytes((p.r, p.g, p.b))
    with open(OUT + tag + ".rgb", "wb") as f:
        f.write(buf)
    return {"file": tag + ".rgb", "n": len(px), "sec": round(time.time() - t0, 3)}

