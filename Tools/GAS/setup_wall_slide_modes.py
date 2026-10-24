# Registers the wall slide v2 mode next to the legacy one on SandboxCharacter_Mover and switches which one the Falling mode's wall
# entry transition grabs (wall-slide-v2.md 2.1 / W2). Idempotent: running it again changes nothing.
#
# Run from the editor Python console:
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_wall_slide_modes as s; s.run(use='v2')   # or 'legacy', or None
# or headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_wall_slide_modes.py --use v2"
#
# Steps:
#  1. SandboxCharacter_Mover: the Mover component gets a "WallSlidingV2" mode (UGSTN_WallSlideV2MovementMode) if it has none. A new
#     mode starts with the settings it shares with the legacy "WallSliding" mode copied from it (surface limits, probe distance,
#     wall jump, detach, mode names, input mapping, filter...), so the A/B test compares two modes with the same wall rules; the
#     control model (Control, Brake) and the edge settings keep the class defaults. An existing V2 mode is never touched again.
#  2. (only with --use) The TargetModeName of the Falling mode's wall entry transition(s) becomes WallSlidingV2 (--use v2) or
#     WallSliding (--use legacy). Nothing else of the transition changes.
# The ability's WallModeNames (UGSTN_GA_WallSlide) must list "WallSlidingV2" too for the ability to stay active on it: that is a C++
# default, not an asset (see the wall-slide-v2 W2 notes), so the script only warns when the ability cannot be checked.
# The Blueprint is copied to Saved/Backups/GAS (timestamped) before it is changed (Content is only partly under version control).
import os, shutil, re, time
import unreal

SANDBOX_CHAR = '/Game/Blueprints/SandboxCharacter_Mover'
FALLING_MODE = 'Falling'
LEGACY_MODE = 'WallSliding'
V2_MODE = 'WallSlidingV2'
USE_TARGETS = {'v2': V2_MODE, 'legacy': LEGACY_MODE}

# Properties of the v2 mode that are NOT copied from the legacy mode: its own control model and edge settings
V2_ONLY = {'control', 'brake', 'edges', 'max_move_radii'}

subobjects = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def log(msg):
    unreal.log(f'[setup_wall_slide_modes] {msg}')


def backup(asset_path):
    """One timestamped copy per run, in Saved/Backups/GAS (an older backup of the same asset is never overwritten)."""
    src = unreal.Paths.convert_relative_path_to_full(
        unreal.Paths.project_content_dir() + asset_path[len('/Game/'):] + '.uasset')
    dst_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Backups', 'GAS')
    os.makedirs(dst_dir, exist_ok=True)
    stem = os.path.splitext(os.path.basename(src))[0]
    dst = os.path.join(dst_dir, f'{stem}.{time.strftime("%Y%m%d-%H%M%S")}.uasset')
    if not os.path.exists(dst):
        shutil.copy2(src, dst)
        log(f'backup {src} -> {dst}')


def editor_property_names(cls):
    """Editor property names of a class, from its generated docstring."""
    return re.findall(r'- ``(\w+)`` \(', cls.__doc__ or '')


def blueprint_components(bp):
    out = []
    for handle in subobjects.k2_gather_subobject_data_for_blueprint(bp):
        data = sdl.get_data(handle)
        obj = sdl.get_object_for_blueprint(data, bp)
        if obj:
            out.append((handle, obj))
    return out


def find_mover(bp):
    return next((obj for _, obj in blueprint_components(bp) if isinstance(obj, unreal.MoverComponent)), None)


def add_v2_mode(mover):
    """Adds the WallSlidingV2 mode when the Mover component has none. Returns True if the component changed."""
    modes = mover.get_editor_property('movement_modes')
    if modes.get(V2_MODE):
        log(f'{V2_MODE} already registered, left as it is')
        return False

    new = unreal.new_object(unreal.GSTN_WallSlideV2MovementMode, outer=mover)
    legacy = modes.get(LEGACY_MODE)
    if legacy:
        legacy_names = set(editor_property_names(type(legacy)))
        copied = 0
        for name in editor_property_names(unreal.GSTN_WallSlideV2MovementMode):
            if name in V2_ONLY or name not in legacy_names:
                continue
            try:
                new.set_editor_property(name, legacy.get_editor_property(name))
                copied += 1
            except Exception as e:
                log(f'  could not copy {name}: {e}')
        log(f'{V2_MODE}: created, {copied} settings copied from {LEGACY_MODE}')
    else:
        log(f'WARNING: no "{LEGACY_MODE}" mode to copy the shared settings from, {V2_MODE} created with the class defaults')

    modes[V2_MODE] = new
    mover.set_editor_property('movement_modes', modes)
    return True


def switch_entry_target(mover, target):
    """Points the Falling mode's wall entry transition(s) at `target`. Returns True if any changed."""
    falling = mover.get_editor_property('movement_modes').get(FALLING_MODE)
    if not falling:
        log(f'ERROR: no "{FALLING_MODE}" mode on {SANDBOX_CHAR}')
        return False

    changed = False
    found = 0
    for i, transition in enumerate(falling.get_editor_property('transitions')):
        if not isinstance(transition, unreal.GSTN_WallEntryTransition):
            continue
        found += 1
        current = str(transition.get_editor_property('target_mode_name'))
        if current == target:
            log(f'{FALLING_MODE}: transition {i} already targets {target}')
            continue
        transition.set_editor_property('target_mode_name', target)
        log(f'{FALLING_MODE}: transition {i} ({type(transition).__name__}) TargetModeName {current} -> {target}')
        changed = True
    if not found:
        log(f'WARNING: the {FALLING_MODE} mode has no wall entry transition (run setup_gas_assets.py --wall-transition first?)')
    return changed


def run(use=None):
    """use: 'v2' | 'legacy' | None (None only registers the v2 mode)."""
    if use is not None and use not in USE_TARGETS:
        log(f'ERROR: use must be one of {list(USE_TARGETS)}, got {use!r}')
        return False

    bp = unreal.load_asset(SANDBOX_CHAR)
    mover = find_mover(bp) if bp else None
    if not mover:
        log(f'ERROR: no Mover component on {SANDBOX_CHAR}')
        return False

    # Decide first, back up only if something will change
    has_v2 = bool(mover.get_editor_property('movement_modes').get(V2_MODE))
    falling = mover.get_editor_property('movement_modes').get(FALLING_MODE)
    target = USE_TARGETS.get(use)
    needs_switch = bool(target) and falling is not None and any(
        isinstance(t, unreal.GSTN_WallEntryTransition) and str(t.get_editor_property('target_mode_name')) != target
        for t in falling.get_editor_property('transitions'))
    if has_v2 and not needs_switch:
        log('nothing to change')
        return False

    backup(SANDBOX_CHAR)
    changed = add_v2_mode(mover)
    if target:
        changed |= switch_entry_target(mover, target)
    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        log(f'{SANDBOX_CHAR} saved')
    return changed


if __name__ == '__main__':
    import sys
    chosen = None
    if '--use' in sys.argv:
        idx = sys.argv.index('--use')
        chosen = sys.argv[idx + 1] if idx + 1 < len(sys.argv) else None
    run(use=chosen)
