"""Recovery spacing (owner's animation-principles list, 31/10: "slow out of the strike, slow in to the guard"). The striking
limb leaves the hold fast and arrives softly into the guard/idle. Shared by preflight.py (rig points) and motion_check.py
(Unreal bones).

  path      effector positions from the end of the hold to the FAF; arrival = the first frame from which it stays within
            ARRIVE_CM of its FAF pose
  slow out  the speed peak of the recovery lies in its first half (the limb leaves the hold fast); a peak after 60% of the
            way to the arrival = it drifts out of the hold and rushes home
  slow in   the last APPROACH steps into the arrival ease down: the last step <= EASE_MAX x the largest step of the approach;
            steps that are all alike (linear) or that grow into the arrival (a hard stop) are flagged"""
import numpy as np

ARRIVE_CM = 1.0
APPROACH = 4
EASE_MAX = 0.6
PEAK_MAX_SHARE = 0.6


def recovery_spacing(pos, hold_end, faf):
    """pos: (N, 3) effector positions. Returns the table and the verdicts (no thresholds file needed)."""
    pos = np.asarray(pos, float)
    faf = min(faf, len(pos) - 1)
    dist = np.linalg.norm(pos[hold_end:faf + 1] - pos[faf], axis=1)
    far = np.nonzero(dist >= ARRIVE_CM)[0]
    arrive = hold_end + (int(far[-1]) + 1 if len(far) else 0)
    steps = [float(np.linalg.norm(pos[t + 1] - pos[t])) for t in range(hold_end, arrive)]
    res = {"from": hold_end, "arrival": arrive, "steps": [{"frame": hold_end + 1 + i, "step_cm": round(s, 1)} for i, s in enumerate(steps)]}
    if len(steps) < 3:
        res["verdict"] = "too short to judge"; return res, []
    flags = []
    pk = int(np.argmax(steps)); share = (pk + 0.5) / len(steps)
    res["peak_frame"] = hold_end + 1 + pk; res["peak_share"] = round(share, 2)
    if share > PEAK_MAX_SHARE:
        flags.append({"check": "slow out of the hold: the recovery's speed peak comes late (it drifts out, then rushes home)",
                      "peak_frame": res["peak_frame"], "peak_share_of_recovery": round(share, 2), "max": PEAK_MAX_SHARE})
    ap = steps[-APPROACH:]
    ease = ap[-1] / max(max(ap), 1e-6)
    res["approach_steps"] = [round(s, 1) for s in ap]; res["ease_last_over_max"] = round(ease, 2)
    if ease > EASE_MAX:
        kind = "grows into the arrival (hard stop)" if ap[-1] >= max(ap) - 1e-6 else "even (linear)"
        flags.append({"check": f"no slow-in to the guard: final approach spacing {kind}", "approach_steps_cm": res["approach_steps"],
                      "last_over_max": round(ease, 2), "max": EASE_MAX, "arrival": arrive})
    return res, flags


def _selftest():
    t = np.linspace(0, 1, 13)
    eased = np.stack([np.zeros(13), np.zeros(13), 40 * (1 - (1 - t) ** 3)], axis=1)       # fast out, slow in
    linear = np.stack([np.zeros(13), np.zeros(13), 40 * t], axis=1)
    late = np.stack([np.zeros(13), np.zeros(13), 40 * t ** 3], axis=1)                      # slow out, hard stop
    pad = lambda p: np.vstack([p, np.repeat(p[-1:], 3, axis=0)])
    a = recovery_spacing(pad(eased), 0, 15)[1]; b = recovery_spacing(pad(linear), 0, 15)[1]; c = recovery_spacing(pad(late), 0, 15)[1]
    return not a and len(b) >= 1 and len(c) == 2


if __name__ == "__main__":
    print("selftest", _selftest())
