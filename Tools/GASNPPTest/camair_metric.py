"""camera-air-1: jitter of the final view per capture folder. python Tools/GASNPPTest/camair_metric.py diag diag_fix (folders under Saved/CamAir)"""
import json, os, sys
for d in sys.argv[1:]:
    for k in ('Overlord_jump', 'Overlord_dair', 'Overlord_fair', 'Playtest_jump', 'Playtest_dair', 'Playtest_fair'):
        p = f'Saved/CamAir/{d}/KIT_Test_{k}.jsonl'
        if not os.path.exists(p):
            continue
        R = [json.loads(l) for l in open(p)]
        dp = [R[i]['pov'][2] - R[i - 1]['pov'][2] for i in range(1, len(R))]
        j = [abs(dp[i] - dp[i - 1]) for i in range(14, min(60, len(dp)))]
        print(f'{d:12s} {k:14s} mean|d2 pov z| {sum(j) / len(j):5.2f} max {max(j):5.1f}')
