# Registers the wall slide v2 mode (and, with --use v2climb, the climb v2 mode) next to the legacy one on SandboxCharacter_Mover and
# switches which mode(s) the Falling mode's wall entry transition grabs (wall-slide-v2.md 2.1 / W2, 6.3 / W4b). Idempotent: running it
# again changes nothing.
#
# Run from the editor Python console:
#   import sys; sys.path.insert(0, r"<project>/Tools/GAS"); import setup_wall_slide_modes as s; s.run(use='v2')   # or 'legacy', 'v2climb', or None
# or headless (editor closed):
#   UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<project>/Tools/GAS/setup_wall_slide_modes.py --use v2"
#
# --use values:
#   legacy   Falling grabs "WallSliding" (UGSTN_WallEntryTransition, TargetModeName WallSliding)
#   v2       Falling grabs "WallSlidingV2" (UGSTN_WallEntryTransition, TargetModeName WallSlidingV2)
#   v2climb  WallSlidingV2 AND WallClimbingV2 are registered, and the Falling entry transition becomes its surface-driven twin
#            (UGSTN_WallEntryTransition -> UGSTN_WallSurfaceModeTransition, UGSTN_AbilityWallEntryTransition ->
#            UGSTN_AbilityWallSurfaceModeTransition, so the grab stays a gameplay ability): the surface under the wall decides (a
#            Surface.Behaviour.WallClimb surface -> WallClimbingV2, WallSlide -> WallSlidingV2, unmarked -> the slide) and a character crossing
#            from one kind of surface to the other is handed over in place by the modes themselves
#   (none)   only registers WallSlidingV2
# v2 and legacy turn a surface-driven transition back into its plain twin. A transition is replaced by one of the other class only when --use
# needs it; the settings the classes share (probe distance, conditions, re-entry rules, ability tags...) are copied over, nothing else of the
# Falling mode changes.
#
# Steps:
#  1. SandboxCharacter_Mover: the Mover component gets a "WallSlidingV2" mode (UGSTN_WallSlideV2MovementMode) if it has none. A new
#     mode starts with the settings it shares with the legacy "WallSliding" mode copied from it (surface limits, probe distance,
#     wall jump, detach, mode names, input mapping, filter...), so the A/B test compares two modes with the same wall rules; the
#     control model (Control, Brake), the edge settings and the surface mode list keep the class defaults. An existing V2 mode is never
#     touched again.
#  2. (only with --use v2climb) A "WallClimbingV2" mode (UGSTN_WallClimbV2MovementMode) is registered the same way, with the shared settings
#     copied from "WallSliding" except its own control model, edge settings, surface filter (WallClimb) and surface mode list (class defaults).
#  3. (only with --use) The Falling mode's wall entry transition(s) are set to what --use asks for (above).
# The ability's WallModeNames (UGSTN_GA_WallSlide) must list "WallSlidingV2" and "WallClimbingV2" too for the ability to stay active on them:
# that is a C++ default, not an asset, and both names are there.
# The Blueprint is copied to Saved/Backups/GAS (timestamped) before it is changed (Content is only partly under version control).
import os, shutil, re, time
import unreal

SANDBOX_CHAR = '/Game/Blueprints/SandboxCharacter_Mover'
FALLING_MODE = 'Falling'
LEGACY_MODE = 'WallSliding'
V2_MODE = 'WallSlidingV2'
CLIMB_MODE = 'WallClimbingV2'
# use -> TargetModeName of a plain entry transition, or None for the surface-driven transition (which has no fixed target)
USE_TARGETS = {'v2': V2_MODE, 'legacy': LEGACY_MODE, 'v2climb': None}

# Properties of a v2 mode that are NOT copied from the legacy mode: its own control model, edge settings and surface mode list
V2_ONLY = {'control', 'brake', 'edges', 'max_move_radii', 'surface_modes'}
# The climb mode keeps its own WallClimb surface filter too (the legacy one is empty) and its own tilt flag
CLIMB_ONLY = V2_ONLY | {'surface_filter', 'b_allow_capsule_tilt'}
# Properties of a transition that are not copied when its class changes: the surface-driven one has no fixed target and its own list
TRANSITION_ONLY = {'target_mode_name', 'surface_modes'}
# ...and a surface-driven transition does not take the surface filter of the plain one it replaces: its mode list is the filter (the C++ ignores the field
# there; the first A/B run found the WallSlide-only filter of the old transition on the swapped one, and no climb-only wall was ever grabbed)
SURFACE_TRANSITION_SKIP = {'surface_filter'}

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


def add_v2_mode(mover, mode_name=V2_MODE, cls=None, skip=V2_ONLY):
    """Adds a v2 wall mode (the slide by default) when the Mover component has none by that name. Returns True if the component changed."""
    cls = cls or unreal.GSTN_WallSlideV2MovementMode
    modes = mover.get_editor_property('movement_modes')
    if modes.get(mode_name):
        log(f'{mode_name} already registered, left as it is')
        return False

    new = unreal.new_object(cls, outer=mover)
    legacy = modes.get(LEGACY_MODE)
    if legacy:
        legacy_names = set(editor_property_names(type(legacy)))
        copied = 0
        for name in editor_property_names(cls):
            if name in skip or name not in legacy_names:
                continue
            try:
                new.set_editor_property(name, legacy.get_editor_property(name))
                copied += 1
            except Exception as e:
                log(f'  could not copy {name}: {e}')
        log(f'{mode_name}: created, {copied} settings copied from {LEGACY_MODE}')
    else:
        log(f'WARNING: no "{LEGACY_MODE}" mode to copy the shared settings from, {mode_name} created with the class defaults')

    modes[mode_name] = new
    mover.set_editor_property('movement_modes', modes)
    return True


def add_climb_mode(mover):
    """Adds the WallClimbingV2 mode when the Mover component has none. Returns True if the component changed."""
    return add_v2_mode(mover, CLIMB_MODE, unreal.GSTN_WallClimbV2MovementMode, CLIMB_ONLY)


def wants_surface_transition(use):
    return use == 'v2climb'


def is_surface_transition(transition):
    return isinstance(transition, (unreal.GSTN_WallSurfaceModeTransition, unreal.GSTN_AbilityWallSurfaceModeTransition))


def transition_class(old, use):
    """The class a wall entry transition `old` must have for --use: the ability gated family stays ability gated, the surface-driven
    twin is used for v2climb and the plain twin otherwise."""
    ability = isinstance(old, unreal.GSTN_AbilityWallEntryTransition)
    if wants_surface_transition(use):
        return unreal.GSTN_AbilityWallSurfaceModeTransition if ability else unreal.GSTN_WallSurfaceModeTransition
    return unreal.GSTN_AbilityWallEntryTransition if ability else unreal.GSTN_WallEntryTransition


def transition_is_right(transition, use):
    """True if this wall entry transition already is what --use asks for (class and, for the plain class, target)."""
    if not isinstance(transition, unreal.GSTN_WallEntryTransition):
        return True  # not a wall entry transition: not ours to change
    if wants_surface_transition(use):
        return is_surface_transition(transition)
    return (not is_surface_transition(transition)) and str(transition.get_editor_property('target_mode_name')) == USE_TARGETS[use]


def make_transition(old, use, outer):
    """A new entry transition of the class --use asks for, with the settings the classes share copied from `old`."""
    cls = transition_class(old, use)
    new = unreal.new_object(cls, outer=outer)
    new_names = set(editor_property_names(cls))
    skip = TRANSITION_ONLY | (SURFACE_TRANSITION_SKIP if wants_surface_transition(use) else set())
    copied = 0
    for name in editor_property_names(type(old)):
        if name in skip or name not in new_names:
            continue
        try:
            new.set_editor_property(name, old.get_editor_property(name))
            copied += 1
        except Exception as e:
            log(f'  could not copy {name}: {e}')
    if not wants_surface_transition(use):
        new.set_editor_property('target_mode_name', USE_TARGETS[use])
    log(f'{cls.__name__}: created, {copied} settings copied from {type(old).__name__}')
    return new


def switch_entry_transitions(mover, use):
    """Makes the Falling mode's wall entry transition(s) what `use` asks for: the surface-driven class (v2climb) or the plain class
    aimed at WallSlidingV2 / WallSliding. Returns True if any changed."""
    falling = mover.get_editor_property('movement_modes').get(FALLING_MODE)
    if not falling:
        log(f'ERROR: no "{FALLING_MODE}" mode on {SANDBOX_CHAR}')
        return False

    changed = False
    found = 0
    transitions = list(falling.get_editor_property('transitions'))
    for i, transition in enumerate(transitions):
        if not isinstance(transition, unreal.GSTN_WallEntryTransition):
            continue
        found += 1
        if transition_is_right(transition, use):
            log(f'{FALLING_MODE}: transition {i} ({type(transition).__name__}) already right for --use {use}')
            continue
        if type(transition) is transition_class(transition, use):
            # Same class, only the target differs (plain transition): set it in place, nothing else of the transition changes
            current = str(transition.get_editor_property('target_mode_name'))
            transition.set_editor_property('target_mode_name', USE_TARGETS[use])
            log(f'{FALLING_MODE}: transition {i} TargetModeName {current} -> {USE_TARGETS[use]}')
        else:
            transitions[i] = make_transition(transition, use, falling)
            log(f'{FALLING_MODE}: transition {i} {type(transition).__name__} -> {type(transitions[i]).__name__}')
        changed = True
    if changed:
        falling.set_editor_property('transitions', transitions)
    if not found:
        log(f'WARNING: the {FALLING_MODE} mode has no wall entry transition (run setup_gas_assets.py --wall-transition first?)')
    return changed


def run(use=None):
    """use: 'v2' | 'legacy' | 'v2climb' | None (None only registers the v2 slide mode; v2climb also registers the climb mode)."""
    if use is not None and use not in USE_TARGETS:
        log(f'ERROR: use must be one of {list(USE_TARGETS)}, got {use!r}')
        return False

    bp = unreal.load_asset(SANDBOX_CHAR)
    mover = find_mover(bp) if bp else None
    if not mover:
        log(f'ERROR: no Mover component on {SANDBOX_CHAR}')
        return False

    # Decide first, back up only if something will change
    modes = mover.get_editor_property('movement_modes')
    has_v2 = bool(modes.get(V2_MODE))
    needs_climb = use == 'v2climb' and not modes.get(CLIMB_MODE)
    falling = modes.get(FALLING_MODE)
    needs_switch = use is not None and falling is not None and any(
        isinstance(t, unreal.GSTN_WallEntryTransition) and not transition_is_right(t, use)
        for t in falling.get_editor_property('transitions'))
    if has_v2 and not needs_climb and not needs_switch:
        log('nothing to change')
        return False

    backup(SANDBOX_CHAR)
    changed = add_v2_mode(mover)
    if use == 'v2climb':
        changed |= add_climb_mode(mover)
    if use is not None:
        changed |= switch_entry_transitions(mover, use)
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
