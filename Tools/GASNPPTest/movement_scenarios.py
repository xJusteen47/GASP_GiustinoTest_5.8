# Movement QA (qa-movement-1117): in-game checks of RUN / SPRINT / SKID and JUMP / AIR that judge state, movement and animation together, on
# the authority ('srv') and the owner ('cli'). Run them with the fresh-PIE batch driver (batch_driver.py, BATCH_MODULE=movement_scenarios) in a
# RENDERING editor (no -nullrhi: the anim graph only ticks a pose that is drawn). Each scenario writes its numbers to
# Saved/Logs/qam/<scenario>.json, so the handover can cite a file instead of a log.
#
# What is read: the sim ticks of both copies (UGSTN_GASNPPTestComponent.DrainSimHistory: loc, vel, yaw, mode, run stage, skid phase and the
# skid's Mover tags) and, every editor tick, the owner's presented frame (GetPresentationSnapshot: foot slide, skid lean, montage, override
# layer, bone heights over the capsule bottom, the anim Blueprint's own variables). Targets come from the Mario 3D World preset
# (handoffs/preset-mario3dw.md), the jump from the hold-gravity model of 2026-11-18 (docs/notes/handovers/jump-hold-gravity.md): full jump
# apex about 1.9 character heights, air time about 0.75 s, tap about 0.32 of the full jump; a run start
# ramp of about 6 frames, air turn of 180 degrees in about 3 frames. Checks have a tolerance band around the target and print the measure.
import json
import math
import os

import unreal
import gasnpp_test as g

SUITE_MOVE = ['move_run_stages', 'move_skid', 'move_analog_walk', 'move_jump_full', 'move_jump_tap', 'move_air_control', 'move_fast_fall']
FRAME = 0.016          # the fixed step, 62.5 Hz
SKID_TAG = 'Movement.State.Skid'
SKID_WINDOW_TAG = 'Movement.State.Skid.JumpWindow'
# The kit the owner plays (it carries DA_Locomotion_Default through FGSTN_LocomotionKitExtension; a kit without it has no run stages, skid or
# analog walk at all). QAM_KIT='' keeps the pawn's own kit
KIT = os.environ.get('QAM_KIT', 'KIT_Test_Playtest')
OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Logs', 'qam')


# ------------------------------------------------------------------------------------------------ helpers
def _host_pawn(key):
    c = g.comps(g.worlds()[0]).get(key)
    return c.get_owner() if c else None


def _hspeed(v):
    return math.hypot(v[0], v[1])


def _along(v, d):
    return v[0] * d[0] + v[1] * d[1]


def _hit_distance(hit, length):
    try:
        t = hit.to_tuple()
        return float(t[3]) if float(t[3]) > 0 else float(t[2]) * length
    except Exception:
        return 0.0


def _open_dir(key, need=2600.0):
    """The flat direction from the host's copy of the pawn with the longest free run (no wall at chest height, floor every 200 cm), up to `need`"""
    pawn = _host_pawn(key)
    loc = pawn.get_actor_location()
    best = (0.0, (1.0, 0.0))
    for k in range(16):
        a = 2.0 * math.pi * k / 16.0
        d = (math.cos(a), math.sin(a))
        end = unreal.Vector(loc.x + d[0] * need, loc.y + d[1] * need, loc.z + 20.0)
        hit = unreal.SystemLibrary.line_trace_single(pawn, unreal.Vector(loc.x, loc.y, loc.z + 20.0), end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                     False, [pawn], unreal.DrawDebugTrace.NONE, True)
        free = need if not hit else _hit_distance(hit, need)
        step = 200.0
        dist = step
        while dist <= free:
            p = unreal.Vector(loc.x + d[0] * dist, loc.y + d[1] * dist, loc.z)
            ground = unreal.SystemLibrary.line_trace_single(pawn, p, unreal.Vector(p.x, p.y, p.z - 250.0), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                            False, [pawn], unreal.DrawDebugTrace.NONE, True)
            if not ground:
                break
            dist += step
        free = min(free, dist - step)
        if free > best[0]:
            best = (free, d)
    return best


def _stick_for(client, d, mag):
    """IA_Move value (right, forward) for the world direction d at stick size mag, relative to the client's control yaw"""
    pc, _ = g._local_pawn(client)
    yaw = math.radians(pc.get_control_rotation().yaw)
    fwd, right = (math.cos(yaw), math.sin(yaw)), (-math.sin(yaw), math.cos(yaw))
    return (mag * (d[0] * right[0] + d[1] * right[1]), mag * (d[0] * fwd[0] + d[1] * fwd[1]), 0.0)


class _Q:
    """One movement QA scenario: the _Run sampler, a stick driver (world direction + size, re-aimed every tick), the owner's presentation
    sampler and the metrics file."""

    def __init__(self, name, client):
        self.name, self.client = name, client
        self.run = g._Run(name, client)
        self.key = self.run.key
        self.stick = {'dir': None, 'mag': 0.0, 'held': False}
        self.pres, self.metrics, self.abp_keys = [], {'scenario': name}, None
        self.dir, self.free = (1.0, 0.0), 0.0
        self.h = unreal.register_slate_post_tick_callback(self._tick)

    def _tick(self, _dt):
        try:
            if self.stick['dir'] is not None and self.stick['mag'] > 0.0:
                g.hold(self.client, 'move', _stick_for(self.client, self.stick['dir'], self.stick['mag']))
                self.stick['held'] = True
            elif self.stick['held']:
                g.release(self.client, 'move')
                self.stick['held'] = False
            c = g.comps(g.worlds()[self.client]).get(self.key)
            if c is not None:
                p = json.loads(c.get_presentation_snapshot())
                if self.abp_keys is None and p.get('abp'):
                    self.abp_keys = sorted(p['abp'].keys())
                p['t'] = self.run.now()
                self.pres.append(p)
        except Exception:
            pass

    # -- steps
    def kit(self):
        def f():
            before = str((g._loadout_view(0, self.key)[0] or {}).get('kit'))
            self.metrics['kit_before'] = before
            if not KIT:
                return f'kit kept: {before}', True
            ok = g._apply_loadout(self.key, KIT)
            self.metrics['kit'] = KIT
            return f'kit {before} -> {KIT}: {ok}', bool(ok)
        return f
    def set_stick(self, d, mag, label):
        def f():
            self.stick.update(dir=d, mag=mag)
            return label, True
        return f

    def find_dir(self, need=2600.0):
        def f():
            self.free, self.dir = _open_dir(self.key, need)
            self.metrics['free_run_cm'] = round(self.free)
            return f'open direction {tuple(round(x, 2) for x in self.dir)} free {self.free:.0f} cm', self.free > 600.0
        return f

    def flip(self):
        return (-self.dir[0], -self.dir[1])

    def side(self):
        return (-self.dir[1], self.dir[0])

    def finish(self):
        inner = self.run.finish()

        def f():
            unreal.unregister_slate_post_tick_callback(self.h)
            g.release(self.client, 'move')
            g.combat_clear(self.client)
            verdict = inner()
            self.metrics['verdict'] = verdict
            self.metrics['abp_variables'] = self.abp_keys
            self.metrics['checks'] = [list(r) for r in self.run.results]
            try:
                os.makedirs(OUT, exist_ok=True)
                with open(os.path.join(OUT, self.name + '.json'), 'w') as fh:
                    json.dump(self.metrics, fh, indent=1, default=str)
            except Exception as e:
                unreal.log_warning(f'qam metrics not written: {e}')
            return verdict
        return f

    # -- queries
    def ticks(self, side, t0, t1):
        return self.run.final_sim(side, t0, t1)

    def pres_win(self, t0, t1):
        return [p for p in self.pres if t0 <= p['t'] <= t1]

    def check(self, label, fn):
        return self.run.at(label, fn)


def _both(fn):
    a, b = fn('srv'), fn('cli')
    return bool(a[0] and b[0]), f'srv: {a[1]} | cli: {b[1]}'


def _capsule_height(key):
    pawn = _host_pawn(key)
    try:
        return 2.0 * pawn.get_component_by_class(unreal.CapsuleComponent).get_scaled_capsule_half_height()
    except Exception:
        return 180.0


def _jump(client, hold_s):
    """A jump press; with hold_s > 0 the button stays held (the caller releases it with combat_clear)"""
    mask = g.button_mask('Jump')
    g.combat_tap(client, mask)
    if hold_s > 0:
        g.combat_hold(client, mask)
    return f'jump (hold {hold_s}s)', True


def _flight(q, side, t0, t1):
    """The first flight in [t0, t1] on the sim ticks: dict(z0, apex, takeoff_ms, apex_ms, land_ms, ticks) or None"""
    w = q.ticks(side, t0, t1)
    if len(w) < 3:
        return None
    z0 = w[0]['loc'][2]
    up = next((i for i, s in enumerate(w) if s['loc'][2] > z0 + 1.0), None)
    if up is None:
        return None
    top = max(range(up, len(w)), key=lambda i: w[i]['loc'][2])
    land = next((i for i in range(top, len(w)) if w[i]['loc'][2] <= z0 + 2.0), None)
    return {'z0': z0, 'apex': w[top]['loc'][2] - z0, 'takeoff_ms': w[up - 1]['ms'], 'apex_ms': w[top]['ms'],
            'land_ms': w[land]['ms'] if land is not None else None, 'ticks': w, 'up': up, 'top': top, 'land': land}


def _feet_motion(pres):
    """Range of the left foot height over the capsule bottom (cm) in presented frames: legs that animate move it"""
    zs = [p.get('pose', {}).get('foot_l') for p in pres if p.get('pose', {}).get('foot_l') is not None]
    return (max(zs) - min(zs)) if zs else 0.0


# ------------------------------------------------------------------------------------------------ RUN / SPRINT
def scenario_move_run_stages(client=1):
    """From rest, full stick along the freest direction for 3.6 s, then released. Run start: the pawn moves on its first stick frame, does not
    reach the run speed at once (no step) and gets to 90 % of it within 20 frames (the ramp, ~6 f by data). Stages: at least run, sprint 1 and
    sprint 2 in order, each faster than the last. Release: stops within 45 frames. Animation: no montage, no override layer, the legs move while
    running. Owner and authority end at the same place."""
    q = _Q('move_run_stages', client)
    T, HOLD = 1.0, 3.6

    def start():
        def side(sd):
            w = [s for s in q.ticks(sd, T - 0.2, T + HOLD + 2.2) if s.get('runfr', -1) >= 0]
            if not w:
                return False, 'no run frames'
            moving = [s for s in w if s['runfr'] >= 0]
            cap = max(_hspeed(s['vel']) for s in moving if s['runfr'] <= 56)
            by = {}
            for s in moving:
                by.setdefault(s['runfr'], _hspeed(s['vel']))
            f1 = by.get(1, by.get(0, 0.0))
            to90 = next((f for f in sorted(by) if by[f] >= 0.9 * cap), None)
            q.metrics.setdefault('start', {})[sd] = {'speed_by_frame': {k: round(v) for k, v in sorted(by.items())[:12]}, 'cap_first_0.9s': round(cap), 'frames_to_90pct': to90}
            ok = f1 > 1.0 and f1 < 0.6 * cap and to90 is not None and to90 <= 20
            return ok, f'frame-1 speed {f1:.0f}, cap {cap:.0f}, 90% at frame {to90}'
        return _both(side)

    def stages():
        def side(sd):
            w = [s for s in q.ticks(sd, T - 0.2, T + HOLD + 2.2) if s.get('runfr', -1) >= 0]
            seq, plateau, frames = [], {}, {}
            for s in w:
                st = s.get('stage', -1)
                if st < 0:
                    continue
                if not seq or seq[-1] != st:
                    seq.append(st)
                plateau[st] = max(plateau.get(st, 0.0), _hspeed(s['vel']))
                frames[st] = frames.get(st, 0) + 1
            q.metrics.setdefault('stages', {})[sd] = {'sequence': seq, 'max_speed': {k: round(v) for k, v in plateau.items()}, 'frames': frames,
                                                      'trace': [(s['runfr'], s['stage'], round(_hspeed(s['vel']))) for s in w][::4]}
            rising = list(seq)   # stage 0 = run, 1 = sprint 1, 2 = sprint 2
            speeds = [plateau[st] for st in sorted(set(rising))]
            ok = len(set(rising)) >= 3 and rising == sorted(rising) and all(b > a * 1.05 for a, b in zip(speeds, speeds[1:]))
            return ok, f'stages {seq}, max speeds {[round(x) for x in speeds]}, frames {frames}'
        return _both(side)

    def stop():
        def side(sd):
            allw = q.ticks(sd, T - 0.2, T + HOLD + 2.2)
            last = max((i for i, s in enumerate(allw) if s.get('runfr', -1) >= 0), default=0)
            w = allw[last:]
            v0 = _hspeed(w[0]['vel']) if w else 0.0
            i0 = next((i for i, s in enumerate(w) if _hspeed(s['vel']) < v0 - 5.0), None)
            i1 = next((i for i, s in enumerate(w) if _hspeed(s['vel']) < 10.0), None)
            n = None if i0 is None or i1 is None else i1 - i0
            q.metrics.setdefault('stop', {})[sd] = {'from_speed': round(v0), 'frames': n}
            return n is not None and n <= 45, f'from {v0:.0f} cm/s to rest in {n} frames'
        return _both(side)

    def anim():
        p = q.pres_win(T + 0.5, T + HOLD)
        mont = sorted({x.get('montage') for x in p if x.get('montage')})
        ovr = any(x.get('override') for x in p)
        feet = _feet_motion(p)
        q.metrics['anim_run'] = {'montages': mont, 'override': ovr, 'foot_l_range_cm': round(feet, 1), 'samples': len(p),
                                 'abp_last': (p[-1].get('abp') if p else None)}
        return bool(p) and not mont and not ovr and feet > 8.0, f'{len(p)} frames, montages {mont}, override {ovr}, left foot moves {feet:.1f} cm'

    def agree():
        a, b = q.run.final_by_frame('srv', T + HOLD + 1.0, T + HOLD + 2.2), q.run.final_by_frame('cli', T + HOLD + 1.0, T + HOLD + 2.2)
        common = sorted(set(a) & set(b))
        if not common:
            return False, 'no common frame'
        f = common[-1]
        d = math.dist(a[f]['loc'], b[f]['loc'])
        return d < 5.0, f'frame {f}: owner vs authority {d:.2f} cm'

    steps = [
        (0.3, q.kit()),
        (0.6, q.find_dir()),
        (T, lambda: ('full stick', q.stick.update(dir=q.dir, mag=1.0) or True)),
        (T + HOLD, q.set_stick(None, 0.0, 'release')),
        (T + HOLD + 2.4, q.check('run start: moves on frame 1, ramps, 90% within 20 frames', start)),
        (T + HOLD + 2.4, q.check('stages: run, sprint 1, sprint 2 in order, each faster', stages)),
        (T + HOLD + 2.4, q.check('release: rest within 45 frames', stop)),
        (T + HOLD + 2.4, q.check('anim while running: no montage, no override, legs move', anim)),
        (T + HOLD + 2.4, q.check('owner and authority end at the same place', agree)),
        (T + HOLD + 2.6, q.finish()),
    ]
    q.run.run(steps)


def scenario_move_skid(client=1):
    """Run for 1.6 s (past the skid's minimum speed), then the stick full against the run. The skid starts (Movement.State.Skid on both copies
    within 0.25 s), its jump window shows, the speed along the old direction only falls during it (braking, mean deceleration printed), the
    owner's presentation shows the foot slide of the skid with the lean, and after it the pawn runs the new way facing it."""
    q = _Q('move_skid', client)
    T, RUN, REV = 1.0, 1.6, 1.4
    TR = T + RUN
    store = {}

    def skid_tags():
        def side(sd):
            w = q.ticks(sd, TR - 0.05, TR + REV + (0.6 if sd == 'srv' else 0.0))
            first = next((s['ms'] for s in w if SKID_TAG in s['tags']), None)
            win = any(SKID_WINDOW_TAG in s['tags'] for s in w)
            t0 = w[0]['ms'] if w else None
            delay = None if first is None or t0 is None else (first - t0) / 1000.0
            q.metrics.setdefault('skid_start', {})[sd] = {'delay_s': delay, 'jump_window': win}
            ok = (first is not None) if sd == 'srv' else (delay is not None and delay <= 0.25)
            return ok and win, f'skid after {delay} s (wall clock, judged on the owner), jump window {win}'
        return _both(side)

    def braking():
        def side(sd):
            d = store['dir']
            w = [s for s in q.ticks(sd, TR - 0.05, TR + REV + 0.6) if s.get('skid', -1) in (1, 2)]
            if len(w) < 3:
                return False, f'{len(w)} skid frames'
            sp = [_along(s['vel'], d) for s in w]
            ups = sum(1 for a, b in zip(sp, sp[1:]) if b > a + 2.0)
            dist = math.dist(w[0]['loc'][:2], w[-1]['loc'][:2])
            dt = (w[-1]['ms'] - w[0]['ms']) / 1000.0
            dec = (sp[0] - sp[-1]) / dt if dt > 0 else 0.0
            q.metrics.setdefault('skid_brake', {})[sd] = {'frames': len(w), 'from': round(sp[0]), 'to': round(sp[-1]), 'mean_decel': round(dec), 'slide_cm': round(dist)}
            return ups == 0 and dec > 1000.0, f'{len(w)} frames {sp[0]:.0f}->{sp[-1]:.0f} cm/s, mean decel {dec:.0f} cm/s2, slide {dist:.0f} cm, speed-ups {ups}'
        return _both(side)

    def turned():
        def side(sd):
            d = store['dir']
            w = q.ticks(sd, TR + REV - 0.3 + (0.3 if sd == 'srv' else 0.0), TR + REV + (0.3 if sd == 'srv' else 0.0))
            v = max((-_along(s['vel'], d) for s in w), default=0.0)
            yaw = w[-1].get('yaw') if w else None
            want = math.degrees(math.atan2(-d[1], -d[0]))
            err = abs((yaw - want + 180.0) % 360.0 - 180.0) if yaw is not None else 999.0
            return v > 200.0 and err < 30.0, f'speed the new way {v:.0f} cm/s, facing off by {err:.0f} deg'
        return _both(side)

    def anim():
        p = q.pres_win(TR - 0.1, TR + REV)
        slide = [x for x in p if (x.get('slide') or {}).get('src') == 1]
        lean = max((x.get('lean', 0.0) for x in p), default=0.0)
        mont = sorted({x.get('montage') for x in p if x.get('montage')})
        q.metrics['anim_skid'] = {'slide_frames': len(slide), 'max_lean_deg': round(lean, 1), 'montages': mont,
                                  'abp_mid_skid': (slide[len(slide) // 2].get('abp') if slide else None)}
        return len(slide) >= 3 and lean > 1.0, f'owner shows the skid slide on {len(slide)} frames, lean up to {lean:.1f} deg, montages {mont}'

    def go():
        store['dir'] = q.dir
        q.stick.update(dir=q.dir, mag=1.0)
        return 'run', True

    def rev():
        q.stick.update(dir=q.flip(), mag=1.0)
        return 'stick against the run', True

    steps = [
        (0.3, q.kit()),
        (0.6, q.find_dir()),
        (T, go),
        (TR, rev),
        (TR + REV, q.set_stick(None, 0.0, 'release')),
        (TR + REV + 0.8, q.check('skid starts on both copies within 0.25 s, jump window shown', skid_tags)),
        (TR + REV + 0.8, q.check('skid brakes: the old-way speed only falls', braking)),
        (TR + REV + 0.8, q.check('after the skid: runs the new way, facing it', turned)),
        (TR + REV + 0.8, q.check('anim: owner shows the skid foot slide and lean', anim)),
        (TR + REV + 1.0, q.finish()),
    ]
    q.run.run(steps)


def scenario_move_analog_walk(client=1):
    """Stick tilts 0.1, 0.2, 0.35, 0.6, 1.0 held 1.1 s each along the free direction. Every tilt moves the pawn (no dead zone left over the
    player's own), the speed grows with the tilt and the lowest tilt is a slow walk (under 35 % of full), with no jump between the two lowest
    tilts bigger than the full-tilt step. The legs move at the lowest walking tilt."""
    q = _Q('move_analog_walk', client)
    T, D = 1.0, 1.1
    tilts = [0.12, 0.2, 0.35, 0.6, 1.0]

    def speeds_of(sd):
        out = []
        for i, _ in enumerate(tilts):
            w = q.ticks(sd, T + i * D + D - 0.4, T + i * D + D)
            out.append(sum(_hspeed(s['vel']) for s in w) / len(w) if w else 0.0)
        return out

    def ladder():
        def side(sd):
            sp = speeds_of(sd)
            q.metrics.setdefault('tilt_speeds', {})[sd] = dict(zip(map(str, tilts), map(round, sp)))
            ok = sp[0] > 5.0 and all(b > a for a, b in zip(sp, sp[1:])) and sp[0] < 0.35 * sp[-1]
            return ok, 'speeds ' + ', '.join(f'{t}:{s:.0f}' for t, s in zip(tilts, sp))
        return _both(side)

    def legs():
        p = q.pres_win(T + D * 1 + 0.3, T + D * 2)
        feet = _feet_motion(p)
        q.metrics['anim_walk_0.2'] = {'foot_l_range_cm': round(feet, 1), 'abp': (p[-1].get('abp') if p else None)}
        return feet > 4.0, f'left foot moves {feet:.1f} cm at tilt 0.2'

    steps = [(0.3, q.kit()), (0.6, q.find_dir(1800.0))]
    for i, m in enumerate(tilts):
        steps.append((T + i * D, (lambda m=m: (f'tilt {m}', q.stick.update(dir=q.dir, mag=m) or True))))
    steps += [
        (T + len(tilts) * D, q.set_stick(None, 0.0, 'release')),
        (T + len(tilts) * D + 0.6, q.check('analog walk: every tilt moves, speed grows, low tilt is slow', ladder)),
        (T + len(tilts) * D + 0.6, q.check('anim: legs move at tilt 0.2', legs)),
        (T + len(tilts) * D + 0.8, q.finish()),
    ]
    q.run.run(steps)


# ------------------------------------------------------------------------------------------------ JUMP / AIR
def scenario_move_jump_full(client=1):
    """A jump from rest with the button held 0.4 s. Apex 1.6..2.4 character heights (target 1.9), the rise slows gently while held (hold
    gravity 2.6 g: the slowest of the first 8 rising frames is at least 70 % of the first, expected ~79 %), air time 0.6..1.1 s (target 0.75). Owner and authority
    agree on the apex within 2 cm. Animation: no montage, the presented pose changes in the air (the feet leave their ground height)."""
    q = _Q('move_jump_full', client)
    T = 1.2
    H = {}

    def apex():
        H['h'] = _capsule_height(q.key)

        def side(sd):
            f = _flight(q, sd, T - 0.1, T + 2.0)
            if not f:
                return False, 'no flight'
            ratio = f['apex'] / H['h']
            air = None if f['land_ms'] is None else (f['land_ms'] - f['takeoff_ms']) / 1000.0
            rise = [s['vel'][2] for s in f['ticks'][f['up']:f['up'] + 8]]
            keep = (min(rise) / max(rise[0], 1.0)) if rise else 0.0
            q.metrics.setdefault('jump_full', {})[sd] = {'apex_cm': round(f['apex'], 1), 'heights': round(ratio, 2), 'air_s': air,
                                                         'rise_vz_first8': [round(x) for x in rise], 'apex_s': (f['apex_ms'] - f['takeoff_ms']) / 1000.0}
            # Hold-gravity jump (2026-11-18, early 2D Mario defaults): held 0.4 s ~335 cm = ~1.9 heights, air ~0.75 s; the hold gravity (2.6 g)
            # keeps ~0.79 of the take-off speed after 8 frames (it was constant with the Mario 3D World rise scale 0)
            ok = 1.6 <= ratio <= 2.4 and air is not None and 0.6 <= air <= 1.1 and keep >= 0.7
            return ok, f'apex {f["apex"]:.0f} cm = {ratio:.2f} heights ({H["h"]:.0f} cm), air {air} s, rise speed kept {keep:.2f}'
        return _both(side)

    def agree():
        a, b = _flight(q, 'srv', T - 0.1, T + 2.0), _flight(q, 'cli', T - 0.1, T + 2.0)
        if not a or not b:
            return False, 'no flight'
        return abs(a['apex'] - b['apex']) < 2.0, f'apex srv {a["apex"]:.1f} cli {b["apex"]:.1f}'

    def anim():
        p = q.pres_win(T - 0.3, T + 1.6)
        ground = [x['pose'].get('foot_l') for x in q.pres_win(T - 0.5, T - 0.05) if x.get('pose', {}).get('foot_l') is not None]
        air = [x['pose'].get('foot_l') for x in q.pres_win(T + 0.2, T + 0.6) if x.get('pose', {}).get('foot_l') is not None]
        mont = sorted({x.get('montage') for x in p if x.get('montage')})
        dz = (max(air) - (sum(ground) / len(ground))) if air and ground else 0.0
        q.metrics['anim_jump'] = {'montages': mont, 'foot_l_ground': round(sum(ground) / len(ground), 1) if ground else None,
                                  'foot_l_air_max': round(max(air), 1) if air else None, 'abp_air': (q.pres_win(T + 0.4, T + 0.45) or [{}])[-1].get('abp')}
        return not mont and dz > 5.0, f'montages {mont}, left foot rises {dz:.1f} cm over its ground pose in the air'

    steps = [
        (0.3, q.kit()),
        (0.5, lambda: ('jump kit (no-op if the kit has it)', g._srv(q.key).server_set_jump_kit(True))),
        (T, lambda: _jump(client, 0.4)),
        (T + 0.4, lambda: ('release', g.combat_clear(client))),
        (T + 2.2, q.check('full jump: apex ~1.9 heights, air ~0.75 s, slow deceleration while held', apex)),
        (T + 2.2, q.check('owner and authority agree on the apex', agree)),
        (T + 2.2, q.check('anim: no montage, feet leave the ground pose in the air', anim)),
        (T + 2.4, q.finish()),
    ]
    q.run.run(steps)


def scenario_move_jump_tap(client=1):
    """A full jump (held 0.4 s), then a tap (one frame). The tap's apex is 0.2..0.45 of the full one (target ~0.32: 107 / 335 cm with the
    hold-gravity defaults of 2026-11-18, no minimum hold) and, once the button is up, gravity is the cut gravity: the vertical speed falls by
    at least 3000 cm/s2 (cut 9 g, reached over the cut ramp) in frames 4..8 after the release."""
    q = _Q('move_jump_tap', client)
    TA, TB = 1.2, 3.6

    def ratio():
        def side(sd):
            a, b = _flight(q, sd, TA - 0.1, TA + 2.0), _flight(q, sd, TB - 0.1, TB + 2.0)
            if not a or not b:
                return False, 'missing flight'
            r = b['apex'] / max(a['apex'], 1.0)
            w = b['ticks'][b['up']:]
            v = [s['vel'][2] for s in w[4:9]]
            dec = (v[0] - v[-1]) / (FRAME * (len(v) - 1)) if len(v) > 1 else 0.0
            q.metrics.setdefault('jump_tap', {})[sd] = {'full_cm': round(a['apex'], 1), 'tap_cm': round(b['apex'], 1), 'ratio': round(r, 2), 'cut_decel': round(dec)}
            return 0.2 <= r <= 0.45 and dec >= 3000.0, f'tap {b["apex"]:.0f} / full {a["apex"]:.0f} = {r:.2f}, cut deceleration {dec:.0f} cm/s2'
        return _both(side)

    steps = [
        (0.3, q.kit()),
        (0.5, lambda: ('jump kit', g._srv(q.key).server_set_jump_kit(True))),
        (TA, lambda: _jump(client, 0.4)),
        (TA + 0.4, lambda: ('release', g.combat_clear(client))),
        (TB, lambda: _jump(client, 0.0)),
        (TB + 2.2, q.check('tap jump: lower apex, cut gravity after the release', ratio)),
        (TB + 2.4, q.finish()),
    ]
    q.run.run(steps)


def scenario_move_air_control(client=1):
    """A: run 0.9 s, jump (held 0.3 s), 0.12 s after take-off the stick full against the run: the horizontal speed turns the new way before the
    landing (AirControl 0.85) and the pawn faces the new way within 8 frames of the flip (air turn 3600 deg/s target: 180 in ~3 frames).
    B: a jump from rest, the stick pushed sideways at take-off: the air speed reaches at least 300 cm/s (MinAirSpeed 470) before landing."""
    q = _Q('move_air_control', client)
    TA, TB = 1.0, 4.4
    store = {}

    def go():
        store['dir'] = q.dir
        q.stick.update(dir=q.dir, mag=1.0)
        return 'run', True

    def flip():
        store['flip_t'] = q.run.now()
        q.stick.update(dir=q.flip(), mag=1.0)
        return 'stick against the run in the air', True

    def reversal():
        def side(sd):
            d = store['dir']
            f = _flight(q, sd, TA + 0.8, TA + 2.6)
            if not f:
                return False, 'no flight'
            air = f['ticks'][f['up']:f['land'] or len(f['ticks'])]
            neg = next((i for i, s in enumerate(air) if _along(s['vel'], d) < 0.0), None)
            want = math.degrees(math.atan2(-d[1], -d[0]))
            # The flip on this copy's clock: the first air tick whose speed along the run falls by more than 5 cm/s
            flip = next((i for i in range(1, len(air)) if _along(air[i]['vel'], d) < _along(air[i - 1]['vel'], d) - 5.0), None)
            near = air[flip:] if flip is not None else []
            turned = next((i for i, s in enumerate(near) if s.get('yaw') is not None and abs((s['yaw'] - want + 180.0) % 360.0 - 180.0) < 30.0), None)
            q.metrics.setdefault('air_reverse', {})[sd] = {'air_frames': len(air), 'flip_air_frame': flip, 'frames_to_reverse_speed': neg, 'frames_to_face': turned,
                                                           'yaw_trace': [round(s.get('yaw') or 0) for s in near[:40]], 'want_yaw': round(want),
                                                           'owner_turn_rate_maxspeed_airborne': [(p.get('turnRate'), p.get('maxSpeed'), p.get('runAirborne')) for p in q.pres_win(TA + 0.8, TA + 2.0)][::6]}
            return neg is not None and turned is not None and turned <= 8, f'speed reversed after {neg} of {len(air)} air frames, facing the new way after {turned} frames'
        return _both(side)

    def side_speed():
        def sd_fn(sd):
            f = _flight(q, sd, TB - 0.1, TB + 2.0)
            if not f:
                return False, 'no flight'
            air = f['ticks'][f['up']:f['land'] or len(f['ticks'])]
            v = max((_hspeed(s['vel']) for s in air), default=0.0)
            q.metrics.setdefault('air_side', {})[sd] = {'max_air_speed': round(v)}
            return v >= 300.0, f'max air speed from rest {v:.0f} cm/s'
        return _both(sd_fn)

    steps = [
        (0.3, q.kit()),
        (0.6, q.find_dir()),
        (0.5, lambda: ('jump kit', g._srv(q.key).server_set_jump_kit(True))),
        (TA, go),
        (TA + 0.9, lambda: _jump(client, 0.3)),
        (TA + 1.2, lambda: ('release jump', g.combat_clear(client))),
        (TA + 1.02, flip),
        (TA + 2.4, q.set_stick(None, 0.0, 'release')),
        (TB, lambda: _jump(client, 0.3)),
        (TB + 0.02, lambda: ('stick sideways', q.stick.update(dir=q.side(), mag=1.0) or True)),
        (TB + 0.3, lambda: ('release jump', g.combat_clear(client))),
        (TB + 1.6, q.set_stick(None, 0.0, 'release')),
        (TB + 2.2, q.check('air reverse: speed turns before landing, facing within 8 frames', reversal)),
        (TB + 2.2, q.check('air control from rest: at least 300 cm/s sideways', side_speed)),
        (TB + 2.4, q.finish()),
    ]
    q.run.run(steps)


def scenario_move_fast_fall(client=1):
    """A plain full jump, then the same jump with a crouch tap 0.1 s after the apex. The fast fall shows in the jump state on both copies and
    its fastest descent is at least 1.2 times the plain one's and at most 2730 cm/s (FastFallSpeed 2600 + 5 %); it lands sooner."""
    q = _Q('move_fast_fall', client)
    TA, TB = 1.2, 3.8
    crouch = g.button_mask('Crouch')

    def fall_stats(sd, t0):
        f = _flight(q, sd, t0 - 0.1, t0 + 2.2)
        if not f:
            return None
        down = min((s['vel'][2] for s in f['ticks'][f['top']:f['land'] or len(f['ticks'])]), default=0.0)
        land = None if f['land_ms'] is None else (f['land_ms'] - f['apex_ms']) / 1000.0
        return {'apex_ms': f['apex_ms'], 'max_down': -down, 'apex_to_land_s': land, 'apex_s': (f['apex_ms'] - f['takeoff_ms']) / 1000.0}

    def compare():
        def side(sd):
            a, b = fall_stats(sd, TA), fall_stats(sd, TB)
            if not a or not b:
                return False, 'missing flight'
            q.metrics.setdefault('fast_fall', {})[sd] = {'plain': a, 'fast': b}
            ok = b['max_down'] > a['max_down'] + 100.0 and b['max_down'] <= 2730.0 and (b['apex_to_land_s'] or 9) <= 0.9 * (a['apex_to_land_s'] or 0)
            return ok, f'fastest descent plain {a["max_down"]:.0f} fast {b["max_down"]:.0f} cm/s, apex to landing {a["apex_to_land_s"]} vs {b["apex_to_land_s"]} s'
        return _both(side)

    def flag():
        def side(sd):
            seen = any((s.get(sd) or {}).get('jump', {}).get('fastFalling') for s in q.run.samples if TB <= s['t'] <= TB + 1.6)
            return seen, f'fastFalling seen {seen}'
        return _both(side)


    steps = [
        (0.3, q.kit()),
        (0.5, lambda: ('jump kit', g._srv(q.key).server_set_jump_kit(True))),
        (TA, lambda: _jump(client, 0.4)),
        (TA + 0.4, lambda: ('release', g.combat_clear(client))),
        (TB, lambda: _jump(client, 0.4)),
        (TB + 0.4, lambda: ('release', g.combat_clear(client))),
    ]
    # Crouch taps after the apex (a full jump peaks ~0.49 s after take-off with the preset; a tap while rising is refused, which is fine)
    def crouch_at():
        def f():
            return 'crouch tap (fast fall)', g.combat_tap(client, crouch)
        return f
    steps += [
        (TB + 0.55, crouch_at()),
        (TB + 0.6, crouch_at()),
        (TB + 2.4, q.check('fast fall flag on both copies', flag)),
        (TB + 2.4, q.check('fast fall: faster than the plain fall (+100 cm/s, capped), lands 10% sooner', compare)),
        (TB + 2.6, q.finish()),
    ]
    q.run.run(steps)
