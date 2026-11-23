# NIGHT-FOOTLOCK: foot slide after a montage, per sequence of a capture (pie_capture_move.py or pie_capture_react_ingame.py output).
# Window: from the first frame without a montage after montage frames, WIN frames long, cut where the capsule has moved > 2 cm from its start.
# slide = horizontal world travel of each foot summed over frames where that foot is NOT stepping (lock phase 2), step = travel while stepping,
# end = distance from the window-start spot to the final spot. Usage: python footlock_rows.py <capture dir> [<capture dir> ...] > rows.txt
import json, math, os, sys

WIN = 80


def world(r, b):
    body = r.get('body')
    if body is not None:   # move capture: actor-relative
        if b not in body:
            return None
        return [body['actor'][i] + body[b][i] for i in range(3)]
    return r.get(b)        # react capture: world


def actor(r):
    return (r.get('body') or {}).get('actor') or r.get('loc')


def fl(r):
    return (r.get('body') or {}).get('fl') or r.get('fl') or [0, 0, 0]


def windows(rows):
    seen, out = False, []
    for i, r in enumerate(rows):
        if r.get('montage'):
            seen = True
        elif seen:
            out.append(i)
            seen = False
    return out


def analyse(rows, i0):
    a0 = actor(rows[i0])
    slide, step, prev, n = [0.0, 0.0], [0.0, 0.0], None, 0
    first = [world(rows[i0], 'foot_l'), world(rows[i0], 'foot_r')]
    last = first
    for r in rows[i0:i0 + WIN]:
        if r.get('montage') or math.hypot(actor(r)[0] - a0[0], actor(r)[1] - a0[1]) > 2.0:
            break
        cur = [world(r, 'foot_l'), world(r, 'foot_r')]
        if None in cur:
            break
        ph = fl(r)[0]
        phases = [ph // 10, ph % 10]
        if prev:
            for k in range(2):
                d = math.hypot(cur[k][0] - prev[k][0], cur[k][1] - prev[k][1])
                (step if phases[k] == 2 else slide)[k] += d
        prev, last, n = cur, cur, n + 1
    end = [math.hypot(last[k][0] - first[k][0], last[k][1] - first[k][1]) for k in range(2)]
    return n, slide, step, end


for base in sys.argv[1:]:
    for seq in sorted(os.listdir(base)):
        p = os.path.join(base, seq, 'frames.jsonl')
        if not os.path.isfile(p):
            continue
        rows = [json.loads(l) for l in open(p)]
        for i0 in windows(rows):
            n, s, st, e = analyse(rows, i0)
            print(f"{os.path.basename(base)}/{seq} f{rows[i0]['f']} n{n} slide L {s[0]:.1f} R {s[1]:.1f}  step L {st[0]:.1f} R {st[1]:.1f}  "
                  f"end L {e[0]:.1f} R {e[1]:.1f}")
