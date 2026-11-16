# NIGHT-ROOT: per sequence of a pie_capture_move output, root-capsule horizontal offset with and without a montage, and foot_l world creep
# on frames where the actor does not move. Usage: python rootrows.py <capture dir> > out.txt
import json, math, os, sys

base = sys.argv[1]
for seq in sorted(os.listdir(base)):
    p = os.path.join(base, seq, 'frames.jsonl')
    if not os.path.isfile(p):
        continue
    mont, game, creep = [], [], 0.0
    prev = None
    for line in open(p):
        r = json.loads(line)
        b = r.get('body') or {}
        if 'root' not in b:
            continue
        h = math.hypot(b['root'][0], b['root'][1])
        (mont if r.get('montage') else game).append(h)
        foot = [b['actor'][i] + b['foot_l'][i] for i in range(2)]
        if prev and math.hypot(b['actor'][0] - prev[0][0], b['actor'][1] - prev[0][1]) < 0.05:
            creep += math.hypot(foot[0] - prev[1][0], foot[1] - prev[1][1])
        prev = (b['actor'], foot)
    f = lambda a: f"n={len(a)} max={max(a):.1f} mean={sum(a)/len(a):.1f}" if a else "n=0"
    print(f"{seq}: montage[{f(mont)}] game[{f(game)}] footL creep while actor still {creep:.1f} cm")
