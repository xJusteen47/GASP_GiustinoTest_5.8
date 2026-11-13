# Gives /Game/Blueprints/SandboxCharacter_Mover (BP_GASNPP_TestCharacter inherits it) the traversal presentation pieces.
# Idempotent: running it again changes nothing. The Blueprint is copied to Saved/Backups/GAS before the first change.
#
# Run headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_traversal_presentation.py" -unattended -nullrhi
# or from the editor Python console:  import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_traversal_presentation as s; s.run()
#
# What it does:
#  1. DepthMarker: a UGSTN_DepthMarkerComponent (GSTNDepthMarkerRuntime) with default settings (active). Inert on a dedicated server.
#  2. GSTNCamera: a UGSTN_CameraComponent (GSTNCameraRuntime), INACTIVE by default (bAutoActivate = false), so the GASP camera keeps
#     working: the pawn's view is taken from the first ACTIVE camera component, and an inactive component neither ticks nor
#     supplies a view. Nothing else (PlayerCameraManager, settings) is changed.
#     To turn it on in PIE: select the character and call Activate on the GSTNCamera component (Blueprint node "Activate" on the
#     component, or Details > Activation > Auto Activate for a permanent switch; also deactivate the GASP camera component or
#     it may still win, as the first active camera component found is used). The master switch (Project Settings > Plugins > GSTN
#     Camera > Enabled, console variable gstn.Camera.Enable, default 1) is already on and needs no change. Feed it look input with
#     AddLookInput, or set the plugin look action and mapping context in the settings.
#  3. T5 surface filter: SurfaceFilter.RequiredBehaviours = "any tags match Surface.Behaviour.WallSlide" on the wall slide mode(s)
#     and the Falling mode's entry transition(s) that lead to a slide mode, and "... WallClimb" on the climb mode(s) and the
#     transition(s) to them. bUnknownSurfacesPass stays true, so a collider no one marked behaves exactly as before.
#     Modes and transitions are told apart by name (contains "Slide" / "Climb"; for a mode, its dictionary key in the Mover component;
#     for a transition, TargetModeName).
import os, shutil, re
import unreal

SANDBOX_CHAR = '/Game/Blueprints/SandboxCharacter_Mover'
SLIDE_TAG = 'Surface.Behaviour.WallSlide'
CLIMB_TAG = 'Surface.Behaviour.WallClimb'

subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log(f'[setup_traversal_presentation] {msg}')


def backup(asset_path):
    src = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + asset_path[len('/Game/'):] + '.uasset')
    dst_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backups', 'GAS')
    os.makedirs(dst_dir, exist_ok=True)
    dst = os.path.join(dst_dir, os.path.basename(src))
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        log(f'backup {src} -> {dst}')


def blueprint_components(bp):
    out = []
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
        data = sdl.get_data(handle)
        obj = sdl.get_object_for_blueprint(data, bp)
        if obj:
            out.append((handle, obj))
    return out


def add_component(bp, cls, name):
    """Adds a component of cls named `name` under the root when the Blueprint has none of that class. Returns the object or None."""
    existing = next((o for _, o in blueprint_components(bp) if isinstance(o, cls)), None)
    if existing:
        return existing, False
    backup(SANDBOX_CHAR)
    root = subobjects.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=cls, blueprint_context=bp)
    handle, fail = subobjects.add_new_subobject(params)
    if not fail.is_empty():
        log(f'ERROR adding {name}: {fail}')
        return None, False
    subobjects.rename_subobject(handle, name)
    log(f'added {name} ({cls.static_class().get_name()})')
    return next((o for _, o in blueprint_components(bp) if isinstance(o, cls)), None), True


def any_tag_query(tag_name):
    """FGameplayTagQuery 'any tags match <tag_name>', through its text form (the builder is not exposed to Python).
    Stream: [version 0, has root 1, AnyTagsMatch 1, tag count 1, tag index 0]."""
    q = unreal.GameplayTagQuery()
    q.import_text(f'(TokenStreamVersion=0,TagDictionary=((TagName="{tag_name}")),QueryTokenStream=(0,1,1,1,0),UserDescription="",AutoDescription="")')
    return q


def query_tags(query):
    return re.findall(r'TagName="([^"]+)"', query.export_text())


def set_filter(obj, tag_name, label):
    """Sets SurfaceFilter.RequiredBehaviours on a mode or transition when it is not already that single tag. Returns changed."""
    flt = obj.get_editor_property('surface_filter')
    if query_tags(flt.get_editor_property('required_behaviours')) == [tag_name] and flt.get_editor_property('unknown_surfaces_pass'):
        return False
    flt.set_editor_property('required_behaviours', any_tag_query(tag_name))
    flt.set_editor_property('unknown_surfaces_pass', True)
    obj.set_editor_property('surface_filter', flt)
    log(f'{label}: SurfaceFilter.RequiredBehaviours = any({tag_name}), UnknownSurfacesPass = true')
    return True


def tag_for_name(name):
    n = str(name)
    if 'Slid' in n:
        return SLIDE_TAG
    if 'Climb' in n:
        return CLIMB_TAG
    return None


def setup_surface_filters(bp):
    mover = next((o for _, o in blueprint_components(bp) if isinstance(o, unreal.MoverComponent)), None)
    if not mover:
        log(f'ERROR: no Mover component on {SANDBOX_CHAR}')
        return False
    changed = False
    seen = 0
    for mode_name, mode in mover.get_editor_property('movement_modes').items():
        if isinstance(mode, unreal.GSTN_WallMovementModeBase):
            tag = tag_for_name(mode_name)
            if tag:
                seen += 1
                if _would_change(mode, tag):
                    backup(SANDBOX_CHAR)
                changed |= set_filter(mode, tag, f'mode {mode_name}')
            else:
                log(f'mode {mode_name} is a wall mode that is neither slide nor climb: left alone')
        for i, tr in enumerate(mode.get_editor_property('transitions')):
            if isinstance(tr, unreal.GSTN_WallEntryTransition):
                tag = tag_for_name(tr.get_editor_property('target_mode_name'))
                if tag:
                    seen += 1
                    if _would_change(tr, tag):
                        backup(SANDBOX_CHAR)
                    changed |= set_filter(tr, tag, f'{mode_name} transition {i} -> {tr.get_editor_property("target_mode_name")}')
    log(f'surface filters: {seen} modes/transitions examined')
    return changed


def _would_change(obj, tag_name):
    flt = obj.get_editor_property('surface_filter')
    return not (query_tags(flt.get_editor_property('required_behaviours')) == [tag_name]
                and flt.get_editor_property('unknown_surfaces_pass'))


def run():
    bp = unreal.load_asset(SANDBOX_CHAR)
    if not bp:
        log(f'ERROR: {SANDBOX_CHAR} not found')
        return False
    changed = False

    _, c = add_component(bp, unreal.GSTN_DepthMarkerComponent, 'DepthMarker')
    changed |= c

    cam, c = add_component(bp, unreal.GSTN_CameraComponent, 'GSTNCamera')
    changed |= c
    if cam and cam.get_editor_property('auto_activate'):
        backup(SANDBOX_CHAR)
        cam.set_editor_property('auto_activate', False)
        log('GSTNCamera.bAutoActivate = false (inactive: the GASP camera keeps the view)')
        changed = True
    others = [o.get_name() for _, o in blueprint_components(bp) if isinstance(o, unreal.CameraComponent) and o is not cam]
    log(f'other camera components on the Blueprint: {others}')

    changed |= setup_surface_filters(bp)

    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        log('compiled and saved')
    log(f'traversal presentation changed: {changed}')
    return changed


if __name__ == '__main__':
    run()
