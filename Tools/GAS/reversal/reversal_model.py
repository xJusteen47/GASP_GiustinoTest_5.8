"""Offline 1-D model of USmoothWalkingMode for a 180 stick flip (skid-reversal-1), to pick the DA_Locomotion_Default Reversal values.

python reversal_model.py [turn] [accel] [frames_of_turn_override] -> prints the speed curve and the time to 420 cm/s the new way. Mirrors
SmoothWalkingMode.cpp GenerateWalkMove (turning ExponentialSmoothingApprox, DirectionalAccelerationFactor 1, critical spring on the
velocity with AccelerationSmoothingTime). Baseline check: turn 16, accel 600 should give the measured rev0e curve (0.576 s).
"""
import math, sys

DT = 0.016


def inv_exp_approx(x):
    return 1.0 / (1.0 + x + 0.48 * x * x + 0.235 * x * x * x)


def crit_spring(x, v, goal, t, dt):
    # SpringMath::CriticalSpringDamper (halflife-free form used by Mover: smoothing time t)
    if t < 1e-6:
        return goal, 0.0
    y = 2.0 * 2.0 / t / 2.0 * 1.0  # damping = 4/t, y = damping/2
    y = 2.0 / t
    j0 = x - goal
    j1 = v + j0 * y
    e = inv_exp_approx(y * dt)
    return e * (j0 + j1 * dt) + goal, e * (v - j1 * y * dt)


def run(turn, acc_fn, top=470.0, v0=612.0, smooth=0.03, frames=60):
    I = v0; S = v0; SA = 0.0; out = []
    D = -top
    for k in range(frames):
        ts = 2.0 / max(turn(k), 1e-6)
        target = -abs(I)
        I = target + (I - target) * inv_exp_approx(DT / ts)
        acc = acc_fn(k)
        accelerating = 1.01 * D * D > S * S
        a = -acc if accelerating else (0.0)
        if not accelerating:
            # deceleration laterally towards D
            a = math.copysign(min(1500.0, abs(D - I) / DT), D - I)
        prev = abs(I)
        diff = D - I
        nxt = I + a * DT if diff * a * DT < diff * diff else D
        nxt = max(-max(prev, top), min(max(prev, top), nxt))
        I = nxt
        S, SA = crit_spring(S, SA, I, smooth, DT)
        out.append(S)
    return out


def t_to(curve, v):
    for k, s in enumerate(curve):
        if s <= -v:
            return round((k + 1) * DT, 3)
    return None


if __name__ == '__main__':
    turn = float(sys.argv[1]) if len(sys.argv) > 1 else 16.0
    acc = float(sys.argv[2]) if len(sys.argv) > 2 else 600.0
    c = run(lambda k: turn, lambda k: acc)
    print('turn', turn, 'acc', acc, 'zero', next(((k + 1) * DT for k, s in enumerate(c) if s <= 0), None), 't300', t_to(c, 300), 't420', t_to(c, 420))
    print([round(x) for x in c[:40]])
