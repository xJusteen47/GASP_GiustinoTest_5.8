"""MCP toolset of GSTN Motion Tools for the editor's built-in MCP server (ToolsetRegistry). Registered by
init_unreal.py only when the ToolsetRegistry plugin is present; otherwise agents call the package through Python.
"""
import json

import unreal
import toolset_registry

import gstn_motion_tools as mt


def _opt(v):
    return None if v is None or v < 0 else v


def _summary(result):
    return json.dumps(result, sort_keys=True)


@unreal.uclass()
class GSTNMotionToolsToolset(unreal.ToolsetDefinition):
    """Non-destructive animation helpers for Level Sequences with a Control Rig track: follow-through (overlap) on
    named body chains and easing presets on key intervals, each written on its own additive Sequencer layer so the
    base keys never change. Also reads back the chains, layers and settings of a sequence."""

    @toolset_registry.tool_call
    @staticmethod
    def describe_sequence(sequence: str) -> str:
        """Chains available on the sequence's rig, existing layers and the settings applied so far.

        Args:
            sequence: Level Sequence asset path; empty for the one open in Sequencer.

        Returns:
            JSON object with frames, chains (name -> controls), layers (name -> exists) and settings.
        """
        return _summary(mt.describe(sequence or None))

    @toolset_registry.tool_call
    @staticmethod
    def apply_overlap(sequence: str, chain: str, preset: str, delay: float, softness: float, bounce: float,
                      mode: str, weight: float, protect: str, floor_z: float) -> str:
        """Writes follow-through on one chain to the Overlap layer (rewrites that chain if already there).

        Args:
            sequence: Level Sequence asset path; empty for the one open in Sequencer.
            chain: testa, busto, braccio_l, braccio_r, mano_l, mano_r, mantello, coda.
            preset: leggero, pesante, frusta, rigido.
            delay: Frames the chain tip lags; negative = preset value.
            softness: 0..1 rounding of the motion; negative = preset value.
            bounce: 0..1 overshoot when the motion stops; negative = preset value.
            mode: molla or sfasamento; empty = preset mode.
            weight: Layer amount for this chain, 1 = as computed.
            protect: keys (base key poses stay exact), ends, none.
            floor_z: World height of a floor plane the chain may not be pushed through; negative = no floor guard.

        Returns:
            JSON summary: controls, frames, effective settings, peak delta in degrees, floor guard result.
        """
        return _summary(mt.overlap(chain=chain, preset=preset, delay=_opt(delay), softness=_opt(softness),
                                   bounce=_opt(bounce), mode=mode or None, weight=weight, protect=protect,
                                   floor=None if floor_z < 0 else {"z": floor_z}, sequence=sequence or None))

    @toolset_registry.tool_call
    @staticmethod
    def apply_easing(sequence: str, controls: list[str], start: int, end: int, preset: str, intensity: float) -> str:
        """Reshapes the in-between of a key interval with an easing preset on the Easing layer; the values at start
        and end stay.

        Args:
            sequence: Level Sequence asset path; empty for the one open in Sequencer.
            controls: Control names.
            start: First display frame (usually a base key).
            end: Last display frame (usually the next base key).
            preset: lineare, morbido, scatto, anticipo, overshoot, assestamento.
            intensity: Strength of anticipation/overshoot/settle, 1 = curve sheet default.

        Returns:
            JSON summary with the peak delta in degrees.
        """
        return _summary(mt.easing(controls, start, end, preset, intensity, sequence=sequence or None))

    @toolset_registry.tool_call
    @staticmethod
    def set_layer(sequence: str, layer: str, active: bool, weight: float) -> str:
        """Switches a layer (Overlap or Easing) on or off and sets its weight.

        Args:
            sequence: Level Sequence asset path; empty for the one open in Sequencer.
            layer: Overlap or Easing.
            weight: Section weight; negative = unchanged.

        Returns:
            JSON summary.
        """
        return _summary(mt.layer(layer, active=active, weight=_opt(weight), sequence=sequence or None))

    @toolset_registry.tool_call
    @staticmethod
    def remove_layer(sequence: str, layer: str, chain: str) -> str:
        """Removes one chain from a layer, or the whole layer when chain is empty.

        Args:
            sequence: Level Sequence asset path; empty for the one open in Sequencer.
            layer: Overlap or Easing.

        Returns:
            JSON summary.
        """
        return _summary(mt.remove(layer, chain=chain or None, sequence=sequence or None))
