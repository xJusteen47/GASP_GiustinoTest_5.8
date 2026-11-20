#!/usr/bin/env python3
"""fbx_to_bvh.py <in.fbx> <out.bvh> [--fps 60]

Convert a skeletal FBX clip to BVH with Blender's Python module (pip install bpy), so kb_motion.py can measure it.
Generic: no engine or project names. Prints one line.
"""
import sys, argparse
import bpy

ap = argparse.ArgumentParser(); ap.add_argument('src'); ap.add_argument('dst'); ap.add_argument('--fps', type=int, default=60)
a = ap.parse_args(sys.argv[1:])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=a.src, automatic_bone_orientation=False)
arm = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
bpy.context.view_layer.objects.active = arm; arm.select_set(True)
act = arm.animation_data.action if arm.animation_data else None
f0, f1 = (int(act.frame_range[0]), int(act.frame_range[1])) if act else (1, 1)
bpy.context.scene.render.fps = a.fps
bpy.ops.export_anim.bvh(filepath=a.dst, frame_start=f0, frame_end=f1, root_transform_only=False)
print(f'wrote {a.dst} frames {f0}-{f1}')
