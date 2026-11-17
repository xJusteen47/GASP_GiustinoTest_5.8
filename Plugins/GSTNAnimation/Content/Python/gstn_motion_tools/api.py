"""Operations of GSTN Motion Tools, written against a small backend interface so the same logic runs in the editor
(sequencer_io.UnrealBackend) and in the cloud tests (tests/fake_backend.py).

Contract, for the panel, the Python/MCP callers and agy:
- The base section of the Control Rig track is NEVER written. Each operation writes only its own additive layer
  ("Overlap", "Easing"), one Sequencer section row per layer; re-applying rewrites the same channels (idempotent).
- Overlap reads the visible motion under it (base + Easing layer); Easing reads the base only.
- With protect="keys" (default) every base key frame of the chain, the first and the last frame get delta 0: the
  animator's key poses stay exact (owner rule 13).
- Optional floor guard: the layer may not push a chain part under a plane (it never fixes the base: phase 2).

Backend interface (duck-typed):
  controls() -> [name]                         transform controls of the rig
  frame_range() -> (start, end_exclusive)      display frames of the playback range
  key_frames(control) -> {frame}               base keys of that control (any channel)
  sample(control, frames, mute) -> [(loc3, rot3)]   visible local values with layers in `mute` switched off
  write(layer, control, kind, axis, frames, values)  replace that channel's keys on the layer ('loc'|'rot', 0..2)
  clear(layer, controls)                       remove the layer keys of those controls (None = all)
  has_layer(layer) / ensure_layer(layer) / remove_layer(layer)
  set_layer_active(layer, bool) / set_layer_weight(layer, w) -> bool (False = no native weight)
  world_z(control, frames, mute) -> [z]        world height of the control
  load_settings() -> dict / save_settings(dict)
  transaction(text) -> context manager
"""
from . import core, presets

LAYER_OVERLAP = "Overlap"
LAYER_EASING = "Easing"
LAYER_ORDER = [LAYER_EASING, LAYER_OVERLAP]     # lower first; a layer reads everything below it
DENSE_KEY_SHARE = 0.6                           # keys on >= 60% of the frames = a baked clip, not key poses


def _layers_from(layer):
    """Layers that must be muted while sampling the input of `layer`: itself and every layer above it."""
    if layer not in LAYER_ORDER:
        return [layer]
    return LAYER_ORDER[LAYER_ORDER.index(layer):]


def _range(backend, frame_range):
    start, end = backend.frame_range()
    if frame_range:
        start, end = max(start, int(frame_range[0])), min(end, int(frame_range[1]) + 1)
    if end - start < 2:
        raise ValueError("Frame range %d..%d is too short" % (start, end - 1))
    return start, end


def _resolve_controls(backend, chain, controls):
    available = backend.controls()
    if controls:
        missing = [c for c in controls if c not in available]
        if missing:
            raise ValueError("Controls not on this rig: %s" % ", ".join(missing))
        return list(controls), "selection"
    if not chain:
        raise ValueError("Give a chain name or a list of controls")
    resolved = presets.resolve_chain(chain, available)
    if not resolved:
        raise ValueError("Chain '%s' has no control on this rig" % chain)
    return resolved, presets.chain_key(chain)


def overlap_settings(preset="leggero", delay=None, softness=None, bounce=None, mode=None):
    """The effective slider values: the preset's, overridden by any value given."""
    key, p = presets.overlap_preset(preset)
    return {
        "preset": key,
        "delay": float(p["delay"] if delay is None else delay),
        "softness": core.clamp(float(p["softness"] if softness is None else softness), 0.0, 1.0),
        "bounce": core.clamp(float(p["bounce"] if bounce is None else bounce), 0.0, 1.0),
        "mode": p["mode"] if mode is None else presets.mode_name(mode),
    }


def apply_overlap(backend, chain=None, controls=None, preset="leggero", delay=None, softness=None, bounce=None,
                  mode=None, weight=1.0, protect=core.PROTECT_KEYS, falloff=2.0, include_location=False,
                  floor=None, frame_range=None, key_frames=None, layer=LAYER_OVERLAP):
    """Writes follow-through for one chain on the Overlap layer. Returns a summary dict.
    floor: None (off) or {"z": world cm, "parts": [control names] (default: the chain)}.
    key_frames: the animator's key frames (display frames) to protect; default = the base keys of each control.
    A base keyed on (almost) every frame, e.g. an AnimSequence opened with the FK Control Rig, has no recognisable
    key poses: then only the ends are protected and the result carries a warning asking for key_frames."""
    settings = overlap_settings(preset, delay, softness, bounce, mode)
    ctrls, chain_id = _resolve_controls(backend, chain, controls)
    start, end = _range(backend, frame_range)
    frames = list(range(start, end))
    n = len(frames)
    mute = _layers_from(layer)

    deltas = {}         # (control, kind, axis) -> delta list (masked, weighted)
    warnings = []
    for i, ctrl in enumerate(ctrls):
        samples = backend.sample(ctrl, frames, mute)
        ctrl_protect = protect
        if key_frames is not None:
            keys = [int(k) - start for k in key_frames]
        else:
            keys = [k - start for k in backend.key_frames(ctrl) if start <= k < end]
            if protect == core.PROTECT_KEYS and len(keys) >= DENSE_KEY_SHARE * n:
                ctrl_protect = core.PROTECT_ENDS
                if not warnings:
                    warnings.append("Base keyed on almost every frame (baked clip): key poses not recognisable, only "
                                    "the first and last frame are protected. Pass key_frames=[...] (owner keys).")
        mask = core.key_weight_mask(n, keys, falloff, ctrl_protect)
        d_link = core.link_delay(settings["delay"], i, len(ctrls))
        kinds = [("rot", 1)] + ([("loc", 0)] if include_location else [])
        for kind, idx in kinds:
            for axis in range(3):
                values = [s[idx][axis] for s in samples]
                delta = core.overlap_delta(values, d_link, settings["softness"], settings["bounce"],
                                           settings["mode"], is_rotation=(kind == "rot"))
                deltas[(ctrl, kind, axis)] = core.apply_mask(delta, mask, weight)

    with backend.transaction("GSTN Motion Tools: overlap %s" % chain_id):
        backend.ensure_layer(layer)
        backend.clear(layer, ctrls)
        for (ctrl, kind, axis), d in deltas.items():
            backend.write(layer, ctrl, kind, axis, frames, d)
        guard = None
        if floor is not None:
            guard = _floor_guard(backend, layer, ctrls, frames, deltas, floor)
        all_settings = backend.load_settings()
        all_settings.setdefault("overlap", {})[chain_id] = dict(settings, controls=ctrls, weight=weight,
                                                                 protect=protect, falloff=falloff, floor=floor,
                                                                 key_frames=key_frames,
                                                                 frames=[start, end - 1])
        backend.save_settings(all_settings)

    peak = max((abs(v) for d in deltas.values() for v in d), default=0.0)
    out = {"ok": True, "op": "overlap", "chain": chain_id, "controls": ctrls, "frames": [start, end - 1],
           "settings": settings, "weight": weight, "protect": protect, "peak_delta": round(peak, 3)}
    if guard:
        out["floor"] = guard
    if warnings:
        out["warnings"] = warnings
    return out


def _floor_guard(backend, layer, ctrls, frames, deltas, floor):
    """Scales the layer down on frames where it pushes a guarded part's sphere under the floor plane."""
    floor_z = float(floor.get("z", 0.0)) if isinstance(floor, dict) else float(floor)
    parts = (floor.get("parts") if isinstance(floor, dict) else None) or ctrls
    n = len(frames)
    scale = [1.0] * n
    for part in parts:
        r = presets.radius_for(part)
        z_base = backend.world_z(part, frames, [layer])
        z_layer = backend.world_z(part, frames, [])
        s = core.floor_guard_scale(z_base, z_layer, floor_z, r)
        scale = [min(a, b) for a, b in zip(scale, s)]
    reduced = sum(1 for s in scale if s < 0.999)
    if reduced:
        scale = core.soften_scale(scale)
        for (ctrl, kind, axis), d in deltas.items():
            scaled = [v * s for v, s in zip(d, scale)]
            deltas[(ctrl, kind, axis)] = scaled
            backend.write(layer, ctrl, kind, axis, frames, scaled)
    # Report what is left (a linear estimate can leave a few mm on strongly curved chains).
    worst = None
    for part in parts:
        r = presets.radius_for(part)
        z = backend.world_z(part, frames, [])
        zb = backend.world_z(part, frames, [layer])
        for a, b in zip(z, zb):
            pen = (floor_z + r) - a
            base_pen = (floor_z + r) - b
            extra = pen - max(0.0, base_pen)
            if pen > 0 and extra > 0 and (worst is None or extra > worst):
                worst = extra
    return {"z": floor_z, "parts": parts, "frames_reduced": reduced,
            "residual_cm": round(worst, 2) if worst is not None else 0.0}


def apply_easing(backend, controls, start, end, preset="morbido", intensity=1.0, include_location=False,
                 layer=LAYER_EASING):
    """Reshapes the in-between of start..end (display frames, usually two base keys) with an easing preset, on the
    Easing layer. The values at start and end do not change."""
    preset = core.easing_name(preset)
    if not controls:
        raise ValueError("Select at least one control")
    available = backend.controls()
    missing = [c for c in controls if c not in available]
    if missing:
        raise ValueError("Controls not on this rig: %s" % ", ".join(missing))
    r0, r1 = backend.frame_range()
    start, end = int(start), int(end)
    if not (r0 <= start < end < r1):
        raise ValueError("Easing interval %d..%d is outside the playback range %d..%d" % (start, end, r0, r1 - 1))
    frames = list(range(start, end + 1))
    mute = _layers_from(layer)
    writes = []
    for ctrl in controls:
        samples = backend.sample(ctrl, frames, mute)
        kinds = [("rot", 1)] + ([("loc", 0)] if include_location else [])
        for kind, idx in kinds:
            for axis in range(3):
                values = [s[idx][axis] for s in samples]
                d = core.easing_delta(values, 0, len(values) - 1, preset, intensity, is_rotation=(kind == "rot"))
                writes.append((ctrl, kind, axis, d))
    with backend.transaction("GSTN Motion Tools: easing %s" % preset):
        backend.ensure_layer(layer)
        for ctrl, kind, axis, d in writes:
            backend.write(layer, ctrl, kind, axis, frames, d, keep_outside=True)
        s = backend.load_settings()
        s.setdefault("easing", []).append({"controls": list(controls), "frames": [start, end], "preset": preset,
                                           "intensity": intensity})
        backend.save_settings(s)
    peak = max((abs(v) for _, _, _, d in writes for v in d), default=0.0)
    return {"ok": True, "op": "easing", "preset": preset, "controls": list(controls), "frames": [start, end],
            "intensity": intensity, "peak_delta": round(peak, 3)}


def remove(backend, layer=LAYER_OVERLAP, chain=None, controls=None):
    """Removes a chain's keys from a layer, or the whole layer when no chain/controls are given."""
    if not backend.has_layer(layer):
        return {"ok": True, "op": "remove", "layer": layer, "removed": False}
    with backend.transaction("GSTN Motion Tools: remove %s" % layer):
        s = backend.load_settings()
        if chain or controls:
            ctrls, chain_id = _resolve_controls(backend, chain, controls)
            backend.clear(layer, ctrls)
            s.get("overlap", {}).pop(chain_id, None)
        else:
            backend.remove_layer(layer)
            if layer == LAYER_OVERLAP:
                s.pop("overlap", None)
            elif layer == LAYER_EASING:
                s.pop("easing", None)
        backend.save_settings(s)
    return {"ok": True, "op": "remove", "layer": layer, "removed": True}


def set_layer(backend, layer=LAYER_OVERLAP, active=None, weight=None):
    """Switches a layer on/off and/or sets its native weight (Sequencer section weight)."""
    if not backend.has_layer(layer):
        raise ValueError("Layer '%s' does not exist yet" % layer)
    out = {"ok": True, "op": "set_layer", "layer": layer}
    with backend.transaction("GSTN Motion Tools: layer %s" % layer):
        if active is not None:
            backend.set_layer_active(layer, bool(active))
            out["active"] = bool(active)
        if weight is not None:
            out["weight"] = float(weight)
            out["native_weight"] = backend.set_layer_weight(layer, float(weight))
    return out


def describe(backend):
    controls = backend.controls()
    s = backend.load_settings()
    return {"ok": True, "op": "describe", "frames": list(backend.frame_range()), "controls": len(controls),
            "chains": presets.available_chains(controls),
            "layers": {name: backend.has_layer(name) for name in LAYER_ORDER}, "settings": s}
