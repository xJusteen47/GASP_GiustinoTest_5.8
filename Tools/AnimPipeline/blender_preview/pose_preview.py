"""PROTO pose preview: render a move clip (FBX) on a proxy mannequin in Blender, from behind and from the side, in seconds.

Owner 2026-11-24 (choice 2B): a fast offline look at silhouette and pose, to throw away a wrong round BEFORE the in-game
capture. Everything this writes is stamped PROTO (rule 6): it is never a review and never goes to the owner. Rule 10 still
judges only the in-game capture: this preview has no Mover root motion, capsule, hitstop pose, foot IK, blend from the
previous state, hit on the dummy or hitboxes. It shows the clip exactly as authored, nothing more.

Input: the clip FBX that goes to Unreal (UEFN mannequin skeleton, bones only, as written by the authoring scripts or
clipcheck/export_clips_ue.py). The mesh is not in the FBX: a proxy body (capsules weighted 1:1 to the main bones, left
limbs blue, right limbs orange) is built from the rest skeleton, so it works for any clip on that skeleton.

Views use the in-game capture's convention (capture_ue.py _place, ingame_judge.py): az 0 = in front of the character,
positive az toward the character's left; perspective fov 55, distance 450 cm, elevation 12 deg, camera following the
pelvis horizontally. Defaults: back = az 180, side = az 90.

Frames: anim frames (0 = first frame of the clip) or video frames "v13" counted like the in-game video, which repeats the
first active frame for the hitstop (animpipe/vframes.py; pass --at and --hitstop as for `animpipe render`).

Run (cloud OK: `pip install bpy numpy pillow`, ffmpeg for --video; software GL is enough):
  python -I Tools/AnimPipeline/blender_preview/pose_preview.py <clip.fbx> --out <dir>
     [--frames 0,6,12 | v13,v21] [--every 4] [--views back,side | 180,90,45] [--base <base.fbx>]
     [--at N --hitstop 8] [--video] [--size 480] [--silhouette] [--keep-frames]
Writes <dir>/grid_az<N>.png (one grid per view; with --base: base row above variant row, same frames), optional
<dir>/preview_az<N>.mp4 (60 fps, every frame), <dir>/preview.json; prints at most 5 lines (rule 4).
"""
import argparse
import json
import math
import os
import sys
import time

import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from animpipe.vframes import video_to_anim  # noqa: E402  (pure function, no engine)

VIEWS = {"back": 180.0, "side": 90.0, "front": 0.0, "right": 270.0, "q34": 135.0}
FOV, DIST_CM, EL_DEG = 55.0, 450.0, 12.0
LEFT, RIGHT, MID = (0.20, 0.42, 0.85, 1), (0.95, 0.55, 0.15, 1), (0.72, 0.72, 0.70, 1)

# Proxy body: (bone that drives it, from, to, radius in cm on a 85 cm thigh-to-ankle leg). "from"/"to" are bone heads;
# "+tip" is the bone head pushed along the parent->bone direction (head, toes), so the shape needs no extra bone.
SEGS = [
    ("pelvis", "pelvis", "spine_01", 15), ("spine_01", "spine_01", "spine_03", 14), ("spine_03", "spine_03", "spine_05", 16),
    ("spine_05", "spine_05", "neck_01", 9), ("neck_01", "neck_01", "head", 6), ("head", "head", "head+tip", 11),
]
for s in "lr":
    SEGS += [
        (f"clavicle_{s}", f"clavicle_{s}", f"upperarm_{s}", 5), (f"upperarm_{s}", f"upperarm_{s}", f"lowerarm_{s}", 6),
        (f"lowerarm_{s}", f"lowerarm_{s}", f"hand_{s}", 5), (f"hand_{s}", f"hand_{s}", f"middle_01_{s}", 5),
        (f"thigh_{s}", f"thigh_{s}", f"calf_{s}", 9), (f"calf_{s}", f"calf_{s}", f"foot_{s}", 6.5),
        (f"foot_{s}", f"foot_{s}", f"ball_{s}", 5), (f"ball_{s}", f"ball_{s}", f"ball_{s}+tip", 4),
    ]
TIP = {"head": ("neck_01", 0.95), "ball_l": ("foot_l", 0.45), "ball_r": ("foot_r", 0.45)}  # length vs parent segment


def load(fbx, tag):
    """Import one clip; returns (armature, first anim frame in scene frames, last)."""
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=fbx, automatic_bone_orientation=False)
    arm = next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")
    arm.name = tag
    act = arm.animation_data.action
    f0, f1 = (int(round(x)) for x in act.frame_range)
    return arm, f0, f1


def rest_head(arm, name, posed=False):
    """World position of a joint, in the rest skeleton or (posed) at the current frame."""
    if name.endswith("+tip"):
        b = name[:-4]
        par, k = TIP[b]
        h, p = rest_head(arm, b, posed), rest_head(arm, par, posed)
        return h + (h - p) * k
    if posed:
        return arm.matrix_world @ arm.pose.bones[name].head
    return arm.matrix_world @ arm.data.bones[name].head_local


PROXY = {}  # armature name -> (proxy object, [(from, to, radius)])


def capsule(bm_verts, bm_faces, a, b, r, n=12, rings=4):
    """Append a closed capsule a->b (radius r) to the vertex/face lists; returns the new vertex indices."""
    d = b - a
    L = max(d.length, 1e-4)
    z = d / L
    x = z.orthogonal().normalized()
    y = z.cross(x)
    start = len(bm_verts)
    prof = []  # (axial offset, radius) from the bottom pole to the top pole
    for i in range(rings + 1):
        t = -math.pi / 2 + (math.pi / 2) * i / rings
        prof.append((r * math.sin(t), r * math.cos(t)))
    for i in range(rings + 1):
        t = (math.pi / 2) * i / rings
        prof.append((L + r * math.sin(t), r * math.cos(t)))
    for ax, rr in prof:
        for j in range(n):
            ang = 2 * math.pi * j / n
            bm_verts.append(a + z * ax + (x * math.cos(ang) + y * math.sin(ang)) * rr)
    rows = len(prof)
    for i in range(rows - 1):
        for j in range(n):
            q = (start + i * n + j, start + i * n + (j + 1) % n, start + (i + 1) * n + (j + 1) % n, start + (i + 1) * n + j)
            bm_faces.append(q)
    return range(start, len(bm_verts))


def proxy(arm, silhouette):
    """One mesh of capsules between joints, rebuilt from the posed joint positions at every frame (pose_proxy). Not an
    Armature modifier: the joint positions are what the clip checks read too, and they do not depend on how the FBX
    importer guessed bone rolls and lengths (an Armature-deformed proxy came apart on the dash clip)."""
    leg = (rest_head(arm, "thigh_l") - rest_head(arm, "foot_l")).length
    k = leg / 85.0  # radii are in cm on an 85 cm leg; the FBX may be in m or cm
    verts, faces, groups, mats = [], [], {}, []
    for bone, fa, fb, r in SEGS:
        if bone not in arm.data.bones:
            continue
        f_before = len(faces)
        capsule(verts, faces, rest_head(arm, fa), rest_head(arm, fb), r * k)
        groups.setdefault(bone, []).append((fa, fb, r * k))
        side = LEFT if bone.endswith("_l") else RIGHT if bone.endswith("_r") else MID
        mats += [side] * (len(faces) - f_before)
    me = bpy.data.meshes.new(arm.name + "_proxy")
    me.from_pydata([tuple(v) for v in verts], [], faces)
    ob = bpy.data.objects.new(arm.name + "_proxy", me)
    bpy.context.scene.collection.objects.link(ob)
    PROXY[arm.name] = (ob, [seg for segs in groups.values() for seg in segs])
    palette = {}
    for c in (MID, LEFT, RIGHT):
        m = bpy.data.materials.new(str(c))
        m.diffuse_color = (0.02, 0.02, 0.02, 1) if silhouette else c
        me.materials.append(m)
        palette[c] = len(me.materials) - 1
    for p, c in zip(me.polygons, mats):
        p.material_index = palette[c]
        p.use_smooth = True
    arm.hide_render = True
    return k


def pose_proxy(arm):
    """Move the proxy's vertices to the current frame's joints (same topology as built in proxy())."""
    ob, segs = PROXY[arm.name]
    verts, faces = [], []
    for fa, fb, r in segs:
        capsule(verts, faces, rest_head(arm, fa, True), rest_head(arm, fb, True), r)
    ob.data.vertices.foreach_set("co", [c for v in verts for c in v])
    ob.data.update()


def axes(arm, f0):
    """Character forward/left on the ground from the first frame: feet (foot->ball) and hips (thigh_r->thigh_l)."""
    bpy.context.scene.frame_set(f0)
    P = lambda n: arm.matrix_world @ arm.pose.bones[n].head
    fwd = (P("ball_l") - P("foot_l")) + (P("ball_r") - P("foot_r"))
    left = P("thigh_l") - P("thigh_r")
    up = Vector((0, 0, 1))
    left = (left - up * left.dot(up)).normalized()
    fwd = (fwd - up * fwd.dot(up))
    fwd = (fwd - left * fwd.dot(left)).normalized()
    return fwd, left


def stage(k, silhouette, size):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sc.render.resolution_x = sc.render.resolution_y = size
    sc.render.resolution_percentage = 100
    sh = sc.display.shading
    sh.light = "FLAT" if silhouette else "STUDIO"
    sh.color_type = "MATERIAL"
    sh.show_shadows = not silhouette
    sh.show_cavity = False
    sh.background_type = "VIEWPORT"
    sh.background_color = (1, 1, 1) if silhouette else (0.82, 0.84, 0.88)
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "Standard"
    # Ground with a 50 cm stripe grid fixed in the world, so travel and root motion read against it.
    s = 2000 * k
    verts, faces = [(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [(0, 1, 2, 3)]
    if not silhouette:
        w, z = 0.6 * k, 0.05 * k
        for i in range(-40, 41):
            c = i * 50 * k
            for q in ([(c - w, -s), (c + w, -s), (c + w, s), (c - w, s)], [(-s, c - w), (s, c - w), (s, c + w), (-s, c + w)]):
                n = len(verts)
                verts += [(x, y, z) for x, y in q]
                faces.append((n, n + 1, n + 2, n + 3))
    me = bpy.data.meshes.new("ground")
    me.from_pydata(verts, [], faces)
    for col in ((0.93, 0.93, 0.93, 1) if silhouette else (0.62, 0.64, 0.62, 1), (0.45, 0.47, 0.45, 1)):
        m = bpy.data.materials.new("ground")
        m.diffuse_color = col
        me.materials.append(m)
    for p in me.polygons:
        p.material_index = 0 if p.index == 0 else 1
    sc.collection.objects.link(bpy.data.objects.new("ground", me))
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.angle = math.radians(FOV)
    cam.data.clip_start, cam.data.clip_end = 1 * k, 10000 * k
    sc.collection.objects.link(cam)
    sc.camera = cam
    return cam


def place(cam, arm, frame, az, fwd, left, k, pelvis_z):
    sc = bpy.context.scene
    sc.frame_set(frame)
    pose_proxy(arm)
    p = arm.matrix_world @ arm.pose.bones["pelvis"].head
    c = Vector((p.x, p.y, pelvis_z))  # follow horizontally only: a jump or a crouch shows against a still frame line
    r, e = math.radians(az), math.radians(EL_DEG)
    d = (fwd * math.cos(r) + left * math.sin(r)) * math.cos(e) + Vector((0, 0, math.sin(e)))
    cam.location = c + d * DIST_CM * k
    cam.rotation_euler = (c - cam.location).to_track_quat("-Z", "Y").to_euler()


def render(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def grid(rows, labels, row_names, out, title):
    from PIL import Image, ImageDraw
    im0 = Image.open(rows[0][0])
    w, h = im0.size
    pad, top = 4, 30
    G = Image.new("RGB", (len(rows[0]) * (w + pad) + pad + 90, len(rows) * (h + pad) + pad + top), (255, 255, 255))
    d = ImageDraw.Draw(G)
    d.text((pad, 8), title, fill=(200, 0, 0))
    for ri, row in enumerate(rows):
        y = top + pad + ri * (h + pad)
        d.text((pad, y + h // 2), row_names[ri], fill=(0, 0, 0))
        for ci, f in enumerate(row):
            x = 90 + pad + ci * (w + pad)
            G.paste(Image.open(f).convert("RGB"), (x, y))
            d.rectangle([x, y, x + 120, y + 16], fill=(255, 255, 255))
            d.text((x + 3, y + 3), labels[ci], fill=(0, 0, 0))
            d.text((x + w - 44, y + h - 14), "PROTO", fill=(200, 0, 0))
    G.save(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("fbx")
    ap.add_argument("--out", required=True)
    ap.add_argument("--base")
    ap.add_argument("--frames", help="anim frames 0,6,12 or video frames v13,v21")
    ap.add_argument("--every", type=int, default=0, help="default frames: every N anim frames (default: 12 cells)")
    ap.add_argument("--views", default="back,side")
    ap.add_argument("--at", type=int)
    ap.add_argument("--hitstop", type=int, default=8)
    ap.add_argument("--video", action="store_true")
    ap.add_argument("--size", type=int, default=480)
    ap.add_argument("--silhouette", action="store_true")
    ap.add_argument("--keep-frames", action="store_true", help="keep the single PNGs in <out>/_frames")
    a = ap.parse_args()
    t0 = time.time()
    os.makedirs(a.out, exist_ok=True)
    tmp = os.path.join(a.out, "_frames")
    os.makedirs(tmp, exist_ok=True)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    clips = [("variant", a.fbx)] + ([("base", a.base)] if a.base else [])
    loaded = {}
    for tag, path in clips:
        arm, f0, f1 = load(path, tag)
        loaded[tag] = (arm, f0, f1)
    k = 1.0
    for tag, (arm, f0, f1) in loaded.items():
        k = proxy(arm, a.silhouette)
    cam = stage(k, a.silhouette, a.size)
    arm, f0, f1 = loaded["variant"]
    n_anim = f1 - f0 + 1

    # Frames: video numbers map to anim frames through the hitstop freeze, like the in-game video.
    if a.frames:
        toks = [t.strip() for t in a.frames.split(",") if t.strip()]
        cells = []
        for t in toks:
            if t[0] in "vV":
                v = int(t[1:])
                cells.append((f"v{v} a{video_to_anim(v, a.at, a.hitstop)}", video_to_anim(v, a.at, a.hitstop)))
            else:
                cells.append((f"a{int(t)}", int(t)))
    else:
        step = a.every or max(1, math.ceil(n_anim / 12))
        cells = [(f"a{f}", f) for f in range(0, n_anim, step)]
    cells = [(l, f) for l, f in cells if 0 <= f < n_anim]

    azs = [VIEWS.get(v.strip(), None) if not v.strip().lstrip("-").replace(".", "").isdigit() else float(v)
           for v in a.views.split(",")]
    azs = [z for z in azs if z is not None]
    order = ["base", "variant"] if a.base else ["variant"]
    info = {"proto": True, "fbx": a.fbx, "base": a.base, "anim_frames": n_anim, "cells": cells, "views": azs, "outputs": []}
    for az in azs:
        rows = []
        for tag in order:
            arm_t, f0_t, _ = loaded[tag]
            fwd, left = axes(arm_t, f0_t)
            pz = (arm_t.matrix_world @ arm_t.data.bones["pelvis"].head_local).z
            for other in loaded.values():  # only the clip being shot is visible
                bpy.data.objects[other[0].name + "_proxy"].hide_render = other[0] is not arm_t
            row = []
            for lab, f in cells:
                place(cam, arm_t, f0_t + f, az, fwd, left, k, pz)
                p = os.path.join(tmp, f"{tag}_az{int(az)}_a{f:03d}.png")
                render(p)
                row.append(p)
            rows.append(row)
        g = os.path.join(a.out, f"grid_az{int(az)}.png")
        grid(rows, [l for l, _ in cells], order, g, f"PROTO - Blender proxy, not a review (rule 6/10) - az {int(az)}")
        info["outputs"].append(g)
        if a.video:
            fwd, left = axes(arm, f0)
            pz = (arm.matrix_world @ arm.data.bones["pelvis"].head_local).z
            for other in loaded.values():
                bpy.data.objects[other[0].name + "_proxy"].hide_render = other[0] is not arm
            seq = []  # same numbering as the in-game video: the first active frame is held for the hitstop
            v = 0
            while True:
                f = video_to_anim(v, a.at, a.hitstop) if a.at is not None else v
                if f >= n_anim:
                    break
                seq.append(f)
                v += 1
            done = {}
            for i, f in enumerate(seq):
                p = os.path.join(tmp, f"vid_az{int(az)}_a{f:03d}.png")
                if f not in done:
                    place(cam, arm, f0 + f, az, fwd, left, k, pz)
                    render(p)
                    done[f] = p
                os.link(done[f], os.path.join(tmp, f"seq_az{int(az)}_{i:04d}.png")) if not os.path.exists(
                    os.path.join(tmp, f"seq_az{int(az)}_{i:04d}.png")) else None
            mp4 = os.path.join(a.out, f"preview_az{int(az)}.mp4")
            os.system(f'ffmpeg -loglevel error -y -framerate 60 -i "{tmp}/seq_az{int(az)}_%04d.png" -vf '
                      f'"drawtext=text=\'PROTO v%{{frame_num}}\':x=8:y=8:fontcolor=red:fontsize=18" '
                      f'-pix_fmt yuv420p "{mp4}" 2>/dev/null || ffmpeg -loglevel error -y -framerate 60 -i '
                      f'"{tmp}/seq_az{int(az)}_%04d.png" -pix_fmt yuv420p "{mp4}"')
            info["outputs"].append(mp4)
    if not a.keep_frames:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)
    info["seconds"] = round(time.time() - t0, 1)
    with open(os.path.join(a.out, "preview.json"), "w") as f:
        json.dump(info, f, indent=1)
    print(f"PROTO preview: {len(cells)} frames x {len(azs)} views x {len(order)} clips in {info['seconds']} s")
    for o in info["outputs"][:4]:
        print(o)


if __name__ == "__main__":
    main()
