# Builds the run skid and run start anim layers (polish M4; owner playtest 2026-11-17: "the skid clip I asked for never plays") and plugs them into
# the character's anim set collection. Idempotent: every step is "find or add"; Blueprints are backed up to Saved/Backups/GAS before a change.
#
# Run headless (editor closed; needs GSTNAnimationEditor + GSTNCombatNP built with UGSTN_SkidAnimInstance / GSTN Clip At Time):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_skid_anim_layer.py" -unattended -nullrhi
# Env: SKID_CLIP (default Ported57/Skid/Skid_Revised), RUN_START_CLIP (default none: the run start layer stays off until the owner picks a clip).
# Prints at most 5 lines; full log in the editor log ([setup_skid_anim_layer]).
#
# Steps (assets in /Game/Blueprints/Animation):
#  1. ALI_GSTN_Skid / ALI_GSTN_RunStart and the main ABP seam slots 2/3: setup_anim_layers.py (already run in M0; re-run here, no change if done).
#  2. ABP_GSTN_SkidLayer: parent GSTN_SkidAnimInstance, implements ALI_GSTN_Skid; its SkidLayer graph = one GSTN Clip At Time node -> output.
#     Class default SkidClip = SKID_CLIP. The timing (stretch to the rest frame, exit rate) is C++ + class defaults (StopFraction, ExitPlayRate).
#  3. ABP_GSTN_RunStartLayer: parent GSTN_RunStartAnimInstance, implements ALI_GSTN_RunStart, same one-node graph; RunStartClip = RUN_START_CLIP.
#  4. DA_AnimSet_Skid (Anim.Set.Skid), DA_AnimSet_RunStart (Anim.Set.RunStart), one rule each in DA_AnimSetCollection_SandboxCharacter.
import os, sys
import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_wall_anim_layer as wall  # noqa: E402
import setup_anim_layers as layers  # noqa: E402

ANIM_DIR = wall.ANIM_DIR
SKID_CLIP = os.environ.get('SKID_CLIP', f'{ANIM_DIR}/Ported57/Skid/Skid_Revised')
RUN_START_CLIP = os.environ.get('RUN_START_CLIP', '')

# name -> (parent class, interface, layer graph, clip property, clip path, set asset, set tag)
LAYERS = [
    ('ABP_GSTN_SkidLayer', 'GSTN_SkidAnimInstance', 'ALI_GSTN_Skid', 'SkidLayer', 'skid_clip', SKID_CLIP, 'DA_AnimSet_Skid', 'Anim.Set.Skid'),
    ('ABP_GSTN_RunStartLayer', 'GSTN_RunStartAnimInstance', 'ALI_GSTN_RunStart', 'RunStartLayer', 'run_start_clip', RUN_START_CLIP,
     'DA_AnimSet_RunStart', 'Anim.Set.RunStart'),
]

lib = unreal.GSTN_AnimGraphAuthoringLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
lines = []
errors = []


def log(msg):
    lines.append(msg)
    unreal.log(f'[setup_skid_anim_layer] {msg}')


def fail(msg):
    errors.append(msg)
    lines.append('ERROR ' + msg)
    unreal.log_error(f'[setup_skid_anim_layer] {msg}')


def ensure_layer(name, parent, ali, graph_name, clip_prop, clip_path, skeleton):
    path = f'{ANIM_DIR}/{name}'
    bp = wall.load(path)
    if not bp:
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property('parent_class', getattr(unreal, parent))
        factory.set_editor_property('target_skeleton', skeleton)
        bp = asset_tools.create_asset(name, ANIM_DIR, unreal.AnimBlueprint, factory)
        if not bp:
            fail(f'could not create {path}')
            return None
        log(f'created {path} (parent {parent})')
    before_bytes = wall.snapshot(path)
    lib.implement_layer_interface(bp, ali)
    wall.compile_and_check(bp)
    graph = lib.find_graph(bp, graph_name)
    if not graph:
        fail(f'{name} has no {graph_name} graph after implementing {ali.get_name()}')
        return bp
    node = lib.find_or_add_pose_node(graph, unreal.GSTN_AnimGraphNode_ClipAtTime.static_class(), wall.v2(-300, 0))
    if not node or not lib.connect_to_graph_output(node):
        fail(f'{name}: Clip At Time node not wired to the {graph_name} output')
        return bp
    if not wall.compile_and_check(bp):
        fail(f'{name}: compile failed')
        return bp
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    clip = unreal.load_asset(clip_path) if clip_path else None
    if clip_path and not clip:
        fail(f'{name}: clip {clip_path} not found')
    if clip is not None or not clip_path:
        cdo.set_editor_property(clip_prop, clip)
    wall.backup_snapshot(path, before_bytes)
    wall.save(bp)
    log(f'{name}: {graph_name} = Clip At Time, {clip_prop} = {clip.get_path_name() if clip else "none"}')
    return bp


def ensure_set(name, set_tag, layer_bp):
    path = f'{ANIM_DIR}/{name}'
    anim_set = wall.load(path)
    if not anim_set:
        anim_set = asset_tools.create_asset(name, ANIM_DIR, unreal.GSTN_AnimSetDefinition, unreal.DataAssetFactory())
        log(f'created {path}')
    anim_set.set_editor_property('set_tag', wall.tag(set_tag))
    anim_set.set_editor_property('layer_class', unreal.BlueprintEditorLibrary.generated_class(layer_bp))
    wall.save(anim_set)
    return anim_set


def run():
    if not layers.run():
        fail('setup_anim_layers.run() reported errors (interfaces / main ABP seam)')
        return False
    skeleton = unreal.load_asset(wall.MAIN_ABP).get_editor_property('target_skeleton')
    rules = []
    for name, parent, ali_name, graph_name, clip_prop, clip_path, set_name, set_tag in LAYERS:
        ali = wall.load(f'{ANIM_DIR}/{ali_name}')
        if not ali:
            fail(f'{ali_name} missing')
            continue
        bp = ensure_layer(name, parent, ali, graph_name, clip_prop, clip_path, skeleton)
        if bp:
            rules.append((ensure_set(set_name, set_tag, bp), None, 0))
    wall.ensure_collection(rules)
    return not errors


if __name__ == '__main__':
    try:
        ok = run()
    except Exception as e:  # noqa: BLE001
        fail(str(e))
        ok = False
    tail = [l for l in lines if l.startswith('ERROR')][-4:] or lines[-4:]
    for line in tail:
        print(line)
    print(f'ESITO: {"OK" if ok else "FAIL"} skid layer + run start layer ({len(errors)} errors)')
