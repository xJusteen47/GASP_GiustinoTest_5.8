"""greybox-capsula (owner 2026-11-24, 1A 2A): builds the greybox assets under /Game/GSTN/Greybox.

  SK_GSTN_GreyboxCapsule   capsule with a nose (plus hand and foot markers), on the mannequin Skeleton, skinned to a handful of bones:
                           root (ground) -> pelvis -> spine_01/03/05 -> neck_01 -> head; markers rigid on hand_l/r, foot_l/r
  ABP_GSTN_GreyboxCapsule  parent UGSTN_GreyboxAnimInstance: Copy Pose From Mesh (attached parent = the hidden driver) -> GSTN Greybox Shape
  CHT_GSTN_SimpleLoco      chooser: parameter 0 = UGSTN_SimpleLocoAnimInstance (ReadWrite), 1 = GSTN_ChooserPlayerOutput (Write);
                           column LocoState + output columns (StartTime, PlayRate, BlendTime, reselect, warp targets); one row per state
                           with a GASP placeholder clip until the capsule clips exist (3A)
  ABP_GSTN_SimpleLoco      parent UGSTN_SimpleLocoAnimInstance: state machine "Loco" (one GSTN Chooser Player per state, transitions on
                           LocoState, no crossfade: the row's BlendTime is an inertial blend) -> Inertialization -> DefaultSlot ->
                           Orientation / Stride Warping (settings copied from the GASP stock ABP, alphas bound to the instance) -> Output

Why the capsule is on the mannequin Skeleton (reversible choice, docs/notes/greybox-capsula.md D1): Copy Pose From Mesh copies by bone
name and needs the same parent chain; a subset of the mannequin bones gives that with no IK Rig / Retargeter assets, and any clip made for
the capsule is a mannequin clip (owner: the simple clips stay valid for the real characters).

Steps: GREYBOX_STEPS="mesh,capsule_abp,chooser,simple_abp,warping" (default all). Dry run unless GSTN_SAVE=1 (Tools/CI/Save-Assets.ps1).
Idempotent: an existing asset is reused and only its missing parts are added; the chooser rows are rebuilt only when the chooser is new
(rows the owner added by hand are never touched). Report: Saved/Greybox/setup_greybox_assets.txt (one summary line printed).
Editor APIs that vary between engine versions (Geometry Script, chooser parameters) are probed; a missing one is reported with the names
that exist, so the next run can be fixed without guessing.
"""
import os
import time

import unreal

DIR = '/Game/GSTN/Greybox'
MESH_NAME = 'SK_GSTN_GreyboxCapsule'
CAPSULE_ABP_NAME = 'ABP_GSTN_GreyboxCapsule'
CHOOSER_NAME = 'CHT_GSTN_SimpleLoco'
SIMPLE_ABP_NAME = 'ABP_GSTN_SimpleLoco'
MANNEQUIN = '/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin'
STOCK_ABP = '/Game/GSTN/Locomotion/StockBase/ABP_GSTN_StockMover'
ANIMS = '/Game/Characters/UEFN_Mannequin/Animations'
OUT = unreal.Paths.project_saved_dir() + 'Greybox/setup_greybox_assets.txt'
SAVE = os.environ.get('GSTN_SAVE') == '1'
STEPS = [s.strip() for s in os.environ.get('GREYBOX_STEPS', 'mesh,capsule_abp,chooser,simple_abp,warping').split(',') if s.strip()]

# Capsule shape (cm): the mannequin is ~180 cm; the capsule reads as the body volume, the nose as the facing (mesh +Y = forward)
# Owner 2026-11-24 (run 2 note): floating a little above the ground, its top below the top of the collision capsule, chubbier.
# GREYBOX_SHAPE="radius,bottom,top" overrides (cm above the feet).
_shape = [float(v) for v in os.environ.get('GREYBOX_SHAPE', '34,14,156').split(',')]
CAPSULE_RADIUS = _shape[0]
CAPSULE_BOTTOM = _shape[1]
CAPSULE_HEIGHT = _shape[2] - _shape[1]
NOSE_LENGTH = 16.0
NOSE_RADIUS = 6.0
MARKER_RADIUS = {'hand_l': 10.0, 'hand_r': 10.0, 'foot_l': 8.0, 'foot_r': 8.0}
# Owner 2026-11-24 14:38: hand markers a bit bigger and red (material id 1, the nose's red)
RED_MARKERS = {'hand_l', 'hand_r'}
# Run 3 (owner 2026-11-24 14:38: "skirts or rolls of flesh" at the end of the dropkick instead of just compressing): the whole body and
# the nose are rigid on the pelvis, so the capsule only tilts and squashes as a block; the clips put the whole lean on the pelvis
# (make_capsule_clips.py PELVIS_LEAN_SHARE 1). GREYBOX_RIGID_BODY=0 brings back the blend along the spine chain.
RIGID_BODY = os.environ.get('GREYBOX_RIGID_BODY', '1') != '0'
BODY_CHAIN = ['root', 'pelvis', 'spine_01', 'spine_03', 'spine_05', 'neck_01', 'head']

STATES = ['Idle', 'RunStart', 'Run', 'RunStop', 'Skid', 'Air', 'Crouch']
# State -> (clip name searched under ANIMS or /Game, loop, blend time, reselect while active, orientation warp, stride warp)
PLACEHOLDER_ROWS = {
    'Idle': ('M_Neutral_Stand_Idle_Loop', True, 0.3, False, 0.0, 0.0),
    'RunStart': ('M_Neutral_Run_Start_F_Lfoot', False, 0.15, False, 1.0, 0.0),
    'Run': ('M_Neutral_Run_Loop_F', True, 0.25, False, 1.0, 1.0),
    'RunStop': ('M_Neutral_Run_Stop_F_Lfoot', False, 0.2, False, 0.0, 0.0),
    'Skid': ('SS_180_Turn_Root180', False, 0.15, False, 1.0, 0.0),
    'Air': ('M_Neutral_Jump_Loop_Fall', True, 0.2, False, 0.0, 0.0),
    'Crouch': ('Crouch_Idle_Loop', True, 0.25, False, 0.0, 0.0),
}

lines = []
errors = []
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.GSTN_AnimGraphAuthoringLibrary


def p(*a):
    lines.append(' '.join(str(x) for x in a))


def fail(msg):
    errors.append(msg)
    p('ERROR', msg)


def flush():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')


def v2(x, y):
    return unreal.Vector2D(x, y)


def save(path):
    if SAVE:
        p('saved', path, eal.save_asset(path, only_if_is_dirty=False))


def api_names(obj, words):
    return sorted(n for n in dir(obj) if any(w in n.lower() for w in words))


def call_first(owner, names, *args):
    """Calls the first of names that exists on owner (engine API names move between versions); None if none does."""
    for name in names:
        fn = getattr(owner, name, None)
        if fn:
            return fn(*args)
    fail('none of %s on %s; has %s' % (names, owner, api_names(owner, [names[0].split('_')[0]])))
    return None


class _Toolset:
    """GSTN_ChooserTableToolset UFUNCTIONs are toolset-only (not Blueprint/Python exposed): call them by name on the CDO."""

    def __getattr__(self, name):
        cdo = unreal.GSTN_ChooserTableToolset.get_default_object()
        method = ''.join(w.capitalize() for w in name.split('_'))
        return lambda *args: cdo.call_method(method, tuple(args))


def find_anim(name):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for root in (ANIMS, '/Game'):
        for data in reg.get_assets_by_path(root, recursive=True):
            if str(data.asset_name) == name or (name.startswith('Crouch') and name in str(data.asset_name)):
                asset = data.get_asset()
                if isinstance(asset, unreal.AnimSequenceBase):
                    return asset
    return None


# ---------------------------------------------------------------------------------------------------------------------------- mesh

def bone_heights(skeleton):
    """Component-space reference position of each bone the capsule uses."""
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    out = {}
    for bone in BODY_CHAIN + list(MARKER_RADIUS):
        t = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
        out[bone] = t.translation
    return out


def body_weights(z, chain_z):
    """Two-bone linear blend along the body chain by height. Run 2 (2026-11-24): everything under the pelvis is rigid on the pelvis, not
    blended to root: pinned to the ground the capsule bent like a banana and could never lie flat (dropkick) or sit low (crouch). The
    ground squash is procedural now (GSTN Greybox Shape scales the pelvis by its height over the reference)."""
    if RIGID_BODY or z <= chain_z[1]:
        return [('pelvis', 1.0)]
    for i in range(len(BODY_CHAIN) - 1):
        z0, z1 = chain_z[i], chain_z[i + 1]
        if z <= z1 or i == len(BODY_CHAIN) - 2:
            if z1 <= z0:
                return [(BODY_CHAIN[i + 1], 1.0)]
            a = min(max((z - z0) / (z1 - z0), 0.0), 1.0)
            return [(BODY_CHAIN[i], 1.0 - a), (BODY_CHAIN[i + 1], a)]
    return [(BODY_CHAIN[-1], 1.0)]


def step_mesh():
    path = '%s/%s' % (DIR, MESH_NAME)
    existing = None
    if eal.does_asset_exist(path) and os.environ.get('GREYBOX_REBUILD_MESH') == '1':
        # Rewrite in place (same asset, so the capsule ABP and the subsystem keep pointing at it). delete_asset + create does not work
        # here: the referenced asset stays loaded and the path still exists (desktop run 2, 2026-11-24)
        existing = eal.load_asset(path)
        p('mesh: rebuilding in place', path)
    if existing is None and eal.does_asset_exist(path):
        p('mesh: exists', path)
        return eal.load_asset(path)
    gs_names = ['GeometryScript_Primitives', 'GeometryScript_NewAssetUtils']
    if not all(hasattr(unreal, n) for n in gs_names):
        fail('Geometry Script not loaded (enable the GeometryScripting plugin, editor only); unreal has %s'
             % api_names(unreal, ['geometryscript_'])[:40])
        return None
    mannequin = eal.load_asset(MANNEQUIN)
    skeleton = mannequin.get_editor_property('skeleton') if mannequin else None
    if not skeleton:
        fail('mannequin mesh or skeleton missing: ' + MANNEQUIN)
        return None
    pos = bone_heights(skeleton)
    p('mesh: bone heights', {b: round(pos[b].z, 1) for b in BODY_CHAIN})

    prims = unreal.GeometryScript_Primitives
    opts = unreal.GeometryScriptPrimitiveOptions()
    mesh = unreal.DynamicMesh()
    # Owner 2026-11-24: grey capsule, red nose. Material id 0 = body and markers, 1 = nose (slot names SLOT_NAMES)
    mat_lib = None
    for lib_name in ('GeometryScript_Materials', 'GeometryScript_MeshMaterialFunctions', 'GeometryScript_MeshMaterials'):
        mat_lib = getattr(unreal, lib_name, None)
        # UE's Python name of EnableMaterialIDs is enable_material_i_ds (the IDs split into I + Ds)
        enable = mat_lib and (getattr(mat_lib, 'enable_material_ids', None) or getattr(mat_lib, 'enable_material_i_ds', None))
        if enable:
            enable(mesh)
            break
    else:
        p('mesh: WARNING no enable_material_ids; material libs:', api_names(unreal, ['geometryscript_mat']))
    nose_opts = unreal.GeometryScriptPrimitiveOptions()
    try:
        opts.set_editor_property('material_id', 0)
        nose_opts.set_editor_property('material_id', 1)
    except Exception as exc:
        p('mesh: WARNING primitive options have no material_id:', exc)
    parts = []  # (first vertex id, vertex count after, kind)

    def vcount():
        return unreal.GeometryScript_MeshQueries.get_vertex_count(mesh)

    start = vcount()
    # Capsule floating CAPSULE_BOTTOM above the ground: origin at its base
    body_t = unreal.Transform(location=unreal.Vector(0, 0, CAPSULE_BOTTOM))
    prims.append_capsule(mesh, opts, body_t, CAPSULE_RADIUS, CAPSULE_HEIGHT - 2 * CAPSULE_RADIUS, 6, 16, 8,
                         unreal.GeometryScriptPrimitiveOriginMode.BASE)
    parts.append((start, vcount(), 'body'))
    # Nose: a cone on the face, pointing forward (+Y), at the head's height
    start = vcount()
    # Under the dome (the capsule top is below the real head now), weighted to the head so it turns with it
    nose_z = CAPSULE_BOTTOM + CAPSULE_HEIGHT - CAPSULE_RADIUS * 0.9
    nose_t = unreal.Transform(location=unreal.Vector(0, CAPSULE_RADIUS - 2.0, nose_z), rotation=unreal.Rotator(0, 0, -90))
    prims.append_cone(mesh, nose_opts, nose_t, NOSE_RADIUS, 1.0, NOSE_LENGTH, 12, 1, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    parts.append((start, vcount(), 'pelvis' if RIGID_BODY else 'head'))
    for bone, radius in MARKER_RADIUS.items():
        start = vcount()
        prims.append_sphere_lat_long(mesh, nose_opts if bone in RED_MARKERS else opts, unreal.Transform(location=pos[bone]), radius, 8, 12,
                                     unreal.GeometryScriptPrimitiveOriginMode.CENTER)
        parts.append((start, vcount(), bone))

    # Skin weights: probe the bone weight library (names changed across 5.x)
    bw = getattr(unreal, 'GeometryScript_BoneWeights', None) or getattr(unreal, 'GeometryScript_MeshBoneWeights', None)
    if not bw:
        fail('no Geometry Script bone weight library; unreal has %s' % api_names(unreal, ['boneweight', 'skin']))
        return None
    p('mesh: bone weight API', api_names(bw, ['bone', 'weight', 'skin']))
    chain_z = [0.0] + [pos[b].z for b in BODY_CHAIN[1:]]
    bone_index = {}
    ref = unreal.AnimPoseExtensions.get_bone_names(unreal.AnimPoseExtensions.get_reference_pose(skeleton))
    for i, name in enumerate(ref):
        bone_index[str(name)] = i
    # The skin weight attribute and the bone hierarchy must exist on the dynamic mesh before weights are set, or the new asset
    # fails with 'LOD 0 has no skin weight attributes' (UE 5.9)
    bw.copy_bones_from_skeleton(skeleton, mesh)
    bw.mesh_create_bone_weights(mesh)
    set_fn = getattr(bw, 'set_vertex_bone_weights', None)
    if set_fn:
        for first, last, kind in parts:
            for vid in range(first, last):
                v = unreal.GeometryScript_MeshQueries.get_vertex_position(mesh, vid)[0]
                pairs = body_weights(v.z, chain_z) if kind == 'body' else [(kind, 1.0)]
                weights = [unreal.GeometryScriptBoneWeight(bone_index=bone_index[b], weight=w) for b, w in pairs if w > 0.0]
                set_fn(mesh, vid, weights)
        p('mesh: manual weights on', vcount(), 'vertices')
    else:
        smooth = getattr(bw, 'compute_smooth_bone_weights', None)
        if not smooth:
            fail('no way to set bone weights; library has %s' % dir(bw))
            return None
        smooth(mesh, skeleton, unreal.GeometryScriptSmoothBoneWeightsOptions())
        p('mesh: WARNING smooth auto weights (set_vertex_bone_weights missing): the capsule may follow the legs and arms')

    if existing is not None:
        outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(
            mesh, existing, unreal.GeometryScriptCopyMeshToAssetOptions(), unreal.GeometryScriptMeshWriteLOD())
        p('mesh: rewritten', path, 'vertices', vcount(), 'outcome', outcome)
        if 'SUCCESS' not in str(outcome).upper():
            fail('copy_mesh_to_skeletal_mesh: %s' % (outcome,))
            return None
        assign_materials(existing)
        save(path)
        return existing
    new_utils = unreal.GeometryScript_NewAssetUtils
    options = unreal.GeometryScriptCreateNewSkeletalMeshAssetOptions()
    result = new_utils.create_new_skeletal_mesh_asset_from_mesh(mesh, skeleton, path, options)
    asset = result[0] if isinstance(result, tuple) else result
    if not asset:
        fail('create_new_skeletal_mesh_asset_from_mesh returned nothing: %s' % (result,))
        return None
    p('mesh: created', path, 'vertices', vcount())
    assign_materials(asset)
    save(path)
    return asset


BASE_MATERIAL = '/Engine/BasicShapes/BasicShapeMaterial'
MATERIALS = [('Body', 'MI_GSTN_Greybox_Grey', unreal.LinearColor(0.35, 0.35, 0.37, 1.0)),
             ('Nose', 'MI_GSTN_Greybox_Red', unreal.LinearColor(0.8, 0.04, 0.03, 1.0))]


PARENT_NAME = 'M_GSTN_Greybox'


def make_parent():
    """Our own parent: BasicShapeMaterial lacks the SkeletalMesh usage flag, so in game the capsule fell back to the default checker
    material (desktop run 2). A one-parameter material ('Color' -> Base Color) with bUsedWithSkeletalMesh."""
    path = '%s/%s' % (DIR, PARENT_NAME)
    if eal.does_asset_exist(path):
        mat = eal.load_asset(path)
    else:
        mat = asset_tools.create_asset(PARENT_NAME, DIR, unreal.Material, unreal.MaterialFactoryNew())
        mel = unreal.MaterialEditingLibrary
        param = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -300, 0)
        param.set_editor_property('parameter_name', 'Color')
        param.set_editor_property('default_value', unreal.LinearColor(0.35, 0.35, 0.37, 1.0))
        mel.connect_material_property(param, '', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 200)
        rough.set_editor_property('r', 0.6)
        mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    try:
        unreal.MaterialEditingLibrary.recompile_material(mat)
    except Exception as exc:  # headless -nullrhi: no compile here, the editor compiles the shaders when it loads the material
        p('material: not compiled here (%s); compiled on load' % str(exc)[:80])
    save(path)
    return mat


def make_material(name, color):
    path = '%s/%s' % (DIR, name)
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else None
    if not mi:
        mi = asset_tools.create_asset(name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', make_parent())
    # BasicShapeMaterial's tint parameter is 'Color'
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, 'Color', color)
    try:
        unreal.MaterialEditingLibrary.update_material_instance(mi)
    except Exception as exc:
        p('material instance: not updated here (%s)' % str(exc)[:80])
    save(path)
    return mi


def assign_materials(asset):
    """Slot 0 grey (body, markers), slot 1 red (nose), in material id order."""
    try:
        slots = []
        for slot_name, mi_name, color in MATERIALS:
            sm = unreal.SkeletalMaterial()
            sm.set_editor_property('material_interface', make_material(mi_name, color))
            sm.set_editor_property('material_slot_name', slot_name)
            slots.append(sm)
        asset.set_editor_property('materials', slots)
        p('mesh: materials', [s.get_editor_property('material_slot_name') for s in slots])
    except Exception as exc:
        fail('mesh: materials not assigned: %s' % exc)


# --------------------------------------------------------------------------------------------------------------------- capsule ABP

def make_abp(name, parent_class, skeleton):
    path = '%s/%s' % (DIR, name)
    if eal.does_asset_exist(path):
        return eal.load_asset(path), False
    factory = unreal.AnimBlueprintFactory()
    factory.set_editor_property('parent_class', parent_class)
    factory.set_editor_property('target_skeleton', skeleton)
    return asset_tools.create_asset(name, DIR, unreal.AnimBlueprint, factory), True


def compile_bp(bp):
    report, ok = lib.compile_with_report(bp)
    for line in report.splitlines():
        if line.startswith('Error') or line.startswith('Warning'):
            (fail if line.startswith('Error') else p)('compile %s: %s' % (bp.get_name(), line))
    return ok


def step_capsule_abp(mesh):
    if not mesh:
        fail('capsule_abp: no capsule mesh')
        return None
    bp, created = make_abp(CAPSULE_ABP_NAME, unreal.GSTN_GreyboxAnimInstance, mesh.get_editor_property('skeleton'))
    graph = lib.find_graph(bp, 'AnimGraph')
    copy = lib.find_or_add_pose_node(graph, unreal.AnimGraphNode_CopyPoseFromMesh, v2(-600, 0))
    if not lib.find_anim_graph_nodes(bp, unreal.GSTN_AnimGraphNode_GreyboxShape):
        lib.connect_to_graph_output(copy)
        lib.insert_pose_node_before_output(bp, unreal.GSTN_AnimGraphNode_GreyboxShape)
    ok = lib.set_node_property(copy, 'bUseAttachedParent', 'True') and lib.set_node_property(copy, 'bCopyCurves', 'True')
    if not ok:
        fail('capsule_abp: could not set Copy Pose From Mesh properties')
    p('capsule_abp:', 'created' if created else 'updated', bp.get_path_name(), 'compile', compile_bp(bp))
    save(bp.get_path_name().split('.')[0])
    return bp


# ------------------------------------------------------------------------------------------------------------------------- chooser

def step_chooser():
    path = '%s/%s' % (DIR, CHOOSER_NAME)
    if eal.does_asset_exist(path):
        p('chooser: exists (rows untouched)', path)
        return eal.load_asset(path)
    factory_cls = getattr(unreal, 'ChooserTableFactory', None)
    if not factory_cls:
        fail('no ChooserTableFactory; unreal has %s' % api_names(unreal, ['chooser'])[:40])
        return None
    cht = asset_tools.create_asset(CHOOSER_NAME, DIR, unreal.ChooserTable, factory_cls())
    ed = getattr(unreal, 'ChooserTableEditorLibrary', None)
    if not ed:
        fail('no ChooserTableEditorLibrary; unreal has %s' % api_names(unreal, ['chooser'])[:40])
        return cht

    # Signature: 0 = the simple loco anim instance (ReadWrite: output columns write the warp targets), 1 = the player output (Write)
    p('chooser: editor library parameter API', api_names(ed, ['param']))
    sim_class = unreal.GSTN_SimpleLocoAnimInstance.static_class()
    out_struct = unreal.GSTN_ChooserPlayerOutput.static_struct()
    for index, (kind, value, direction) in enumerate((('ContextObjectTypeClass', sim_class, 'ReadWrite'),
                                                     ('ContextObjectTypeStruct', out_struct, 'Write'))):
        if ed.get_num_parameters(cht) <= index:
            call_first(ed, ['add_parameter'], cht, kind)
        call_first(ed, ['set_parameter_property'], cht, index, 'Class' if index == 0 else 'Struct', value.get_path_name())
        call_first(ed, ['set_parameter_property'], cht, index, 'Direction', direction)
    for index in range(ed.get_num_parameters(cht)):
        p('chooser: parameter', index, ed.get_parameter_type(cht, index), ed.get_parameter_property(cht, index, ''))

    tool = _Toolset()
    state_col = tool.add_chooser_column(cht, 'EnumColumn')
    tool.set_chooser_column_input(cht, state_col, '', 0, ['LocoState'])
    outputs = [('StartTime', 1), ('PlayRate', 1), ('BlendTime', 1), ('OrientationWarpTarget', 0), ('StrideWarpTarget', 0)]
    out_cols = {}
    for prop, param in outputs:
        col = tool.add_chooser_column(cht, 'OutputFloatColumn')
        tool.set_chooser_column_input(cht, col, '', param, [prop])
        out_cols[prop] = col
    reselect_col = tool.add_chooser_column(cht, 'OutputBoolColumn')
    tool.set_chooser_column_input(cht, reselect_col, '', 1, ['bReselectWhileActive'])

    for state in STATES:
        clip_name, loop, blend, reselect, orient, stride = PLACEHOLDER_ROWS[state]
        clip = find_anim(clip_name)
        if not clip:
            fail('chooser: placeholder clip %s for %s not found (row left empty)' % (clip_name, state))
        row = tool.add_chooser_row(cht, 'AssetChooser', len(tool.get_chooser_table(cht).rows), clip)
        tool.set_chooser_cell(cht, state_col, row, '(Value=%s)' % state)
        tool.set_chooser_cell(cht, out_cols['StartTime'], row, '0.0')
        tool.set_chooser_cell(cht, out_cols['PlayRate'], row, '1.0')
        tool.set_chooser_cell(cht, out_cols['BlendTime'], row, str(blend))
        tool.set_chooser_cell(cht, out_cols['OrientationWarpTarget'], row, str(orient))
        tool.set_chooser_cell(cht, out_cols['StrideWarpTarget'], row, str(stride))
        tool.set_chooser_cell(cht, reselect_col, row, 'True' if reselect else 'False')
        p('chooser: row', row, state, clip.get_path_name() if clip else None)
    info = tool.get_chooser_table(cht)
    p('chooser: columns', len(info.columns), 'rows', len(info.rows))
    save(path)
    return cht


# ---------------------------------------------------------------------------------------------------------------------- simple ABP

def step_simple_abp(cht):
    mannequin = eal.load_asset(MANNEQUIN)
    skeleton = mannequin.get_editor_property('skeleton') if mannequin else None
    if not skeleton or not cht:
        fail('simple_abp: needs the mannequin skeleton and the chooser')
        return None
    bp, created = make_abp(SIMPLE_ABP_NAME, unreal.GSTN_SimpleLocoAnimInstance, skeleton)
    graph = lib.find_graph(bp, 'AnimGraph')
    machine = lib.find_or_add_state_machine(graph, 'Loco', v2(-900, 0))
    nodes = {}
    for i, state in enumerate(STATES):
        node = lib.find_or_add_state(machine, state, v2(300 * (i % 4), 260 * (i // 4)))
        nodes[state] = node
        player = lib.find_or_add_pose_node(lib.get_state_graph(node), unreal.GSTN_AnimGraphNode_ChooserPlayer, v2(-320, 0))
        lib.connect_to_graph_output(player)
        if not lib.set_node_property(player, 'Chooser', cht.get_path_name()):
            fail('simple_abp: chooser not set in state ' + state)
    lib.set_entry_state(machine, nodes['Idle'])
    # Every state to every other on LocoState == target; crossfade 0 (the chosen row's BlendTime is an inertial blend)
    for src in STATES:
        for dst in STATES:
            if src == dst:
                continue
            transition = lib.find_or_add_transition(nodes[src], nodes[dst], 0.0)
            if not lib.set_transition_rule_enum_equals(transition, ['LocoState'], dst):
                fail('simple_abp: rule %s -> %s' % (src, dst))
    if not lib.find_anim_graph_nodes(bp, unreal.AnimGraphNode_Inertialization):
        lib.connect_to_graph_output(machine)
        lib.insert_pose_node_before_output(bp, unreal.AnimGraphNode_Inertialization)
        lib.insert_pose_node_before_output(bp, unreal.AnimGraphNode_Slot)
    p('simple_abp:', 'created' if created else 'updated', bp.get_path_name(), 'compile', compile_bp(bp))
    save(bp.get_path_name().split('.')[0])
    return bp


def step_warping(bp):
    """Orientation + Stride Warping after the slot, settings from the GASP stock ABP, alphas and inputs bound to the instance."""
    if not bp:
        fail('warping: no simple ABP')
        return
    stock = eal.load_asset(STOCK_ABP)
    specs = [
        ('AnimGraphNode_OrientationWarping', {'Alpha': ['OrientationWarpAlpha'], 'LocomotionAngle': ['LocomotionAngle']}),
        ('AnimGraphNode_StrideWarping', {'Alpha': ['StrideWarpAlpha'], 'LocomotionSpeed': ['LocomotionSpeed']}),
    ]
    for class_name, pins in specs:
        cls = getattr(unreal, class_name, None)
        if not cls:
            fail('warping: no %s (AnimationWarping editor module)' % class_name)
            continue
        if not lib.find_anim_graph_nodes(bp, cls):
            lib.insert_pose_node_before_output(bp, cls)
        node = lib.find_anim_graph_nodes(bp, cls)[0] if lib.find_anim_graph_nodes(bp, cls) else None
        if not node:
            fail('warping: %s not placed' % class_name)
            continue
        if stock and not lib.copy_node_settings_from(node, stock):
            p('warping: no %s in the stock ABP to copy (bones must be set by hand)' % class_name)
        # Graph mode: the node reads LocomotionAngle / LocomotionSpeed (our clips carry no root motion to measure)
        lib.set_node_property(node, 'Mode', 'Graph')
        for pin, path in pins.items():
            if not lib.bind_node_pin_to_property(node, pin, path):
                fail('warping: bind %s.%s' % (class_name, pin))
    p('warping: compile', compile_bp(bp))
    save(bp.get_path_name().split('.')[0])


def run():
    p('setup_greybox_assets', time.strftime('%Y-%m-%d %H:%M:%S'), 'SAVE' if SAVE else 'DRY RUN', 'steps', STEPS)
    mesh = step_mesh() if 'mesh' in STEPS else (eal.load_asset('%s/%s' % (DIR, MESH_NAME)) if eal.does_asset_exist('%s/%s' % (DIR, MESH_NAME)) else None)
    if 'capsule_abp' in STEPS:
        step_capsule_abp(mesh)
    cht = step_chooser() if 'chooser' in STEPS else (eal.load_asset('%s/%s' % (DIR, CHOOSER_NAME)) if eal.does_asset_exist('%s/%s' % (DIR, CHOOSER_NAME)) else None)
    bp = step_simple_abp(cht) if 'simple_abp' in STEPS else (eal.load_asset('%s/%s' % (DIR, SIMPLE_ABP_NAME)) if eal.does_asset_exist('%s/%s' % (DIR, SIMPLE_ABP_NAME)) else None)
    if 'warping' in STEPS:
        step_warping(bp)
    p('RESULT:', 'PASS' if not errors else 'FAIL (%d errors)' % len(errors))
    flush()
    print('[setup_greybox_assets] %s, report %s' % ('PASS' if not errors else 'FAIL %d: %s' % (len(errors), errors[0][:160]), OUT))


try:
    run()
except Exception as exc:  # the report must exist even on a crash, with the trace
    import traceback
    fail('exception: %s' % exc)
    p(traceback.format_exc())
    flush()
    print('[setup_greybox_assets] EXCEPTION %s, report %s' % (exc, OUT))
