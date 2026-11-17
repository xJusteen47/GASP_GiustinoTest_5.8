# Ground contact correction setup (owner 2026-11-17: reactions must not sink into the floor, the animation is not replaced).
#  1. Creates or updates the settings asset DA_GroundContact_Default (bones from the C++ defaults, rules written here: knockdown and tumble
#     by tag, reaction montages found by name).
#  2. Inserts the GSTN Ground Contact Fix node right in front of the Output Pose of the Mover ABP and points it at the asset.
# Idempotent: the node is not inserted twice, rules are replaced by name. Backs the ABP up first, writes the graph before/after.
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GASNPPTest/ground_contact_setup.py" -unattended -nullrhi
# Prints at most 5 lines; full log Saved/GroundContact/ground_contact_setup.log.
import os
import shutil

import unreal

ABP = '/Game/Blueprints/SandboxCharacter_Mover_ABP'
DA_DIR = '/Game/Blueprints/Data'
DA_NAME = 'DA_GroundContact_Default'
# Montage names that count as "reaction on the floor" (any montage whose name contains one of these, under /Game)
MONTAGE_KEYS = ['TechBounce', 'GroundHit', 'GroundBounce', 'Knockdown', 'Lying', 'GetUp']
TAG_RULES = [
    ('Knockdown (lying, get-up)', ['Combat.State.Knockdown']),
    ('Tumble (ground bounce, landing)', ['Combat.State.Tumble', 'Combat.Mover.Tumble']),
]
MONTAGE_RULE = 'Reaction montages (tech bounce, ground hit)'
# Aerial landing-lag clips (owner playtest 2026-11-17: the down air's legs went into the floor during its final landing lag; the poses stay as
# approved, only the penetration is removed). AM_OverlordDAirLand{LightTuckR4,HeavyTuckR3,HeavyR2,LightR3,...}; 'AirLand' also catches the
# landing clips of the other aerials when they get one, 'LandingLag' any clip named after the action
LANDING_KEYS = ['AirLand', 'LandingLag']
LANDING_RULE = 'Aerial landing lag montages'

# GROUND_CONTACT_RULES_ONLY=1: update the rules of the settings asset only (the node is already in the ABP since F4)
ONLY_RULES = os.environ.get('GROUND_CONTACT_RULES_ONLY', '') == '1'
proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
out_dir = os.path.join(proj, 'Saved', 'GroundContact')
os.makedirs(out_dir, exist_ok=True)
lines = []


def log(msg):
    lines.append(msg)
    unreal.log(f'[ground_contact_setup] {msg}')


def find_montages(keys=None):
    keys = keys or MONTAGE_KEYS
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = []
    for data in registry.get_assets_by_path('/Game', recursive=True):
        if str(data.asset_class_path.asset_name) != 'AnimMontage':
            continue
        name = str(data.asset_name)
        if any(key.lower() in name.lower() for key in keys):
            found.append(str(data.package_name))
    return sorted(found)


def settings_asset():
    path = f'{DA_DIR}/{DA_NAME}'
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path), False
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset(DA_NAME, DA_DIR, unreal.GSTN_GroundContactSettings, unreal.DataAssetFactory())
    return asset, True


def run():
    da, created = settings_asset()
    for name, tags in TAG_RULES:
        unknown = da.set_rule(name, [unreal.Name(t) for t in tags], [], 1.0)
        if unknown:
            log(f'WARN rule "{name}": unknown tags {list(unknown)} (plugin off?)')
    montage_paths = find_montages()
    montages = [unreal.load_asset(p) for p in montage_paths]
    da.set_rule(MONTAGE_RULE, [], [m for m in montages if m], 1.0)
    landing_paths = [p for p in find_montages(LANDING_KEYS) if p not in montage_paths]
    landing = [unreal.load_asset(p) for p in landing_paths]
    da.set_rule(LANDING_RULE, [], [m for m in landing if m], 1.0)
    unreal.EditorAssetLibrary.save_loaded_asset(da, False)
    with open(os.path.join(out_dir, 'montages.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(montage_paths + ['# landing lag'] + landing_paths))
    if ONLY_RULES:
        log(f'settings {"created" if created else "updated"}: {len(montage_paths)} reaction + {len(landing_paths)} landing lag montages (montages.txt)')
        log('ESITO: OK rules only (GROUND_CONTACT_RULES_ONLY=1, ABP untouched)')
        return

    src = os.path.join(proj, 'Content', 'Blueprints', 'SandboxCharacter_Mover_ABP.uasset')
    bak = os.path.join(proj, 'Saved', 'Backups', 'GAS', 'groundcontact')
    os.makedirs(bak, exist_ok=True)
    shutil.copy2(src, os.path.join(bak, 'SandboxCharacter_Mover_ABP.uasset'))
    abp = unreal.load_asset(ABP)
    lib = unreal.GSTN_AnimGraphAuthoringLibrary
    node_class = unreal.GSTN_AnimGraphNode_GroundContactFix.static_class()
    with open(os.path.join(out_dir, 'abp_before.txt'), 'w', encoding='utf-8') as f:
        f.write(lib.describe_graphs(abp))
    if not lib.insert_pose_node_before_output(abp, node_class):
        log('ESITO: FAIL insert (see editor log)')
        return
    if not lib.set_pose_node_property(abp, node_class, 'Settings', da.get_path_name()):
        log('ESITO: FAIL settings on the node (see editor log)')
        return
    report, ok = lib.compile_with_report(abp)
    with open(os.path.join(out_dir, 'compile.txt'), 'w', encoding='utf-8') as f:
        f.write(str(report))
    saved = unreal.EditorAssetLibrary.save_loaded_asset(abp, False)
    with open(os.path.join(out_dir, 'abp_after.txt'), 'w', encoding='utf-8') as f:
        f.write(lib.describe_graphs(abp))
    log(f'settings {"created" if created else "updated"}: {da.get_path_name()}, {len(montage_paths)} reaction + {len(landing_paths)} landing lag montages (montages.txt)')
    log(f'ESITO: {"OK" if ok and saved else "FAIL"} node before Output Pose, compiled={ok}, saved={saved}')


try:
    run()
except Exception as e:  # noqa: BLE001
    log(f'ESITO: FAIL {e}')
with open(os.path.join(out_dir, 'ground_contact_setup.log'), 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
for line in lines[-5:]:
    print(line)
