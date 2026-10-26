# Builds the wall limb contact Control Rig and puts it at the end of the wall layer (limb-contact-ik.md section 5). Idempotent: every step is
# "find or add"; the layer Blueprint is backed up to Saved/Backups/GAS before it is changed.
#
# Needs: the GSTNAnimation plugin built (modules GSTNAnimationRig and GSTNAnimationEditor), and the wall layer made by setup_wall_anim_layer.py.
# Run from the editor Python console (close ABP_GSTN_WallLayer first):
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_wall_limb_contact as s; s.run()
# or headless:
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_wall_limb_contact.py" -unattended -nullrhi
# --check only prints the rig graph and the layer graphs, changing nothing.
#
# Steps:
#  1. CR_GSTN_WallLimbContact (Control Rig, bones of the main ABP's skeleton). Variable WallLimbContact (GSTN_WallLimbContactState, public)
#     and a Forwards Solve of four GSTN Limb Contact IK nodes in a chain:
#       hand_l: upperarm_l, lowerarm_l, hand_l, Surface = WallLimbContact.HandLeft,  Weight = WallLimbContact.HandWeight, HAND settings
#       hand_r: upperarm_r, lowerarm_r, hand_r, Surface = WallLimbContact.HandRight, Weight = WallLimbContact.HandWeight, HAND settings
#       foot_l: thigh_l,    calf_l,     foot_l, Surface = WallLimbContact.FootLeft,  Weight = WallLimbContact.FootWeight, FOOT settings
#       foot_r: thigh_r,    calf_r,     foot_r, Surface = WallLimbContact.FootRight, Weight = WallLimbContact.FootWeight, FOOT settings
#  2. ABP_GSTN_WallLayer, graph WallLayer: a Control Rig node (that rig) between the WallPhases state machine and the Output Pose, its
#     WallLimbContact pin shown and fed by a get of the layer instance's WallLimbContact (filled on the game thread). Not InputMapping:
#     in 5.8 it maps rig variables to animation curves only.
#
# Checked on 5.8 (2026-10-26): a rig made from a skeleton by script has an empty graph, so the Forwards Solve node is added here; the
# node's rig is the FControlRigAssetStrongReference member ControlRigAssetReference. Every failure is logged with the step.
import sys
import unreal

ANIM_DIR = '/Game/Blueprints/Animation'
RIG_NAME = 'CR_GSTN_WallLimbContact'
LAYER_ABP = f'{ANIM_DIR}/ABP_GSTN_WallLayer'
MAIN_ABP = '/Game/Blueprints/SandboxCharacter_Mover_ABP'
LAYER = 'WallLayer'

VARIABLE = 'WallLimbContact'
VARIABLE_TYPE_PATH = '/Script/GASP_Giustino2.GSTN_WallLimbContactState'
UNIT_PATH = '/Script/GSTNAnimationRig.GSTN_RigUnit_LimbContactIK'
BEGIN_EXECUTION_PATH = '/Script/ControlRig.RigUnit_BeginExecution'
CONTROL_RIG_NODE = '/Script/ControlRigDeveloper.AnimGraphNode_ControlRig'

# Node name -> (root, joint, effector, surface member, weight member, settings)
HAND = '(ContactOffset=3.0,AttachDistance=8.0,ReleaseDistance=25.0,bRequireReach=True,MaxReachRatio=1.0,BlendInSpeed=12.0,BlendOutSpeed=8.0,RotationAlpha=0.6,ContactAxis=(X=0.0,Y=1.0,Z=0.0))'
FOOT = '(ContactOffset=7.0,AttachDistance=8.0,ReleaseDistance=25.0,bRequireReach=True,MaxReachRatio=1.0,BlendInSpeed=12.0,BlendOutSpeed=8.0,RotationAlpha=0.4,ContactAxis=(X=0.0,Y=1.0,Z=0.0))'
LIMBS = [
    ('HandLeftContact', 'upperarm_l', 'lowerarm_l', 'hand_l', 'HandLeft', 'HandWeight', HAND),
    ('HandRightContact', 'upperarm_r', 'lowerarm_r', 'hand_r', 'HandRight', 'HandWeight', HAND),
    ('FootLeftContact', 'thigh_l', 'calf_l', 'foot_l', 'FootLeft', 'FootWeight', FOOT),
    ('FootRightContact', 'thigh_r', 'calf_r', 'foot_r', 'FootRight', 'FootWeight', FOOT),
]

lib = unreal.GSTN_AnimGraphAuthoringLibrary
eal = unreal.EditorAssetLibrary
errors = []


def log(msg):
    unreal.log(f'[setup_wall_limb_contact] {msg}')


def fail(msg):
    errors.append(msg)
    unreal.log_error(f'[setup_wall_limb_contact] {msg}')


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def save(asset):
    eal.save_loaded_asset(asset, only_if_is_dirty=False)
    log(f'saved {asset.get_path_name()}')


# 1. Control Rig ----------------------------------------------------------------------------------------------------------------------

def ensure_rig(skeleton):
    path = f'{ANIM_DIR}/{RIG_NAME}'
    rig = load(path)
    if not rig:
        # Creates the rig with the skeleton's bones imported into its hierarchy, as "Create Control Rig" on a skeleton does
        rig = unreal.ControlRigBlueprintFactory.create_control_rig_from_skeletal_mesh_or_skeleton(skeleton)
        if not rig:
            fail(f'could not create a Control Rig from {skeleton.get_path_name()}')
            return None
        created = rig.get_path_name().split('.')[0]
        if created != path:
            eal.rename_asset(created, path)
            rig = load(path)
        log(f'created {path}')

    if not any(str(v.name) == VARIABLE for v in rig.get_member_variables()):
        # C++ type of a struct variable: the struct's C++ name; the type object is resolved from its path
        rig.add_member_variable(VARIABLE, VARIABLE_TYPE_PATH, True, False, '')
        log(f'variable {VARIABLE}')

    controller = rig.get_controller_by_name('RigVMModel')
    graph = controller.get_graph()
    names = {str(n.get_name()) for n in graph.get_nodes()}

    # The Forwards Solve event. In 5.8 a rig made from a skeleton by script starts with an empty graph, so it is added when missing
    event = next((n for n in graph.get_nodes() if 'BeginExecution' in str(n.get_name())), None)
    if not event:
        event = controller.add_unit_node_from_struct_path(BEGIN_EXECUTION_PATH, 'Execute', unreal.Vector2D(0, 0), 'BeginExecution')
        if not event:
            fail('could not add the Forwards Solve (BeginExecution) node to the rig')
            return rig
        log('node BeginExecution')

    getter = 'GetWallLimbContact'
    if getter not in names:
        controller.add_variable_node_from_object_path(VARIABLE, 'FGSTN_WallLimbContactState', VARIABLE_TYPE_PATH, True, '',
                                                      unreal.Vector2D(-400, 300), getter)

    previous_exec = f'{event.get_name()}.ExecuteContext'
    for index, (node, root, joint, effector, surface, weight, settings) in enumerate(LIMBS):
        if node not in names:
            if not controller.add_unit_node_from_struct_path(UNIT_PATH, 'Execute', unreal.Vector2D(300 + index * 350, 0), node):
                fail(f'could not add {node} ({UNIT_PATH})')
                continue
            log(f'node {node}')
        for pin, value in (('Root', f'(Type=Bone,Name="{root}")'), ('Joint', f'(Type=Bone,Name="{joint}")'),
                           ('Effector', f'(Type=Bone,Name="{effector}")'), ('Settings', settings)):
            if not controller.set_pin_default_value(f'{node}.{pin}', value, True):
                fail(f'default {node}.{pin} = {value}')
        for source, target in ((previous_exec, f'{node}.ExecuteContext'),
                               (f'{getter}.Value.{surface}', f'{node}.Surface'),
                               (f'{getter}.Value.{weight}', f'{node}.Weight')):
            pin = graph.find_pin(target)
            if pin and not pin.get_linked_source_pins():
                if not controller.add_link(source, target):
                    fail(f'link {source} -> {target}')
        previous_exec = f'{node}.ExecuteContext'

    report_compile(rig)
    save(rig)
    return rig


def report_compile(blueprint):
    report, ok = lib.compile_with_report(blueprint)
    for line in report.splitlines():
        if not line.startswith('Info:'):
            (fail if line.startswith('Error') else log)(f'compile {blueprint.get_name()}: {line}')
    if not ok:
        fail(f'{blueprint.get_name()} does not compile')


# 2. Control Rig node in the wall layer -----------------------------------------------------------------------------------------------

def ensure_layer_node(rig):
    layer_bp = load(LAYER_ABP)
    if not layer_bp:
        fail(f'{LAYER_ABP} missing: run setup_wall_anim_layer.py first')
        return
    graph = lib.find_graph(layer_bp, LAYER)
    rig_class = rig.generated_class() if callable(getattr(rig, 'generated_class', None)) else rig.get_editor_property('generated_class')
    # 5.8: the node's rig is an FControlRigAssetStrongReference (ControlRigClass is deprecated); a Blueprint rig goes in its legacy
    # BlueprintRigClass member
    members = {
        'ControlRigAssetReference': f'(BlueprintRigClass="{rig_class.get_path_name()}")',
        # InputMapping maps rig variables to animation curves (floats), not to instance properties: cleared, the struct goes in by pin
        'InputMapping': '()',
    }
    node = lib.find_or_insert_pose_node_before_output(graph, CONTROL_RIG_NODE, members)
    if not node:
        fail('Control Rig node not placed in the wall layer')
        return
    # The rig's public variable shown as a pin on the node, fed by the layer instance's property of the same name each update
    if not lib.expose_pin_from_variable(node, VARIABLE, VARIABLE):
        fail(f'Control Rig node pin {VARIABLE} not fed by the layer property {VARIABLE}')
    report_compile(layer_bp)
    save(layer_bp)


def run(check=False):
    errors.clear()
    main_abp = load(MAIN_ABP)
    skeleton = main_abp.get_editor_property('target_skeleton') if main_abp else None
    if not skeleton:
        fail(f'no skeleton from {MAIN_ABP}')
        return False
    if check:
        rig = load(f'{ANIM_DIR}/{RIG_NAME}')
        layer_bp = load(LAYER_ABP)
        if rig:
            graph = rig.get_controller_by_name('RigVMModel').get_graph()
            log(f'rig variables {[f"{v.name}: {v.cpp_type}" for v in rig.get_member_variables()]}')
            for n in graph.get_nodes():
                log(f'rig node {n.get_name()}')
            for link in graph.get_links():
                log(f'rig link {link.get_pin_path_representation()}')
        if layer_bp:
            log(lib.describe_graphs(layer_bp))
        return True
    rig = ensure_rig(skeleton)
    if rig:
        ensure_layer_node(rig)
    log('done' if not errors else f'{len(errors)} error(s)')
    return not errors


if __name__ == '__main__':
    run(check='--check' in sys.argv)
