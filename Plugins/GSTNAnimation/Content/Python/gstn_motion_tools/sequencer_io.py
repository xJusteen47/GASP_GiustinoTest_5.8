"""Editor backend of GSTN Motion Tools: reads and writes a Level Sequence with a Control Rig track.

Layers are additive Control Rig sections on the same track as the base (the section type Sequencer's Animation Layers
are made of), one row each. Which row is which layer is stored in the sequence's metadata tag `GSTN.MotionTools`
together with the settings applied, so the panel and agents can read back what was done.

Values are read by evaluating the rig (ControlRigSequencerLibrary), so they are exactly what the viewport shows;
layers that must not feed the operation are switched off (section inactive) while sampling and restored after.
Rotation channels: Rotation.X = roll, .Y = pitch, .Z = yaw (Euler degrees), as the Control Rig section stores them.
"""
import contextlib
import json

import unreal

META_TAG = "GSTN.MotionTools"
DISPLAY = unreal.MovieSceneTimeUnit.DISPLAY_RATE
_AXIS_SUFFIX = {"x": 0, "roll": 0, "y": 1, "pitch": 1, "z": 2, "yaw": 2}
_KIND_WORD = {"location": "loc", "translation": "loc", "rotation": "rot", "rotator": "rot", "scale": "scale"}


def _crlib():
    lib = getattr(unreal, "ControlRigSequencerLibrary", None) or getattr(unreal, "ControlRigSequencerEditorLibrary", None)
    if lib is None:
        raise RuntimeError("Control Rig scripting is not available (enable the ControlRig plugin)")
    return lib


def _parse_channel_name(name):
    """'head_ctrl.Rotation.X' -> ('head_ctrl', 'rot', 0); None for anything else (Weight, bools, floats)."""
    parts = str(name).split(".")
    if len(parts) < 3:
        return None
    kind = _KIND_WORD.get(parts[-2].lower())
    axis = _AXIS_SUFFIX.get(parts[-1].lower())
    if kind is None or axis is None:
        return None
    return ".".join(parts[:-2]), kind, axis


def _channel_map(section):
    out = {}
    for ch in section.get_all_channels():
        parsed = _parse_channel_name(ch.channel_name)
        if parsed:
            out[parsed] = ch
    return out


def _key_frame(key):
    return key.get_time(DISPLAY).frame_number.value


def current_sequence():
    seq = unreal.LevelSequenceEditorBlueprintLibrary.get_current_level_sequence()
    if not seq:
        raise RuntimeError("No Level Sequence is open in Sequencer")
    return seq


def open_sequence(path_or_asset):
    seq = unreal.load_asset(path_or_asset) if isinstance(path_or_asset, str) else path_or_asset
    if not isinstance(seq, unreal.LevelSequence):
        raise ValueError("Not a Level Sequence: %s" % path_or_asset)
    if unreal.LevelSequenceEditorBlueprintLibrary.get_current_level_sequence() != seq:
        unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)
    return seq


class UnrealBackend:
    def __init__(self, sequence=None, rig_name=None):
        self.seq = open_sequence(sequence) if sequence is not None else current_sequence()
        proxies = list(_crlib().get_control_rigs(self.seq))
        if rig_name:
            proxies = [p for p in proxies if rig_name.lower() in p.control_rig.get_name().lower()
                       or rig_name.lower() in str(p.track.get_display_name()).lower()]
        if not proxies:
            raise RuntimeError("No Control Rig track in %s" % self.seq.get_path_name())
        if len(proxies) > 1 and not rig_name:
            names = ", ".join(p.control_rig.get_name() for p in proxies)
            raise RuntimeError("More than one Control Rig in the sequence (%s): pass rig_name" % names)
        self.rig = proxies[0].control_rig
        self.track = proxies[0].track
        self._settings = self._read_meta()
        self._controls = None

    # -- sections ---------------------------------------------------------------------------------------------------
    def _layer_rows(self):
        return self._settings.setdefault("_rows", {})

    def _section_at_row(self, row):
        for sec in self.track.get_sections():
            if sec.get_row_index() == row:
                return sec
        return None

    def _layer_section(self, layer):
        row = self._layer_rows().get(layer)
        return self._section_at_row(row) if row is not None else None

    def base_section(self):
        ours = set(self._layer_rows().values())
        candidates = [s for s in self.track.get_sections() if s.get_row_index() not in ours]
        if not candidates:
            raise RuntimeError("The Control Rig track has no base section")
        return min(candidates, key=lambda s: s.get_row_index())

    # -- interface used by api.py -----------------------------------------------------------------------------------
    def controls(self):
        if self._controls is None:
            names = sorted({c for (c, kind, _a) in _channel_map(self.base_section()) if kind == "rot"})
            self._controls = names
        return list(self._controls)

    def frame_range(self):
        return self.seq.get_playback_start(), self.seq.get_playback_end()

    def key_frames(self, control):
        out = set()
        for (c, kind, _a), ch in _channel_map(self.base_section()).items():
            if c == control and kind in ("loc", "rot"):
                out.update(_key_frame(k) for k in ch.get_keys())
        return out

    @contextlib.contextmanager
    def _muted(self, layers):
        changed = []
        for layer in layers:
            sec = self._layer_section(layer)
            if sec and sec.is_active():
                sec.set_is_active(False)
                changed.append(sec)
        try:
            yield
        finally:
            for sec in changed:
                sec.set_is_active(True)

    def _read_local(self, control, frames):
        lib = _crlib()
        fn = [unreal.FrameNumber(f) for f in frames]
        try:
            vals = lib.get_local_control_rig_euler_transforms(self.seq, self.rig, control, fn, DISPLAY)
            return [((v.location.x, v.location.y, v.location.z),
                     (v.rotation.roll, v.rotation.pitch, v.rotation.yaw)) for v in vals]
        except Exception:
            vals = lib.get_local_control_rig_transforms(self.seq, self.rig, control, fn, DISPLAY)
            out = []
            for v in vals:
                r = v.rotation.rotator()
                t = v.translation
                out.append(((t.x, t.y, t.z), (r.roll, r.pitch, r.yaw)))
            return out

    def sample(self, control, frames, mute):
        with self._muted(mute):
            return self._read_local(control, frames)

    def write(self, layer, control, kind, axis, frames, values, keep_outside=False):
        sec = self._layer_section(layer)
        ch = _channel_map(sec).get((control, kind, axis))
        if ch is None:
            raise RuntimeError("Layer %s has no channel %s %s %d" % (layer, control, kind, axis))
        lo, hi = frames[0], frames[-1]
        for k in list(ch.get_keys()):
            f = _key_frame(k)
            if not keep_outside or lo <= f <= hi:
                ch.remove_key(k)
        ch.set_default(0.0)
        n = len(values)
        for i, (f, v) in enumerate(zip(frames, values)):
            # Skip long runs of zero (keeps the layer readable); keep the zero keys that bound a non-zero stretch.
            near = values[max(0, i - 1):min(n, i + 2)]
            if all(abs(x) < 1e-5 for x in near) and 0 < i < n - 1:
                continue
            ch.add_key(unreal.FrameNumber(f), float(v), 0.0, DISPLAY, unreal.MovieSceneKeyInterpolation.AUTO)

    def clear(self, layer, controls):
        sec = self._layer_section(layer)
        if not sec:
            return
        for (c, _kind, _a), ch in _channel_map(sec).items():
            if controls is None or c in controls:
                for k in list(ch.get_keys()):
                    ch.remove_key(k)
                ch.set_default(0.0)

    def has_layer(self, layer):
        return self._layer_section(layer) is not None

    def ensure_layer(self, layer):
        if self.has_layer(layer):
            return
        base = self.base_section()
        sec = self.track.add_section()
        sec.set_blend_type(unreal.MovieSceneBlendType.ADDITIVE)
        if base.has_start_frame():
            sec.set_start_frame(base.get_start_frame())
        else:
            sec.set_start_frame_bounded(False)
        if base.has_end_frame():
            sec.set_end_frame(base.get_end_frame())
        else:
            sec.set_end_frame_bounded(False)
        used = [s.get_row_index() for s in self.track.get_sections() if s != sec]
        row = max(used) + 1 if used else 1
        sec.set_row_index(row)
        self._layer_rows()[layer] = row
        # An empty additive layer must not move anything: loc/rot defaults 0. Scale stays as the engine set it for an
        # additive section; the neutrality check below catches a rig where that is not the case.
        for (c, kind, _a), ch in _channel_map(sec).items():
            if kind in ("loc", "rot"):
                ch.set_default(0.0)
        self._check_neutral(layer)
        self.save_settings(self._settings)

    def _check_neutral(self, layer):
        ctrls = self.controls()
        probe = [ctrls[0], ctrls[len(ctrls) // 2], ctrls[-1]] if ctrls else []
        s, e = self.frame_range()
        frames = [s, (s + e) // 2, e - 1]
        for c in probe:
            on = self._read_local(c, frames)
            with self._muted([layer]):
                off = self._read_local(c, frames)
            for a, b in zip(on, off):
                if max(abs(x - y) for x, y in zip(a[0] + a[1], b[0] + b[1])) > 1e-2:
                    self.remove_layer(layer)
                    raise RuntimeError("A new empty additive layer changed control %s: layer removed. Check the "
                                       "rig's scale channels in additive sections (handover, step 'neutral')." % c)

    def remove_layer(self, layer):
        sec = self._layer_section(layer)
        if sec:
            self.track.remove_section(sec)
        self._layer_rows().pop(layer, None)
        self.save_settings(self._settings)

    def set_layer_active(self, layer, active):
        self._layer_section(layer).set_is_active(active)

    def set_layer_weight(self, layer, weight):
        sec = self._layer_section(layer)
        for ch in sec.get_all_channels():
            if str(ch.channel_name).lower() == "weight":
                for k in list(ch.get_keys()):
                    ch.remove_key(k)
                ch.set_default(weight)
                return True
        return False

    def world_z(self, control, frames, mute):
        lib = _crlib()
        with self._muted(mute):
            return [lib.get_control_rig_world_transform(self.seq, self.rig, control, unreal.FrameNumber(f),
                                                        DISPLAY).translation.z for f in frames]

    # -- settings in the asset's metadata ---------------------------------------------------------------------------
    def _read_meta(self):
        raw = unreal.EditorAssetLibrary.get_metadata_tag(self.seq, META_TAG)
        try:
            return json.loads(raw) if raw else {}
        except ValueError:
            return {}

    def load_settings(self):
        return json.loads(json.dumps(self._settings))

    def save_settings(self, s):
        # The layer rows are owned by this backend: always the live ones, never a stale copy from load_settings().
        s["_rows"] = dict(self._settings.get("_rows", {}))
        self._settings = s
        unreal.EditorAssetLibrary.set_metadata_tag(self.seq, META_TAG, json.dumps(s, sort_keys=True))
        self.seq.modify()

    def transaction(self, text):
        return unreal.ScopedEditorTransaction(text)

    # -- helpers for the panel --------------------------------------------------------------------------------------
    def selected_controls(self):
        try:
            return [str(n) for n in self.rig.current_control_selection()]
        except Exception:
            return []

    def keys_around(self, controls, frame):
        """The base keys just before/after `frame` over the given controls (for 'easing around the playhead')."""
        keys = set()
        for c in controls:
            keys |= self.key_frames(c)
        before = [k for k in keys if k <= frame]
        after = [k for k in keys if k > frame]
        if not before or not after:
            return None
        return max(before), min(after)
