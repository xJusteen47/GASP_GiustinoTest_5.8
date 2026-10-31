# Physics Asset bodies -> UGSTN_HurtboxSetDefinition (M4 H13a). Editor Python only.
#
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import make_hurtbox_set as m
#   m.from_physics_asset('/Game/.../PHYS_Mannequin', '/Game/GASNPPTest/Combat/HBS_FromPhysics',
#                        tag_map={'head': 'Combat.Hurtbox.Head', 'foot': 'Combat.Hurtbox.Feet'}, default_tag='Combat.Hurtbox.Torso')
# or headless:  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/make_hurtbox_set.py <physics asset> <out asset>"
#
# One hurtbox per collision primitive of the physics asset, named after its body (a second primitive of the same body gets a numeric suffix).
# The tag comes from tag_map: the first key that is a substring of the body name (case-insensitive) wins, else default_tag. Priority comes from
# priority_map the same way (default 0). The shape is the primitive's, as a sphere / capsule / box in the local space of the hurtbox
# (capsule extent = radius, radius, half height including the hemispheres, like a hit shape).
#
# Limit (kept small on purpose: M5 replaces the tool): a body lives in the space of its bone, a hurtbox in the space of the actor. Pass
# bone_transforms ({bone name: unreal.Transform in the space of the actor}) to place the primitives; without it the hurtbox is placed at the
# primitive's offset from the actor origin and the tool warns: fix the offsets by hand in the details panel. The set's weight scales,
# thresholds and reaction caps are left at their defaults (set them in the asset or through the keyword arguments).
import unreal

# Substring of the body name -> tag. Matches the leaves of Config/Tags/GSTNCombat.ini; a project passes its own
STANDARD_TAG_MAP = {
    'head': 'Combat.Hurtbox.Head',
    'neck': 'Combat.Hurtbox.Head',
    'foot': 'Combat.Hurtbox.Feet',
    'ball': 'Combat.Hurtbox.Feet',
}


def log(msg):
    unreal.log(f'[make_hurtbox_set] {msg}')


def _pick(table, name, default):
    lowered = name.lower()
    for key, value in table.items():
        if key.lower() in lowered:
            return value
    return default


def _tag_container(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{name}")')
    return unreal.GameplayTagLibrary.make_gameplay_tag_container_from_array([tag])


def _shape(kind, extent, local):
    shape = unreal.GSTN_HitShape()
    shape.set_editor_property('type', kind)
    shape.set_editor_property('extent', extent)
    shape.set_editor_property('local', local)
    return shape


def primitives(physics_asset):
    """Yields (bone, shape type, extent, local Transform in the space of the bone) for every primitive of the physics asset. The body setups of a
    physics asset are not reflected to Python, so the list comes from UGSTN_PhysicsAssetUtils.list_primitives (GSTNCombatCore)"""
    for primitive in unreal.GSTN_PhysicsAssetUtils.list_primitives(physics_asset):
        shape = primitive.get_editor_property('shape')
        yield str(primitive.get_editor_property('bone')), shape.get_editor_property('type'), shape.get_editor_property('extent'), shape.get_editor_property('local')


def from_physics_asset(physics_path, out_path, tag_map=None, default_tag='Combat.Hurtbox.Torso', priority_map=None,
                       bone_transforms=None, weight_scale=1.0, stagger_threshold=None, max_reaction=None):
    """Writes (or rewrites) the hurtbox set at out_path from the physics asset. Returns the asset, or None when the physics asset is not found"""
    phys = unreal.load_asset(physics_path)
    if phys is None:
        log(f'ERROR: no physics asset at {physics_path}')
        return None
    tag_map = STANDARD_TAG_MAP if tag_map is None else tag_map
    priority_map = priority_map or {}
    folder, name = out_path.rsplit('/', 1)
    asset = unreal.load_asset(out_path)
    if asset is None:
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.GSTN_HurtboxSetDefinition, unreal.DataAssetFactory())
        log(f'created {out_path}')

    boxes, used = [], {}
    for bone, kind, extent, local in primitives(phys):
        count = used.get(bone, 0)
        used[bone] = count + 1
        box_name = bone if count == 0 else f'{bone}_{count + 1}'
        if bone_transforms and bone in bone_transforms:
            local = local * bone_transforms[bone]
        box = unreal.GSTN_Hurtbox()
        box.set_editor_property('name', box_name)
        box.set_editor_property('bone', bone)
        box.set_editor_property('shape', _shape(kind, extent, local))
        box.set_editor_property('tags', _tag_container(_pick(tag_map, bone, default_tag)))
        box.set_editor_property('priority', _pick(priority_map, bone, 0))
        reaction = box.get_editor_property('reaction_scale')
        reaction.set_editor_property('weight_scale', weight_scale)
        box.set_editor_property('reaction_scale', reaction)
        boxes.append(box)
    if not bone_transforms:
        log('WARNING: no bone_transforms, the primitives sit at their offset from the actor origin; place them by hand')
    asset.set_editor_property('hurtboxes', boxes)
    if stagger_threshold is not None:
        threshold = asset.get_editor_property('stagger_threshold')
        threshold.set_editor_property('value', stagger_threshold)
        asset.set_editor_property('stagger_threshold', threshold)
    if max_reaction is not None:
        asset.set_editor_property('max_reaction', max_reaction)
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    log(f'{out_path}: {len(boxes)} hurtboxes from {physics_path}')
    return asset


if __name__ == '__main__':
    import sys
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    if len(args) >= 2:
        from_physics_asset(args[0], args[1])
    else:
        log('usage: make_hurtbox_set.py <physics asset path> <output asset path>')
