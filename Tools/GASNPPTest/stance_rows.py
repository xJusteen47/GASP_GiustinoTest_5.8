# STANCE-OFFSET: reads pie_capture_move.py captures (frames.jsonl with body.st / body.fl) and writes, per sequence:
#  - the stance timeline: frames where the stance state changes, the montage stop, the foot lock's first step, the return start
#  - while the capsule moves: the largest rest alpha seen with state 3 (must be 0: GASP locomotion never carries the stance)
#  - idle head / hands (actor-relative, cm) averaged over the frames where the stance is fully shown, and the feet over the same frames
# Usage: python stance_rows.py <capture dir> [<capture dir> ...] > rows.txt     (prints at most 5 lines per capture dir to stdout with -q)
import json
import math
import os
import sys


def rows(path):
    with open(path) as f:
        return [json.loads(line) for line in f if line.strip()]


def mean(vs):
    vs = [v for v in vs if v]
    return [round(sum(c) / len(vs), 1) for c in zip(*vs)] if vs else None


def speed(a, b):
    return math.dist(a[:2], b[:2]) * 60.0 if a and b else 0.0


def report(cap):
    out = []
    for seq in sorted(os.listdir(cap)):
        jf = os.path.join(cap, seq, 'frames.jsonl')
        if not os.path.exists(jf):
            continue
        rs = rows(jf)
        events, prev_st, prev_m, prev_fl = [], None, None, None
        moving_alpha, still = 0.0, []
        for i, r in enumerate(rs):
            b = r.get('body') or {}
            st, fl = b.get('st'), b.get('fl')
            m = (r.get('montage') or [None])[0]
            if st and (prev_st is None or st[0] != prev_st[0]):
                events.append(f"f{r['f']}:st{st[0]}(t{st[1]})")
            if m != prev_m:
                events.append(f"f{r['f']}:montage={m}")
            if fl and prev_fl is not None and fl[0] != prev_fl:   # foot lock phases L*10+R (1 locked, 2 step, 3 release)
                events.append(f"f{r['f']}:fl{fl[0]}")
            prev_st, prev_m, prev_fl = st, m, fl[0] if fl else None
            v = speed((rs[i - 1].get('body') or {}).get('actor'), b.get('actor')) if i else 0.0
            if st and v > 15.0 and st[0] == 3:
                moving_alpha = max(moving_alpha, st[2])
            if st and st[0] == 3 and st[1] > 0.5 and not m and v < 5.0:
                still.append(b)
        out.append(f"{seq}: moving_with_stance_alpha_max={moving_alpha} events=" + ' '.join(events))
        if still:
            out.append(f"{seq}: shown_frames={len(still)} head={mean([s.get('head') for s in still])} hand_l={mean([s.get('hand_l') for s in still])} "
                       f"hand_r={mean([s.get('hand_r') for s in still])} foot_l={mean([s.get('foot_l') for s in still])} foot_r={mean([s.get('foot_r') for s in still])}")
        else:
            idle = [(r.get('body') or {}) for r in rs[:15]]
            out.append(f"{seq}: idle_first15 head={mean([s.get('head') for s in idle])} hand_l={mean([s.get('hand_l') for s in idle])} "
                       f"hand_r={mean([s.get('hand_r') for s in idle])} foot_l={mean([s.get('foot_l') for s in idle])} foot_r={mean([s.get('foot_r') for s in idle])}")
    return out


if __name__ == '__main__':
    for cap in [a for a in sys.argv[1:] if a != '-q']:
        print(f'== {cap}')
        for line in report(cap):
            print(line)
