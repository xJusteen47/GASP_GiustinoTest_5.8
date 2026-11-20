"""Jump squat gate (jumpsquat-v0): reads a PIE capture's frames.jsonl (pie_capture_move.py) and checks
  1. take-off delay: jump.startFrame - the request frame (combat.request[1]) == SQUAT (3 sim frames);
  2. the 3 sim frames before the take-off (historyFrame in [start-3, start-1]): the squat clip's weight (combat.anim, summed) >= 0.8
     on every row, and jump.squat > 0 on them.
    python squat_assert.py <frames.jsonl> <clip name> [--squat 3] [--out assert.txt]
Prints at most 5 lines; exit 0 OK, 1 FAIL."""
import json
import sys


def weight(row, clip):
    w = 0.0
    for e in (row.get('combat') or {}).get('anim') or []:
        parts = str(e).rsplit(' ', 2)
        if len(parts) == 3 and parts[0].lower() == clip.lower():
            w += float(parts[1])
    return w


def main(argv):
    path, clip = argv[0], argv[1]
    squat = int(argv[argv.index('--squat') + 1]) if '--squat' in argv else 3
    out = argv[argv.index('--out') + 1] if '--out' in argv else None
    rows = [json.loads(l) for l in open(path, encoding='utf-8') if l.strip()]
    lines, ok = [], True
    req = start = None
    for r in rows:
        c = r.get('combat') or {}
        rq = c.get('request') or [0, -1]
        if req is None and rq[1] is not None and rq[1] >= 0:
            req = rq[1]
        sf = (c.get('jump') or {}).get('startFrame', -1)
        if req is not None and sf is not None and sf >= req:
            start = sf
            break
    if req is None or start is None:
        lines.append('ASSERT: FAIL no jump request/take-off in the capture')
        ok = False
    else:
        delay = start - req
        ok &= delay == squat
        lines.append(f'delay: request {req} -> take-off {start} = {delay} frames (want {squat}) {"OK" if delay == squat else "FAIL"}')
        seen = {}
        for r in rows:
            c = r.get('combat') or {}
            hf = c.get('historyFrame')
            if hf is not None and start - squat <= hf <= start - 1:
                seen.setdefault(hf, []).append((r['f'], round(weight(r, clip), 2), (c.get('jump') or {}).get('squat')))
        detail = []
        for hf in range(start - squat, start):
            got = seen.get(hf)
            if not got:
                ok = False; detail.append(f'{hf}:missing'); continue
            for f, w, sq in got:
                ok &= w >= 0.8
                detail.append(f'{hf}/f{f}:w{w},sq{sq}')
        lines.append('squat weight: ' + ' '.join(detail))
        lines.insert(0, 'ASSERT: OK' if ok else 'ASSERT: FAIL')
    if out:
        open(out, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
    for ln in lines[:5]:
        print(ln[:300])
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
