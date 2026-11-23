# DROPKICK-CAPSULE (2026-11-15): the Overlord dropkick's movement capsule on the torso instead of the standing capsule (owner: "the capsule should
# be at the centre of the torso, from the video it looks like it is on the head"). Writes two Body Capsule tracks (FGSTN_TimelineTrack_BodyCapsule,
# C++ on claude/project-thread-0pka8k) into /Game/GASNPPTest/Combat/Overlord/TL_Overlord_Dropkick. Data only: no Blueprint graph, no input,
# no project setting. Run AFTER setup_overlord_dropkick_kit.py and setup_dropkick_hold_until_landed.py (the kit script rewrites the timeline).
#
#   air  [startup - AIR_LEAD, land): small capsule (DROPKICK_CAPSULE_HH, default 35 cm) centred on the torso of the LOOP clip's frame 0,
#        Body anchor (the actor moves onto the torso, the mesh is offset back: nothing moves on screen), 20 cm per frame.
#        It lands when the back touches the ground, not ~90 cm earlier (the old one-frame half-metre drop at landing).
#   land [land, total): same capsule, Capsule anchor (it stays on the ground), mesh root back on the capsule bottom (offset z = half height),
#        horizontally on the LAND clip's frame 0 torso so the body does not slide sideways at the touchdown.
#   After the timeline the pawn's own capsule comes back, growing up from the ground (the mesh does not move).
# Torso = midpoint of pelvis and spine_03, in the clip's root space, measured here from the clips (nothing hand typed).
# Mesh axes: the Mannequin faces +Y in mesh space and the pawn's mesh is turned -90 yaw, so capsule X forward = mesh +Y, capsule Y right = mesh -X.
#
# Env: DROPKICK_CAPSULE_HH (cm, default 35), DROPKICK_CAPSULE_AIR_LEAD (frames before the loop, default 6),
#      DROPKICK_CAPSULE_AIR_PIN (2026-11-23, owner: "maybe better to shrink it upward"): where the air capsule shrinks from.
#        torso (default, owner 2026-11-15): centred on the loop's torso, both ends move in (86 -> 35 around the centre).
#        feet: the bottom stays on the mesh root (where the standing capsule's bottom is), only the top comes down. It touches down when the
#              clip ROOT reaches the floor, (loop torso z - hh) cm before torso does (logged): the back is still in the air at touchdown and the
#              land clip shows its own drop (the 11-15 problem torso fixed).
#        head: the top stays where the standing capsule's top is (DROPKICK_STANDING_HH, default 86), only the bottom comes up.
#      The camera no longer reads the capsule (camera anchor Floor source, 2026-11-23): this choice changes collision and landing only.
# Editor:    import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_dropkick_body_capsule as s; s.run()
# Output:    Saved/AnimR2/setup_dropkick_body_capsule.txt, at most 5 printed lines.
import os
import runpy

import unreal

DIR = '/Game/GASNPPTest/Combat/Overlord'
TIMELINE = f'{DIR}/TL_Overlord_Dropkick'
CLIPS = {name: f'{DIR}/AS_OverlordDropkick_{name}' for name in ('Startup', 'Loop', 'Land')}
ANIM_FPS = 60.0
CHAIN = ('pelvis', 'spine_01', 'spine_02', 'spine_03')
OUT = os.path.join(unreal.Paths.project_saved_dir(), 'AnimR2', 'setup_dropkick_body_capsule.txt')
_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[dropkick_capsule] {msg}')


def _torso(anim, frame):
    """Midpoint of pelvis and spine_03 in root space (local poses composed down the chain; pelvis' parent is root)."""
    comp = None
    points = {}
    for bone in CHAIN:
        local = unreal.AnimationLibrary.get_bone_pose_for_frame(anim, bone, frame, False)
        comp = local if comp is None else unreal.MathLibrary.compose_transforms(local, comp)
        points[bone] = comp.translation
    p, s = points['pelvis'], points['spine_03']
    return unreal.Vector((p.x + s.x) * 0.5, (p.y + s.y) * 0.5, (p.z + s.z) * 0.5)


def _capsule_local(v):
    """Mesh space (Mannequin faces +Y, mesh turned -90 yaw) -> capsule frame (X forward, Y right, Z up)."""
    return unreal.Vector(v.y, -v.x, v.z)


def run():
    del _lines[:]
    ok = False
    try:
        lib = unreal.EditorAssetLibrary
        for path in [TIMELINE] + list(CLIPS.values()):
            if not lib.does_asset_exist(path):
                raise RuntimeError(f'{path} missing: run Tools/GAS/setup_overlord_dropkick_kit.py first')
        hh = float(os.environ.get('DROPKICK_CAPSULE_HH', '35'))
        lead = int(os.environ.get('DROPKICK_CAPSULE_AIR_LEAD', '6'))
        pin = os.environ.get('DROPKICK_CAPSULE_AIR_PIN', 'torso').strip().lower()
        standing_hh = float(os.environ.get('DROPKICK_STANDING_HH', '86'))
        if pin not in ('torso', 'feet', 'head'):
            raise RuntimeError(f'DROPKICK_CAPSULE_AIR_PIN={pin!r}: use torso, feet or head')
        anims = {name: unreal.load_asset(path) for name, path in CLIPS.items()}
        asset = unreal.load_asset(TIMELINE)
        data = asset.get_editor_property('data')
        total = data.get_editor_property('total_frames')
        startup = int(round(anims['Startup'].get_play_length() * ANIM_FPS))
        # The land frame as the hold script computed it (first frame of the land clip in the montage)
        reps = int(runpy.run_path(os.path.join(unreal.Paths.project_dir(), 'Tools/AnimPipeline/dropkick_loop_reps.py'))['LOOP_REPS'])
        land = int(round((anims['Startup'].get_play_length() + reps * anims['Loop'].get_play_length()) * ANIM_FPS))
        air_from = max(startup - lead, 0)
        if not (0 <= air_from < land < total):
            raise RuntimeError(f'frames do not fit: air from {air_from}, land {land}, total {total}')

        air = _capsule_local(_torso(anims['Loop'], 0))
        land_torso = _capsule_local(_torso(anims['Land'], 0))
        air_z = {'torso': air.z, 'feet': hh, 'head': 2.0 * standing_hh - hh}[pin]
        air_offset = unreal.Vector(air.x, air.y, air_z)
        land_offset = unreal.Vector(land_torso.x, land_torso.y, hh)

        utils = unreal.GSTN_TimelineUtils
        data = utils.remove_body_capsule_tracks(data)
        data = utils.add_body_capsule_track(data, air_from, land, hh, 0.0, air_offset, unreal.GSTN_BodyCapsuleAnchor.BODY, 20.0)
        data = utils.add_body_capsule_track(data, land, total, hh, 0.0, land_offset, unreal.GSTN_BodyCapsuleAnchor.CAPSULE, 0.0)
        asset.set_editor_property('data', data)
        lib.save_loaded_asset(asset, False)
        log(f'torso loop f0 {air.x:.1f},{air.y:.1f},{air.z:.1f} land f0 {land_torso.x:.1f},{land_torso.y:.1f},{land_torso.z:.1f} (capsule frame, cm)')
        log(f'TL_Overlord_Dropkick: air [{air_from}, {land}) hh {hh:.0f} Body pin {pin} (centre z {air_z:.1f}); land [{land}, {total}) hh {hh:.0f} Capsule; '
            f'torso drop at touchdown ~{hh - land_torso.z:.0f} cm (was {air.z - land_torso.z:.0f} cm in the clips); saved')
        ok = True
    except Exception as e:
        log(f'ERROR: {e!r}')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    for line in _lines[-4:]:
        print(line[:300])
    return ok


if __name__ == '__main__':
    run()
