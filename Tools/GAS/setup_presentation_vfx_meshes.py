"""VFX v2 (Astro Bot style, docs/notes/stile/vfx-stile-v2-astrobot.md): the kit's low-poly meshes, written as OBJ and imported.

Shapes of the style sheet's vocabulary, 100 cm across at scale 1 (a particle's Scale = its size in cm / 100), smooth normals, one section:
- SM_GSTN_VFX_Ball: rounded sphere, 8 sides x 6 bands = 80 triangles (dust, landing balls, trail)
- SM_GSTN_VFX_TorusZ / TorusX: ring lying on the ground (axis Z) / facing X (walls, the hit's ring along the push), 12 x 4 = 96
- SM_GSTN_VFX_Gem: "puffy gem" shard, long along X (the velocity-aligned axis), 8 sides x 5 bands = 64
- SM_GSTN_VFX_Capsule: tapered spindle along X (sprint lines), 8 sides x 4 bands = 48
No paid assets, no Geometry Script dependency (the plugin is not enabled): plain OBJ text, imported with Interchange.
The OBJ axes are remapped (AXES) so that the asset's X/Y/Z are the ones above; check() reads the bounds back and fails loudly otherwise.

Run in the open editor: python -I Tools/GAS/run_in_editor.py setup_presentation_vfx_meshes      (idempotent: re-imported in place)
"""
import math
import os

import unreal

ROOT = "/GSTNPresentation/VFX/Meshes"
EAL = unreal.EditorAssetLibrary
ATL = unreal.AssetToolsHelpers.get_asset_tools()
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "VfxMeshes")

# UE (x, y, z) -> OBJ (a, b, c). Measured on the first import (5.9 Interchange): the OBJ's axes land on the asset's as they are (an OBJ
# written (x, z, -y) gave a torus thin on Y), so no remap. Winding: checked against the engine's sphere (winding_sign), flipped if needed
AXES = lambda x, y, z: (x, y, z)
FLIP = [False]


def _revolve(profile, sides):
    """profile: [(x, r)] along X, first and last r = 0 (tips). Returns verts, normals, faces (vertex index triples, CCW from outside)"""
    verts, norms, rings = [], [], []
    n = len(profile)
    for i, (x, r) in enumerate(profile):
        # Smooth normal: perpendicular to the profile's tangent in the (x, r) plane
        x0, r0 = profile[max(i - 1, 0)]
        x1, r1 = profile[min(i + 1, n - 1)]
        tx, tr = x1 - x0, r1 - r0
        nx, nr = -tr, tx
        length = math.hypot(nx, nr) or 1.0
        nx, nr = nx / length, nr / length
        if r == 0.0:
            verts.append((x, 0.0, 0.0))
            norms.append((-1.0 if i == 0 else 1.0, 0.0, 0.0))
            rings.append([len(verts) - 1] * sides)
            continue
        ring = []
        for s in range(sides):
            a = 2 * math.pi * s / sides
            ca, sa = math.cos(a), math.sin(a)
            verts.append((x, r * ca, r * sa))
            norms.append((nx, nr * ca, nr * sa))
            ring.append(len(verts) - 1)
        rings.append(ring)
    faces = []
    for i in range(n - 1):
        a_ring, b_ring = rings[i], rings[i + 1]
        for s in range(sides):
            t = (s + 1) % sides
            a0, a1, b0, b1 = a_ring[s], a_ring[t], b_ring[s], b_ring[t]
            if a0 != a1:
                faces.append((a0, b0, a1))
            if b0 != b1:
                faces.append((a1, b0, b1))
    return verts, norms, faces


def _torus(axis, major, minor, seg_major, seg_minor):
    verts, norms, faces = [], [], []
    for i in range(seg_major):
        u = 2 * math.pi * i / seg_major
        cu, su = math.cos(u), math.sin(u)
        for j in range(seg_minor):
            v = 2 * math.pi * j / seg_minor + math.pi / seg_minor
            cv, sv = math.cos(v), math.sin(v)
            # Ring in the plane (p, q), axis w
            p, q, w = (major + minor * cv) * cu, (major + minor * cv) * su, minor * sv
            np_, nq, nw = cv * cu, cv * su, sv
            if axis == "Z":
                verts.append((p, q, w))
                norms.append((np_, nq, nw))
            else:
                verts.append((w, p, q))
                norms.append((nw, np_, nq))
    for i in range(seg_major):
        for j in range(seg_minor):
            a = i * seg_minor + j
            b = ((i + 1) % seg_major) * seg_minor + j
            c = ((i + 1) % seg_major) * seg_minor + (j + 1) % seg_minor
            d = i * seg_minor + (j + 1) % seg_minor
            faces += [(a, b, c), (a, c, d)]
    if axis == "X":
        faces = [(a, c, b) for a, b, c in faces]  # the (w, p, q) swap mirrors the winding
    return verts, norms, faces


def _sphere():
    bands = 6
    return _revolve([(50.0 * math.cos(math.pi * i / bands), 50.0 * math.sin(math.pi * i / bands)) for i in range(bands + 1)][::-1], 8)


SHAPES = {
    "Ball": _sphere,
    "TorusZ": lambda: _torus("Z", 42.0, 8.0, 12, 4),
    "TorusX": lambda: _torus("X", 42.0, 8.0, 12, 4),
    # Puffy gem: blunt tips, fat middle (round = harmless, shard = hit: this is the hit's shard, kept round-edged)
    "Gem": lambda: _revolve([(-50, 0), (-34, 16), (-12, 28), (12, 28), (34, 16), (50, 0)], 8),
    "Capsule": lambda: _revolve([(-50, 0), (-30, 7), (25, 12), (42, 9), (50, 0)], 8),
}


def _write_obj(name, mesh):
    verts, norms, faces = mesh
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, f"SM_GSTN_VFX_{name}.obj")
    lines = [f"# {name}: {len(faces)} triangles", f"o {name}"]
    lines += ["v %.4f %.4f %.4f" % AXES(*v) for v in verts]
    lines += ["vn %.4f %.4f %.4f" % AXES(*n) for n in norms]
    lines += ["vt %.4f %.4f" % (0.5 + v[1] / 100.0, 0.5 + v[2] / 100.0) for v in verts]
    if FLIP[0]:
        faces = [(a, c, b) for a, b, c in faces]
    lines += ["f %d/%d/%d %d/%d/%d %d/%d/%d" % (a + 1, a + 1, a + 1, b + 1, b + 1, b + 1, c + 1, c + 1, c + 1) for a, b, c in faces]
    open(path, "w", encoding="ascii").write("\n".join(lines) + "\n")
    return path, len(faces)


def _import(path, name):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = ROOT
    task.destination_name = f"SM_GSTN_VFX_{name}"
    task.replace_existing = True
    task.automated = True
    task.save = False
    ATL.import_asset_tasks([task])
    mesh = unreal.load_asset(f"{ROOT}/SM_GSTN_VFX_{name}")
    if mesh is None:
        raise RuntimeError(f"import failed: {path}")
    # Particles: no collision, no lightmap, no Nanite (a toaster renders them with the plain mesh path)
    try:
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", nanite)
    except Exception:
        pass
    EAL.save_loaded_asset(mesh)
    return mesh


def check(mesh, name):
    box = mesh.get_bounding_box()
    size = [box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z]
    want = {"Ball": (100, 100, 100), "TorusZ": (100, 100, 16), "TorusX": (16, 100, 100), "Gem": (100, 56, 56), "Capsule": (100, 24, 24)}[name]
    ok = all(abs(s - w) < 6 for s, w in zip(size, want))
    return ok, "%s %s %.0f x %.0f x %.0f (want %s)" % ("OK" if ok else "AXES?", name, size[0], size[1], size[2], want)


def winding_sign(mesh):
    """+1 when the triangles' geometric normal (v1 - v0) x (v2 - v0) agrees with the vertex normals on most triangles of the first
    section, else -1. A kit mesh must have the engine sphere's sign, or it renders inside out (the materials are one-sided)"""
    return unreal.GSTN_VfxAuthoringLibrary.mesh_winding_sign(mesh)


def run():
    lines = []
    bad = 0
    ref = winding_sign(unreal.load_asset("/Engine/BasicShapes/Sphere"))
    path, _tris = _write_obj("Ball", SHAPES["Ball"]())
    if winding_sign(_import(path, "Ball")) != ref:
        FLIP[0] = True
    lines.append(f"winding: engine sphere {ref}, kit flipped {FLIP[0]}")
    for name, make in SHAPES.items():
        path, tris = _write_obj(name, make())
        mesh = _import(path, name)
        ok, line = check(mesh, name)
        if winding_sign(mesh) != ref:
            ok, line = False, line + " INSIDE OUT"
        bad += 0 if ok else 1
        lines.append(f"{line}, {tris} tri")
    out = unreal.Paths.project_saved_dir() + "Logs/ci/v2-meshes.txt"
    open(out, "w", encoding="utf-8").write("\n".join(lines))
    return f"{len(lines) - 1} meshes, {bad} wrong (Saved/Logs/ci/v2-meshes.txt)"
