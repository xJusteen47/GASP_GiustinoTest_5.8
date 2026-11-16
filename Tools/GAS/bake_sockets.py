# Bakes sockets of an animation into FGSTN_CombatTimelineData.BakedSockets (M4 H13a). Editor Python only; M5's timeline editor replaces it.
#
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import bake_sockets as b
#   b.bake_into_timeline('/Game/GASNPPTest/Combat/TL_Test_HitBaked', '/Game/.../AM_Punch', ['hand_r'], mesh='/Game/.../SK_Mannequin')
#
# The animation (a montage's first sequence, or a sequence) is sampled once per simulation tick (default 60 Hz, the Network Prediction tick of the
# project), one transform per timeline frame, in the space a BakedSocket hitbox uses: relative to the root of the attacker. The sampling is the
# only part that depends on the animation API, so it is isolated in sample_bone_component_space.
#
# mesh_to_root is the transform of the mesh component relative to the actor root (for the sandbox character: z -90, yaw -90); the result is
# the socket in the space of the root. A socket that is not a bone is resolved on the mesh: the bone it hangs from plus its relative transform.
import unreal


def log(msg):
    unreal.log(f'[bake_sockets] {msg}')


def _sequence_of(anim):
    """The animation sequence of a montage (its first segment) or the sequence itself"""
    if isinstance(anim, unreal.AnimMontage):
        tracks = anim.get_editor_property('slot_anim_tracks')
        segments = tracks[0].get_editor_property('anim_track').get_editor_property('anim_segments')
        return segments[0].get_editor_property('anim_reference')
    return anim


def sample_bone_component_space(sequence, bone, time):
    """The bone's transform in component space at `time` seconds: the local poses along the path from the root composed in order"""
    path = unreal.AnimationLibrary.find_bone_path_to_root(sequence, bone)  # bone ... root
    result = unreal.Transform()
    for name in reversed(path):
        local = unreal.AnimationLibrary.get_bone_pose_for_time(sequence, name, time, True)
        result = local * result
    return result


def bake(anim, sockets, total_frames, tick_rate=60.0, mesh=None, mesh_to_root=None):
    """{socket name: [Transform per frame]} for the given sockets"""
    sequence = _sequence_of(anim)
    mesh_asset = unreal.load_asset(mesh) if isinstance(mesh, str) else mesh
    mesh_to_root = mesh_to_root or unreal.Transform()
    length = sequence.get_editor_property('sequence_length') if hasattr(sequence, 'get_editor_property') else 0.0
    out = {}
    for socket in sockets:
        bone, offset = socket, unreal.Transform()
        mesh_socket = mesh_asset.find_socket(socket) if mesh_asset else None
        if mesh_socket is not None:
            bone = str(mesh_socket.get_editor_property('bone_name'))
            offset = unreal.Transform(location=mesh_socket.get_editor_property('relative_location'),
                                      rotation=mesh_socket.get_editor_property('relative_rotation'),
                                      scale=mesh_socket.get_editor_property('relative_scale'))
        frames = []
        for frame in range(total_frames):
            time = min(frame / tick_rate, length)
            frames.append(offset * sample_bone_component_space(sequence, bone, time) * mesh_to_root)
        out[socket] = frames
    return out


def bake_into_timeline(timeline_asset_path, anim_path, sockets, mesh=None, mesh_to_root=None, tick_rate=60.0):
    """Bakes the sockets for every frame of the UGSTN_CombatTimeline asset and stores them in its BakedSockets. Returns True when saved"""
    asset = unreal.load_asset(timeline_asset_path)
    anim = unreal.load_asset(anim_path)
    if asset is None or anim is None:
        log(f'ERROR: asset {timeline_asset_path} or animation {anim_path} not found')
        return False
    data = asset.get_editor_property('data')
    total = data.get_editor_property('total_frames')
    baked = {}
    for socket, frames in bake(anim, sockets, total, tick_rate, mesh, mesh_to_root).items():
        entry = unreal.GSTN_BakedSocketFrames()
        entry.set_editor_property('frames', frames)
        baked[socket] = entry
    data.set_editor_property('baked_sockets', baked)
    asset.set_editor_property('data', data)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log(f'{timeline_asset_path}: baked {list(baked)} for {total} frames at {tick_rate} Hz')
    return True
