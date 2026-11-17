"""GSTN Motion Tools: overlap/follow-through and easing presets written on additive Sequencer layers.

Editor entry points (same parameters as the panel):

    import gstn_motion_tools as mt
    mt.overlap(chain="braccio_l", preset="pesante")                  # open sequence, preset values
    mt.overlap(chain="testa", preset="leggero", delay=3, bounce=0.5)  # sliders override the preset
    mt.overlap(controls=["hand_r_fk_ctrl"], preset="frusta", floor={"z": 0})
    mt.easing(["upperarm_r_fk_ctrl"], 10, 16, "scatto")
    mt.layer("Overlap", active=False)       # A/B
    mt.remove("Overlap", chain="testa")
    mt.describe()                           # chains on this rig, layers, settings applied
    mt.variant("/Game/AnimPipeline/Pilot/.../LS_GanonFTilt_v31", "overlap")   # work on a copy

`sequence=` (asset path) and `rig_name=` are accepted by every call; default = the sequence open in Sequencer.
The math (core.py) and the operations (api.py) import nothing from `unreal`.
"""
from . import api, core, presets  # noqa: F401

try:
    import unreal as _unreal  # noqa: F401
    _IN_EDITOR = True
except ImportError:
    _IN_EDITOR = False


def _backend(sequence=None, rig_name=None):
    from . import sequencer_io
    return sequencer_io.UnrealBackend(sequence, rig_name)


def overlap(chain=None, controls=None, preset="leggero", delay=None, softness=None, bounce=None, mode=None,
            weight=1.0, protect="keys", falloff=2.0, include_location=False, floor=None, frame_range=None,
            key_frames=None, sequence=None, rig_name=None):
    return api.apply_overlap(_backend(sequence, rig_name), chain=chain, controls=controls, preset=preset,
                             delay=delay, softness=softness, bounce=bounce, mode=mode, weight=weight,
                             protect=protect, falloff=falloff, include_location=include_location, floor=floor,
                             frame_range=frame_range, key_frames=key_frames)


def easing(controls, start, end, preset="morbido", intensity=1.0, include_location=False, sequence=None,
           rig_name=None):
    return api.apply_easing(_backend(sequence, rig_name), controls, start, end, preset, intensity, include_location)


def layer(name="Overlap", active=None, weight=None, sequence=None, rig_name=None):
    return api.set_layer(_backend(sequence, rig_name), name, active, weight)


def remove(name="Overlap", chain=None, controls=None, sequence=None, rig_name=None):
    return api.remove(_backend(sequence, rig_name), name, chain, controls)


def describe(sequence=None, rig_name=None):
    return api.describe(_backend(sequence, rig_name))


def variant(sequence_path, suffix):
    """Duplicates a Level Sequence as <name>_<suffix> next to it and opens the copy. Never touches the source."""
    import unreal
    src = unreal.load_asset(sequence_path)
    if not src:
        raise ValueError("Sequence not found: %s" % sequence_path)
    folder, name = sequence_path.rsplit("/", 1)
    name = name.split(".")[0]
    dst = "%s/%s_%s" % (folder, name, suffix)
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        raise ValueError("Variant already exists: %s (pick another suffix)" % dst)
    unreal.EditorAssetLibrary.duplicate_asset(src.get_path_name().split(".")[0], dst)
    unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(unreal.load_asset(dst))
    return {"ok": True, "op": "variant", "source": sequence_path, "variant": dst}
