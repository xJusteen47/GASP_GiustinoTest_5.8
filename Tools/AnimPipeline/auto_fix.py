"""Deterministic fixer: fix_rules.json made executable (owner, 31/10: "a deterministic script wherever possible, a model only
on what remains"). It runs the motion model in memory, reads the pre-flight flags, applies each flag's rule as a bounded
change of a module-level model parameter, and keeps a change only when the weighted flag cost goes down. A small
coordinate/line search (one knob at a time, steps up to the rule's limit) handles rules that interact.

Protected (never changed): the contact frames, the snap, the hold, the FAF and after, KICK_YAW / CONTACT_ANKLE_Y / REACH,
keys, layers. A move past a knob's limit is a big pose change: not tried, reported.
Stops when the pre-flight is clean, when no move improves the cost, or after --max-evals model runs.

Output: <take>_overrides.json (the changed curves; the model reads them through MODEL_OVERRIDES) and a residual report
(sheets/<take>_autofix.txt/.json): remaining flags, what was tried, the parameter deltas. The residual goes to the Sonnet
worker (iter_worker.md), then to Opus.

Usage: python auto_fix.py <model.py> <take> [--max-evals N] [--sheet KEY]"""
import sys, os, io, json, copy, math, time, importlib.util, contextlib
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import preflight as PF

SEC_W = {1: 6.0, 2: 10.0, 3: 1.0, 4: 4.0, 5: 2.0, 6: 3.0}
# knob limits: max |total change| from the original value (cm, deg, frames, weight)
LIMIT = {"PEL_Y": 3.0, "BAL_S": 6.0, "BAL_W": 1.0, "LEAN": 6.0, "HEEL": 15.0, "ANK_ADJ": 9.0, "CHEST_LAG": 1.0, "HEAD_LAG": 1.5,
         "CHEST_YAW": 15.0, "ARM_L": 6.0, "ARM_R": 6.0}


def load_model(path):
    os.environ["DENSE"] = "1"
    spec = importlib.util.spec_from_file_location("model_under_fix", path)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
    store = {}
    real_open = open
    def fake_open(p, mode="r", *a, **k):                      # the model's output file goes to memory
        return io.StringIO() if "w" in mode else real_open(p, mode, *a, **k)
    class J:
        load = staticmethod(json.load)
        @staticmethod
        def dump(obj, f, **k):
            store["res"] = obj
    m.open = fake_open; m.json = J
    return m, store


class Fixer:
    def __init__(self, model_path, take, sheet=None, max_evals=150):
        self.m, self.store = load_model(model_path)
        self.take, self.sheet = take, sheet or take
        self.max_evals = max_evals; self.evals = 0
        self.knobs = [k for k in LIMIT if hasattr(self.m, k)] + [k for k in ("POLE_L", "POLE_R") if hasattr(self.m, k)]
        self.orig = {k: copy.deepcopy(getattr(self.m, k)) for k in self.knobs}
        self.tried = []; self.accepted = []

    # ---- model + pre-flight ----
    def run(self):
        if hasattr(self.m, "PEL_Y"):
            self.m.PEL_OFF = {k: (0, v, 0) for k, v in self.m.PEL_Y.items()}
        with contextlib.redirect_stdout(io.StringIO()):
            self.m.main()
        self.evals += 1
        mv = PF.Move(None, self.sheet, data=self.store["res"], take=self.take).run()
        return mv

    def cost(self, mv):
        c = 0.0
        for f in mv.flags:
            sev = 1.0
            for k in ("worst_cm", "outside_cm", "pelvis_dip_cm"):
                if isinstance(f.get(k), (int, float)):
                    sev += abs(f[k]) / 3.0
            if "min_max" in f and "range_deg" in f:
                lo, hi = f["range_deg"]; sev += max(f["min_max"][1] - hi, lo - f["min_max"][0], 0) / 2.0
            for k in ("frames", "parts", "feet_cm"):
                if isinstance(f.get(k), list):
                    sev += len(f[k]) / 5.0
            c += SEC_W[f["section"]] * sev
        return c

    # ---- parameter edits ----
    def curve_val(self, name, f):
        d = getattr(self.m, name)
        if f in d:
            return d[f]
        if name == "ANK_ADJ":
            return (0.0, 0.0, 0.0)
        return float(self.m.pchip({k: v for k, v in d.items()})[f][0])

    def change(self, name, f, delta):
        """Add delta to curve `name` at frame f (a key is added at the curve's value when missing). Returns False when the
        change would leave the knob's limit or touch a protected frame."""
        if name not in self.knobs or f is None or f < 0 or f >= self.m.N or self.protected(name, f):
            return False
        d = getattr(self.m, name)
        if name == "ANK_ADJ":
            new = tuple(round(float(a + b), 2) for a, b in zip(self.curve_val(name, f), delta))
            o = self.orig[name].get(f, (0.0, 0.0, 0.0))
            if np.linalg.norm(np.array(new) - np.array(o, float)) > LIMIT[name]:      # the limit is on the change
                return False
        elif name in ("CHEST_LAG", "HEAD_LAG"):
            v = getattr(self.m, name) + delta
            if abs(v - self.orig[name]) > LIMIT[name] or v < 0:
                return False
            setattr(self.m, name, v); return True
        else:
            o = self.orig[name]
            base0 = o[f] if f in o else float(self.m.pchip(dict(o))[f][0])
            new = round(float(self.curve_val(name, f) + delta), 3)
            if abs(new - base0) > LIMIT[name] + 1e-9 or (name == "BAL_W" and not 0.0 <= new <= 1.0):
                return False
            if name == "PEL_Y" and new > 0:                   # never above the idle's pelvis height
                return False
        d[f] = new
        return True

    def protected(self, name, f):
        c0, c1 = self.m.CONTACT; faf = self.m.FAF_K
        hw = self.mv0.hw if hasattr(self, "mv0") else (c0, c1)
        if f >= faf:
            return True
        if name in ("ANK_ADJ", "LEAN", "ARM_L", "ARM_R") and c0 - 1 <= f <= hw[1]:
            return True
        return f in (c0 - 1, c0, c1) and name != "BAL_S"

    # ---- rules -> candidate moves: lists of (name, frame, delta) applied together ----
    def moves(self, f, mv):
        chk = f["check"]; out = []
        frames = f.get("frames") or ([f["frame"]] if isinstance(f.get("frame"), int) else [])
        mid = frames[len(frames) // 2] if frames else None
        if chk.startswith("support knee"):
            up = f["min_max"][1] > f["range_deg"][1]
            for fr in sorted(set(frames)):
                pass
            sgn = 1.0 if up else -1.0
            out.append([("PEL_Y", fr, sgn * 1.0) for fr in sorted({frames[0], mid, frames[-1]})])
        elif chk.startswith("off balance"):
            for fr in frames[:3]:
                out += [[("BAL_W", fr, 0.1)], [("BAL_S", fr, 2.0)], [("BAL_S", fr, -2.0)]]
        elif chk.startswith("weight not shifted"):
            lf = f.get("frame", 0)
            if lf <= 1:                                     # hovering foot: roll the heel / bring the ankle down
                out += [[("HEEL", k, 5.0) for k in (0, 2, 4, 5)], [("ANK_ADJ", k, (0, -1.0, 0)) for k in range(0, 6)]]
            out.append([("BAL_W", k, 0.1) for k in range(max(lf - 3, 0), lf)])
        elif chk.startswith("hips do not drive"):
            out.append([("BAL_S", mv.c0, 2.0)])
        elif chk.startswith("no counterbalance"):
            out.append([("LEAN", mv.c0 - 2, -2.0)])
        elif chk.startswith("no landing absorption"):
            lf = f.get("landing_frame")
            out += [[("PEL_Y", lf + d, -1.5)] for d in (1, 2, 3)] + [[("PEL_Y", lf, 1.0)], [("PEL_Y", lf, 1.0), ("PEL_Y", lf + 2, -1.0)]]
        elif chk.startswith("one-frame pop"):
            for n, tt, _ in f["parts"]:
                a = int(tt.split("->")[0])
                if any(s in n for s in ("calf_", "foot_", "ball_")) and n.endswith("_l"):
                    P = np.array(self.store["res"]["targets"]["foot_MainPoint_l"])
                    mid_pt = 0.5 * (0.5 * (P[a] + P[a + 2]) - P[a + 1])
                    nrm = float(np.linalg.norm(mid_pt))
                    if nrm > 0.9 * LIMIT["ANK_ADJ"]:                  # a partial step inside the limit
                        mid_pt *= 0.9 * LIMIT["ANK_ADJ"] / nrm
                    out.append([("ANK_ADJ", a + 1, tuple(round(float(x), 2) for x in mid_pt))])
        elif chk.startswith("straight paths"):
            for p in f["parts"]:
                n, rest = p.split("@"); a = int(rest.split("-")[0])
                if n.startswith(("foot_", "ball_", "calf_")) and n.endswith("_l"):
                    out += [[("ANK_ADJ", a + 2, (0, -3.0, 0))], [("ANK_ADJ", a + 2, (0, 3.0, 0))]]
                elif n.startswith("head_"):
                    out += [[("PEL_Y", a + 2, -1.0)], [("PEL_Y", a + 2, 1.0)]]
        elif chk.startswith(("velocity reversals", "simultaneous arrivals")) or "peak" in chk:
            out += [[("HEAD_LAG", None, 0.5)], [("CHEST_LAG", None, 0.3)]]
        elif chk.startswith("elbow direction") and "flared" not in chk:
            s = chk.split("(")[1][0]
            if hasattr(self.m, f"POLE_{s.upper()}") and frames:
                out.append([("POLE", s, (frames, frames[-1]))])
        elif chk.startswith("head not toward"):
            for fr in frames[:2]:
                out += [[("CHEST_YAW", fr, 5.0)], [("CHEST_YAW", fr, -5.0)]]
        elif chk.startswith("limb cannot reach") and f["worst"][0].startswith("arm"):
            s = f["worst"][0][-1]; fr = f["worst"][1]
            arm = getattr(self.m, f"ARM_{s.upper()}", {})
            ks = [k for k, v in arm.items() if v[0] == "A" and not self.protected(f"ARM_{s.upper()}", k)]
            if ks:
                k = min(ks, key=lambda k: abs(k - fr)); out.append([(f"ARM_{s.upper()}", k, 0.96)])
        return out

    def apply(self, mv_list):
        snap = {k: copy.deepcopy(getattr(self.m, k)) for k in self.knobs}
        ok = True
        for name, fr, d in mv_list:
            if name == "POLE":                               # R4: elbow down from (first - lag - 1), back-down to (last - lag + 1)
                s = fr; P = getattr(self.m, f"POLE_{s.upper()}"); fl, last = d
                lag = getattr(self.m, f"ARM_LAG_{s.upper()}", 0)
                lag_at = (lambda k: lag(k)) if callable(lag) else (lambda k: lag)
                a = int(math.floor(fl[0] - lag_at(fl[0]) - 1)); b = int(math.ceil(last - lag_at(last) + 1))
                for k in [k for k in P if a < k < b]:
                    del P[k]
                if a >= 0:
                    P[a] = (0.1, -1.0, -0.3)
                if b < self.m.FAF_K:
                    P[b] = (-0.5, -0.85, -0.15)
                continue
            if name.startswith("ARM_"):                      # scale an "A" key to 96% of the arm length (R1)
                arm = getattr(self.m, name); kind, v = arm[fr]; v = np.array(v, float)
                arm[fr] = ("A", tuple(float(x) for x in (v / max(np.linalg.norm(v), 1e-9) * 47.0).round(1))) if np.linalg.norm(v) > 47.0 else arm[fr]
                continue
            if name in ("CHEST_LAG", "HEAD_LAG"):
                ok &= self.change(name, 0, d); continue
            ok &= self.change(name, fr, d)
        return ok, snap

    def restore(self, snap):
        for k, v in snap.items():
            setattr(self.m, k, v)

    def solve(self):
        t0 = time.time()
        mv = self.run(); self.mv0 = mv; start = [f["check"] for f in mv.flags]; best = self.cost(mv)
        while mv.flags and self.evals < self.max_evals:
            improved = False
            for f in sorted(mv.flags, key=lambda x: -SEC_W[x["section"]]):
                for cand in self.moves(f, mv):
                    # line search: repeat the same move while the cost keeps going down
                    for step in range(4):
                        ok, snap = self.apply(cand)
                        if not ok:
                            missing = [n for n, _, _ in cand if n not in self.knobs and n != "POLE" and not n.startswith("ARM_")]
                            why = f"knob not in this model: {','.join(missing)}" if missing else "past the limit or a protected frame"
                            self.restore(snap); self.tried.append((f["check"], str(cand), why)); break
                        new = self.run(); c = self.cost(new)
                        if c < best - 1e-6:
                            best, mv, improved = c, new, True; self.accepted.append((f["check"], str(cand)))
                            if self.evals >= self.max_evals:
                                break
                        else:
                            self.restore(snap); self.tried.append((f["check"], str(cand), f"cost {c:.1f} >= {best:.1f}")); break
                    if improved:
                        break
                if improved or self.evals >= self.max_evals:
                    break
            if not improved:
                break
        self.final = mv; self.secs = time.time() - t0
        return start, mv

    def deltas(self):
        out = {}
        for k in self.knobs:
            a, b = self.orig[k], getattr(self.m, k)
            if a == b:
                continue
            if isinstance(b, dict):
                out[k] = {f: [a.get(f), b[f]] for f in sorted(b) if a.get(f) != b[f]}
            else:
                out[k] = [a, b]
        return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opt = {a.split("=")[0]: a.split("=")[1] for a in sys.argv[1:] if a.startswith("--") and "=" in a}
    model, take = args[0], args[1]
    fx = Fixer(model, take, opt.get("--sheet"), int(opt.get("--max-evals", 150)))
    start, mv = fx.solve()
    dl = fx.deltas()
    ovr = {k: getattr(fx.m, k) for k in dl}
    jp = lambda o: json.loads(json.dumps(o, default=lambda x: list(x) if isinstance(x, tuple) else float(x)))
    if dl:
        json.dump(jp({k: ({str(f): v for f, v in val.items()} if isinstance(val, dict) else val) for k, val in ovr.items()}),
                  open(os.path.join(HERE, f"{take}_overrides.json"), "w"), indent=1)
    lines = [f"AUTOFIX {take}: {len(start)} -> {len(mv.flags)} flag(s), {fx.evals} model runs, {fx.secs:.1f} s",
             "changes:"] + [f"  {k}: {v}" for k, v in dl.items()] + ["remaining flags:"]
    for f in mv.flags:
        extra = {k: v for k, v in f.items() if k not in ("section", "check")}
        lines.append(f"  [{f['section']}] {f['check']}: {json.dumps(extra, default=str)[:160]}")
    tried = {}
    for chk, cand, why in fx.tried:
        if any(chk == f["check"] for f in mv.flags):
            tried.setdefault(chk, []).append(f"{cand[:70]} ({why})")
    lines.append("tried on the remaining flags:")
    for chk, v in tried.items():
        lines.append(f"  {chk[:50]}: " + "; ".join(v[:3]))
    lines.append(f"overrides: {take}_overrides.json (MODEL_OVERRIDES)" if dl else "overrides: none")
    txt = "\n".join(lines)
    open(os.path.join(HERE, "sheets", f"{take}_autofix.txt"), "w", encoding="utf-8").write(txt)
    json.dump({"start": start, "remaining": mv.flags, "deltas": jp({k: {str(f): v for f, v in val.items()} if isinstance(val, dict) else val for k, val in dl.items()}),
               "accepted": fx.accepted, "evals": fx.evals, "seconds": round(fx.secs, 1)},
              open(os.path.join(HERE, "sheets", f"{take}_autofix.json"), "w"), indent=1, default=str)
    print(txt)


if __name__ == "__main__":
    main()
