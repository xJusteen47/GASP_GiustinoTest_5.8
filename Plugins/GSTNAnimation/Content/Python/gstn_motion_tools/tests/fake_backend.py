"""In-memory backend with the same interface as sequencer_io.UnrealBackend, for the cloud tests.

Model: a planar FK chain. Base values are per control a function frame -> (loc3, rot3). Layers are dicts
(control, kind, axis) -> {frame: value}; the visible value is base + sum of active layer values (additive, like an
additive Sequencer section). world_z: controls hang from a root at height root_z, each link `link_len` cm long, the
chain's angle is the running sum of Rotation.Y (pitch) of the links before and including it.
"""
import contextlib
import math


class FakeBackend:
    def __init__(self, base, keys, n_frames, start=0, chain_order=None, root_z=100.0, link_len=30.0):
        self.base = base                  # control -> callable(frame) -> ((x,y,z),(r,p,y))
        self.keys = keys                  # control -> set of frames
        self.start, self.end = start, start + n_frames
        self.layers = {}                  # layer -> {"active": bool, "weight": 1.0, "ch": {(c,k,a): {f: v}}}
        self.settings = {}
        self.chain_order = chain_order or list(base)
        self.root_z, self.link_len = root_z, link_len
        self.transactions = []

    # -- interface -------------------------------------------------------------------------------------------------
    def controls(self):
        return list(self.base)

    def frame_range(self):
        return self.start, self.end

    def key_frames(self, control):
        return set(self.keys.get(control, ()))

    def _layer_value(self, layer, control, kind, axis, f):
        return self.layers[layer]["ch"].get((control, kind, axis), {}).get(f, 0.0)

    def value(self, control, f, mute=()):
        loc, rot = self.base[control](f)
        loc, rot = list(loc), list(rot)
        for name, L in self.layers.items():
            if not L["active"] or name in mute:
                continue
            for a in range(3):
                loc[a] += L["weight"] * self._layer_value(name, control, "loc", a, f)
                rot[a] += L["weight"] * self._layer_value(name, control, "rot", a, f)
        return tuple(loc), tuple(rot)

    def sample(self, control, frames, mute):
        return [self.value(control, f, mute) for f in frames]

    def write(self, layer, control, kind, axis, frames, values, keep_outside=False):
        ch = self.layers[layer]["ch"]
        old = ch.get((control, kind, axis), {}) if keep_outside else {}
        new = {f: v for f, v in old.items() if f < frames[0] or f > frames[-1]}
        new.update({f: v for f, v in zip(frames, values)})
        ch[(control, kind, axis)] = new

    def clear(self, layer, controls):
        ch = self.layers[layer]["ch"]
        for key in list(ch):
            if controls is None or key[0] in controls:
                del ch[key]

    def has_layer(self, layer):
        return layer in self.layers

    def ensure_layer(self, layer):
        self.layers.setdefault(layer, {"active": True, "weight": 1.0, "ch": {}})

    def remove_layer(self, layer):
        self.layers.pop(layer, None)

    def set_layer_active(self, layer, active):
        self.layers[layer]["active"] = active

    def set_layer_weight(self, layer, weight):
        self.layers[layer]["weight"] = weight
        return True

    def world_z(self, control, frames, mute):
        idx = self.chain_order.index(control)
        out = []
        for f in frames:
            z, ang = self.root_z, 0.0
            for c in self.chain_order[:idx + 1]:
                ang += math.radians(self.value(c, f, mute)[1][1])
                z -= self.link_len * math.cos(ang)     # hangs down at angle 0
            out.append(z)
        return out

    def load_settings(self):
        import copy
        return copy.deepcopy(self.settings)

    def save_settings(self, s):
        self.settings = s

    @contextlib.contextmanager
    def transaction(self, text):
        self.transactions.append(text)
        yield
