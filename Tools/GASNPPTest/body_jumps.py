import json, sys
cap = sys.argv[1]
for seq in ('dair_hit', 'dair_land'):
    rows = [json.loads(l) for l in open(f'{cap}/{seq}/frames.jsonl') if l.strip()]
    prev, out = None, []
    for i, r in enumerate(rows):
        b = r['body']; vis = b['pelvis_z']
        if prev is not None and abs(vis - prev) > 14:
            out.append(f"v{i} pelvis {prev:.1f}->{vis:.1f} hh {b['cap_hh']} act {b['actor_z']:.1f} mesh {b['mesh_rel_z']:.1f} h {r['h']:.1f} mont {r.get('montage')}")
        prev = vis
    print(seq, len(out), 'jumps >14cm'); print('\n'.join(out[:8]))
