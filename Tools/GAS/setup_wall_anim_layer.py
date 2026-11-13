# Builds the wall anim layer and plugs it into the character (wall-anim.md "Layer graph", anim-layers.md A2 and part B). Idempotent: running it
# again changes nothing (every step is "find or add"; a Blueprint is backed up and saved only when a step changes it).
#
# Run headless (editor closed; the graph authoring needs the GSTNAnimationEditor module of the GSTNAnimation plugin, built):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_wall_anim_layer.py" -unattended -nullrhi
# or from the editor Python console (close the Blueprints first):
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_wall_anim_layer as s; s.run()
# --check only prints the graphs (UGSTN_AnimGraphAuthoringLibrary.DescribeGraphs) and the compiler messages, changing nothing.
# --variant also makes the heavy-build variant test (review item 2e, below).
#
# Steps (assets in /Game/Blueprints/Animation unless noted):
#  1. ALI_GSTN_Wall: anim layer interface with the layer WallLayer.
#  2. ABP_GSTN_WallLayer: parent GSTN_WallAnimInstance, target skeleton of the main ABP, implements ALI_GSTN_Wall. Its WallLayer graph:
#       state machine WallPhases (entry Enter):
#         Enter --(WallAnim.Phase == Active)--> Loop     Loop  --(Phase == Exit)--> Exit
#         Enter --(Phase == Exit)--> Exit                Exit  --(Phase == Enter)--> Enter   (re-grab during the exit)
#         Loop  --(Phase == Enter)--> Enter  (only when ExitDuration is 0: Active -> None -> Enter never shows an Exit)
#       Loop plays the inner state machine SlideStates (entry Idle): Idle / SlideLeft / SlideRight / Skid, a transition between every pair
#       with the rule WallAnim.SlideState == <target>. Every rule is a Property Access (thread safe) + enum ==. Placeholder clips: CLIPS.
#  3. /Game/Blueprints/SandboxCharacter_Mover_ABP (shared, backed up): reparented to GSTN_CharacterAnimInstance, implements ALI_GSTN_Wall (its
#     own WallLayer graph stays empty: it is only the default the linked class replaces, and the blend never shows it because
#     bOverrideLayerActive is false without a linked wall layer), and the seam in front of the Pose History collector:
#       Procedural_FullBody -> [False] Blend Poses by bool (bOverrideLayerActive, OverrideBlendTime, inertialization) -> Inertialization ->
#       Linked Anim Layer WallLayer -> [True]                                                                               Pose History -> Output
#     In front of the Pose History node (not the Output Pose) so motion matching records the pose that was shown, and after the GASP
#     procedural layers (feet, full body), which must not touch the wall pose. The Inertialization node serves the blend's request: the Dead
#     Blending node of GASP is upstream and would not see it.
#  4. DA_AnimSet_Wall (GSTN_AnimSetDefinition: SetTag Anim.Set.Wall, LayerClass ABP_GSTN_WallLayer) and DA_AnimSetCollection_SandboxCharacter
#     (one rule: DA_AnimSet_Wall, always).
#  5. /Game/Blueprints/SandboxCharacter_Mover (shared, backed up): a GSTN_AnimSetComponent "AnimSets" with DefaultCollection = that collection.
#  (--variant) ABP_GSTN_WallLayer_Heavy (child of ABP_GSTN_WallLayer, same graph; it overrides the placeholder clips through the child's
#     asset overrides: Idle -> M_Relaxed_Jump_Loop_Fall), DA_AnimSet_Wall_Heavy (Anim.Set.Wall, that layer) and a second rule in the collection:
#     DA_AnimSet_Wall_Heavy when Anim.Trait.Build.Heavy, priority 10. Adding Anim.Trait.Build.Heavy to the component's DefaultCosmeticTags switches
#     the layer (not done here: the default character stays on the base layer).
# Blueprints are copied to Saved/Backups/GAS (timestamped) before they are changed.
import os, shutil, sys, time
import unreal

ANIM_DIR = '/Game/Blueprints/Animation'
ALI_NAME = 'ALI_GSTN_Wall'
LAYER_ABP_NAME = 'ABP_GSTN_WallLayer'
SET_NAME = 'DA_AnimSet_Wall'
COLLECTION_NAME = 'DA_AnimSetCollection_SandboxCharacter'
HEAVY_ABP_NAME = 'ABP_GSTN_WallLayer_Heavy'
HEAVY_SET_NAME = 'DA_AnimSet_Wall_Heavy'
MAIN_ABP = '/Game/Blueprints/SandboxCharacter_Mover_ABP'
SANDBOX_CHAR = '/Game/Blueprints/SandboxCharacter_Mover'
LAYER = 'WallLayer'
SET_TAG = 'Anim.Set.Wall'
HEAVY_TRAIT = 'Anim.Trait.Build.Heavy'
COMPONENT_NAME = 'AnimSets'
INSERT_BEFORE = 'AnimGraphNode_PoseSearchHistoryCollector'

ANIMS = '/Game/Characters/UEFN_Mannequin/Animations'
# State -> (placeholder clip, loop). GASP sample clips on the same skeleton, chosen to make each state visible, not to look right
CLIPS = {
    'Enter': (f'{ANIMS}/Jump/M_Neutral_Jump_F_Land_Stand_Light_Lfoot', False),  # impact absorbed on grabbing the wall
    'Exit': (f'{ANIMS}/Jump/M_Neutral_Jump_F_Start_Stand_Lfoot', False),        # push off
    'Idle': (f'{ANIMS}/Jump/M_Neutral_Jump_Loop_Fall', True),                    # still / sliding straight down
    'SlideLeft': (f'{ANIMS}/Walk/M_Neutral_Walk_Loop_LL', True),                 # strafe left
    'SlideRight': (f'{ANIMS}/Walk/M_Neutral_Walk_Loop_RR', True),                # strafe right
    'Skid': (f'{ANIMS}/Slide/M_Neutral_Slide_FootOut_Loop', True),               # fast braking slide
}
HEAVY_IDLE_CLIP = f'{ANIMS}/Jump/M_Relaxed_Jump_Loop_Fall'

# Outer machine: (from, to, phase value, crossfade s)
PHASE_TRANSITIONS = [
    ('Enter', 'Loop', 'Active', 0.15),
    ('Loop', 'Exit', 'Exit', 0.15),
    ('Enter', 'Exit', 'Exit', 0.1),
    ('Exit', 'Enter', 'Enter', 0.1),
    ('Loop', 'Enter', 'Enter', 0.1),
]
SLIDE_STATES = ['Idle', 'SlideLeft', 'SlideRight', 'Skid']
SLIDE_CROSSFADE = 0.2

lib = unreal.GSTN_AnimGraphAuthoringLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary
errors = []


def log(msg):
    unreal.log(f'[setup_wall_anim_layer] {msg}')


def fail(msg):
    errors.append(msg)
    unreal.log_error(f'[setup_wall_anim_layer] {msg}')


def backup(asset_path):
    """One timestamped copy per run, in Saved/Backups/GAS (an older backup of the same asset is never overwritten)."""
    src = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + asset_path[len('/Game/'):] + '.uasset')
    if not os.path.exists(src):
        return
    dst_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backups', 'GAS')
    os.makedirs(dst_dir, exist_ok=True)
    stem = os.path.splitext(os.path.basename(src))[0]
    dst = os.path.join(dst_dir, f'{stem}.{time.strftime("%Y%m%d-%H%M%S")}.uasset')
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        log(f'backup {src} -> {dst}')


def compile_and_check(bp):
    """Compiles and logs the warnings, errors and the summary line (the Info lines of the GASP graph are noise)."""
    report, ok = lib.compile_with_report(bp)
    for line in report.splitlines():
        if line.startswith('Info:'):
            continue
        (fail if line.startswith('Error') else log)(f'compile {bp.get_name()}: {line}')
    return ok


def asset_file(asset_path):
    return unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + asset_path[len('/Game/'):] + '.uasset')


def snapshot(asset_path):
    """The asset's bytes on disk before a step that may change it (written to Saved/Backups/GAS only if it does)."""
    src = asset_file(asset_path)
    return open(src, 'rb').read() if os.path.exists(src) else None


def backup_snapshot(asset_path, data):
    if data is None:
        return
    dst_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backups', 'GAS')
    os.makedirs(dst_dir, exist_ok=True)
    stem = os.path.splitext(os.path.basename(asset_file(asset_path)))[0]
    dst = os.path.join(dst_dir, f'{stem}.{time.strftime("%Y%m%d-%H%M%S")}.uasset')
    if not os.path.exists(dst):
        open(dst, 'wb').write(data)
        log(f'backup {asset_path} -> {dst}')


def save(asset):
    eal.save_loaded_asset(asset, only_if_is_dirty=False)
    log(f'saved {asset.get_path_name()}')


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def v2(x, y):
    return unreal.Vector2D(x, y)


# 1. Layer interface ------------------------------------------------------------------------------------------------------------------

def ensure_interface():
    path = f'{ANIM_DIR}/{ALI_NAME}'
    ali = load(path)
    changed = False
    if not ali:
        factory = unreal.AnimLayerInterfaceFactory()
        ali = asset_tools.create_asset(ALI_NAME, ANIM_DIR, unreal.AnimBlueprint, factory)
        if not ali:
            fail(f'could not create {path}')
            return None
        log(f'created {path}')
        changed = True
    if not lib.find_graph(ali, LAYER):
        changed |= lib.add_anim_layer(ali, LAYER)
        log(f'{ALI_NAME}: layer {LAYER} added')
    if changed:
        compile_and_check(ali)
        save(ali)
    return ali


# 2. Wall layer Blueprint -------------------------------------------------------------------------------------------------------------

def ensure_layer_blueprint(ali, skeleton):
    path = f'{ANIM_DIR}/{LAYER_ABP_NAME}'
    layer_bp = load(path)
    if not layer_bp:
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property('parent_class', unreal.GSTN_WallAnimInstance)
        factory.set_editor_property('target_skeleton', skeleton)
        layer_bp = asset_tools.create_asset(LAYER_ABP_NAME, ANIM_DIR, unreal.AnimBlueprint, factory)
        if not layer_bp:
            fail(f'could not create {path}')
            return None
        log(f'created {path} (parent GSTN_WallAnimInstance)')
    before_bytes = snapshot(path)
    before = lib.describe_graphs(layer_bp)
    lib.implement_layer_interface(layer_bp, ali)
    compile_and_check(layer_bp)  # the layer graph and WallAnim must be on the skeleton class before the rules resolve

    graph = lib.find_graph(layer_bp, LAYER)
    if not graph:
        fail(f'{LAYER_ABP_NAME} has no {LAYER} graph after implementing {ALI_NAME}')
        return layer_bp

    phases = lib.find_or_add_state_machine(graph, 'WallPhases', v2(-420, 0))
    lib.connect_to_graph_output(phases)
    states = {
        'Enter': lib.find_or_add_state(phases, 'Enter', v2(250, 0)),
        'Loop': lib.find_or_add_state(phases, 'Loop', v2(600, 0)),
        'Exit': lib.find_or_add_state(phases, 'Exit', v2(425, 260)),
    }
    lib.set_entry_state(phases, states['Enter'])
    for src, dst, phase, crossfade in PHASE_TRANSITIONS:
        transition = lib.find_or_add_transition(states[src], states[dst], crossfade)
        if not lib.set_transition_rule_enum_equals(transition, ['WallAnim', 'Phase'], phase):
            fail(f'rule {src} -> {dst} (Phase == {phase}) not set')

    for name in ('Enter', 'Exit'):
        set_state_clip(layer_bp, states[name], name)

    slide = lib.find_or_add_state_machine(lib.get_state_graph(states['Loop']), 'SlideStates', v2(-420, 0))
    lib.connect_to_graph_output(slide)
    slide_states = {name: lib.find_or_add_state(slide, name, v2(250 + 350 * (i % 2), 260 * (i // 2)))
                    for i, name in enumerate(SLIDE_STATES)}
    lib.set_entry_state(slide, slide_states['Idle'])
    for name, state in slide_states.items():
        set_state_clip(layer_bp, state, name)
    for src in SLIDE_STATES:
        for dst in SLIDE_STATES:
            if src == dst:
                continue
            transition = lib.find_or_add_transition(slide_states[src], slide_states[dst], SLIDE_CROSSFADE)
            if not lib.set_transition_rule_enum_equals(transition, ['WallAnim', 'SlideState'], dst):
                fail(f'rule {src} -> {dst} (SlideState == {dst}) not set')

    if lib.describe_graphs(layer_bp) == before and before_bytes is not None:
        log(f'{LAYER_ABP_NAME}: graph already as wanted')
        return layer_bp
    if compile_and_check(layer_bp):
        backup_snapshot(path, before_bytes)
        save(layer_bp)
    return layer_bp


def set_state_clip(bp, state, name):
    clip_path, loop = CLIPS[name]
    clip = unreal.load_asset(clip_path)
    if not clip:
        fail(f'placeholder clip {clip_path} not found')
        return
    player = lib.find_or_add_sequence_player(lib.get_state_graph(state), clip, loop, v2(-320, 0))
    lib.connect_to_graph_output(player)


# 3. Main anim Blueprint --------------------------------------------------------------------------------------------------------------

def main_abp_state(abp):
    text = lib.describe_graphs(abp)
    head = text.splitlines()[0] if text else ''
    return {
        'reparented': '(parent GSTN_CharacterAnimInstance)' in head,
        'implements': f'implements {ALI_NAME}_C' in text,
        'blend': f'layer {ALI_NAME}_C::{LAYER}' in text,
    }


def ensure_main_abp(ali):
    abp = unreal.load_asset(MAIN_ABP)
    state = main_abp_state(abp)
    if all(state.values()):
        log(f'{MAIN_ABP}: already reparented, implements {ALI_NAME} and blends {LAYER}')
        return abp
    backup(MAIN_ABP)
    if not state['reparented']:
        unreal.BlueprintEditorLibrary.reparent_blueprint(abp, unreal.GSTN_CharacterAnimInstance)
        log(f'{MAIN_ABP}: reparented to GSTN_CharacterAnimInstance')
        compile_and_check(abp)
    if not state['implements']:
        lib.implement_layer_interface(abp, ali)
        log(f'{MAIN_ABP}: implements {ALI_NAME}')
        compile_and_check(abp)
    if not state['blend']:
        if lib.insert_override_layer_blend(abp, ali, LAYER, 'bOverrideLayerActive', 'OverrideBlendTime', True, INSERT_BEFORE):
            log(f'{MAIN_ABP}: Blend Poses by bool + Linked Anim Layer {LAYER} + Inertialization in front of {INSERT_BEFORE}')
        else:
            fail(f'{MAIN_ABP}: the override layer blend was not inserted')
    if compile_and_check(abp):
        save(abp)
    else:
        fail(f'{MAIN_ABP} does not compile: NOT saved (backup in Saved/Backups/GAS)')
    return abp


# 4. Anim set data --------------------------------------------------------------------------------------------------------------------

def tag(name):
    t = unreal.GameplayTag()
    t.import_text(f'(TagName="{name}")')
    return t


def tag_query_any(name):
    """FGameplayTagQuery 'any tags match <name>' through its text form (the builder is not exposed to Python)."""
    q = unreal.GameplayTagQuery()
    q.import_text(f'(TokenStreamVersion=0,TagDictionary=((TagName="{name}")),QueryTokenStream=(0,1,1,1,0),UserDescription="",AutoDescription="")')
    return q


def ensure_set(name, layer_bp):
    path = f'{ANIM_DIR}/{name}'
    anim_set = load(path)
    if not anim_set:
        anim_set = asset_tools.create_asset(name, ANIM_DIR, unreal.GSTN_AnimSetDefinition, unreal.DataAssetFactory())
        log(f'created {path}')
    layer_class = unreal.BlueprintEditorLibrary.generated_class(layer_bp)
    changed = False
    if str(anim_set.get_editor_property('set_tag').export_text()).find(SET_TAG) < 0:
        anim_set.set_editor_property('set_tag', tag(SET_TAG))
        changed = True
    current = anim_set.get_editor_property('layer_class')
    if not current or layer_class.get_path_name() not in str(current):
        anim_set.set_editor_property('layer_class', layer_class)
        changed = True
    if changed:
        save(anim_set)
    return anim_set


def ensure_collection(rules):
    """rules: list of (anim set, tag query or None, priority). Rules for the same set are matched by set; missing ones are appended."""
    path = f'{ANIM_DIR}/{COLLECTION_NAME}'
    collection = load(path)
    if not collection:
        collection = asset_tools.create_asset(COLLECTION_NAME, ANIM_DIR, unreal.GSTN_AnimSetCollection, unreal.DataAssetFactory())
        log(f'created {path}')
    existing = list(collection.get_editor_property('rules'))
    names = [str(r.get_editor_property('set')) for r in existing]
    changed = False
    for anim_set, query, priority in rules:
        if any(anim_set.get_path_name() in n for n in names):
            continue
        rule = unreal.GSTN_AnimSetRule()
        rule.set_editor_property('set', anim_set)
        if query:
            rule.set_editor_property('when', query)
        rule.set_editor_property('priority', priority)
        existing.append(rule)
        changed = True
        log(f'{COLLECTION_NAME}: rule {anim_set.get_name()} (priority {priority})')
    if changed:
        collection.set_editor_property('rules', existing)
        save(collection)
    return collection


# 5. Character component --------------------------------------------------------------------------------------------------------------

def blueprint_components(bp):
    out = []
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
        obj = sdl.get_object_for_blueprint(sdl.get_data(handle), bp)
        if obj:
            out.append((handle, obj))
    return out


def ensure_component(collection):
    bp = unreal.load_asset(SANDBOX_CHAR)
    component = next((o for _, o in blueprint_components(bp) if isinstance(o, unreal.GSTN_AnimSetComponent)), None)
    if component and component.get_editor_property('default_collection') == collection:
        log(f'{SANDBOX_CHAR}: {COMPONENT_NAME} already uses {COLLECTION_NAME}')
        return
    backup(SANDBOX_CHAR)
    if not component:
        root = subobjects.k2_gather_subobject_data_for_blueprint(bp)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.GSTN_AnimSetComponent, blueprint_context=bp)
        handle, failure = subobjects.add_new_subobject(params)
        if not failure.is_empty():
            fail(f'could not add the anim set component: {failure}')
            return
        subobjects.rename_subobject(handle, COMPONENT_NAME)
        component = next((o for _, o in blueprint_components(bp) if isinstance(o, unreal.GSTN_AnimSetComponent)), None)
        log(f'{SANDBOX_CHAR}: added {COMPONENT_NAME} (GSTN_AnimSetComponent)')
    component.set_editor_property('default_collection', collection)
    if compile_and_check(bp):
        save(bp)


# Variant test (review item 2e) -------------------------------------------------------------------------------------------------------

def ensure_heavy_variant(layer_bp, skeleton):
    path = f'{ANIM_DIR}/{HEAVY_ABP_NAME}'
    heavy = load(path)
    if not heavy:
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property('parent_class', unreal.BlueprintEditorLibrary.generated_class(layer_bp))
        factory.set_editor_property('target_skeleton', skeleton)
        heavy = asset_tools.create_asset(HEAVY_ABP_NAME, ANIM_DIR, unreal.AnimBlueprint, factory)
        if not heavy:
            fail(f'could not create {path}')
            return None
        log(f'created {path} (child of {LAYER_ABP_NAME})')
    if lib.set_child_asset_override(heavy, 'Idle', unreal.load_asset(HEAVY_IDLE_CLIP)):
        log(f'{HEAVY_ABP_NAME}: Idle plays {HEAVY_IDLE_CLIP}')
    else:
        fail(f'{HEAVY_ABP_NAME}: the Idle override was not set')
    if compile_and_check(heavy):
        save(heavy)
    return heavy


# -------------------------------------------------------------------------------------------------------------------------------------

def check():
    for path in (f'{ANIM_DIR}/{ALI_NAME}', f'{ANIM_DIR}/{LAYER_ABP_NAME}', f'{ANIM_DIR}/{HEAVY_ABP_NAME}', MAIN_ABP):
        bp = load(path)
        if not bp:
            log(f'check: {path} missing')
            continue
        compile_and_check(bp)
        text = lib.describe_graphs(bp)
        if path == MAIN_ABP:
            log(f'check: {path} {main_abp_state(bp)}')
        else:
            for line in text.splitlines():
                log(f'check: {line}')


def run(variant=False):
    main_abp = unreal.load_asset(MAIN_ABP)
    skeleton = main_abp.get_editor_property('target_skeleton')
    ali = ensure_interface()
    if not ali:
        return False
    layer_bp = ensure_layer_blueprint(ali, skeleton)
    ensure_main_abp(ali)
    rules = [(ensure_set(SET_NAME, layer_bp), None, 0)]
    if variant:
        heavy = ensure_heavy_variant(layer_bp, skeleton)
        if heavy:
            rules.append((ensure_set(HEAVY_SET_NAME, heavy), tag_query_any(HEAVY_TRAIT), 10))
    collection = ensure_collection(rules)
    ensure_component(collection)
    if errors:
        log(f'FINISHED WITH {len(errors)} ERROR(S):')
        for e in errors:
            log(f'  {e}')
    else:
        log('finished, no errors')
    return not errors


if __name__ == '__main__':
    if '--check' in sys.argv:
        check()
    else:
        run(variant='--variant' in sys.argv)
