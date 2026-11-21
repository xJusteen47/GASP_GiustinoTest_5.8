# crouch-mesh-pop-1: reads crouch_pop_run.py's rows.csv and prints (max 5 lines) the numbers that prove or disprove a one-frame mesh jump,
# per spam phase (spam_fix = gstn.Mesh.CapsuleShiftBeforeAnim 1, spam_old = 0). Standing still on a flat floor, through every crouch / stand:
#   mesh_step  max per-frame change of the mesh component height (cm): the mesh must not move at all -> <= 0.5
#   root_step  max per-frame change of the root bone height above the capsule bottom (cm): the feet stay on the floor -> <= 1.0
#   spikes     frames where pelvis or head jump > 3 cm and come back > 3 cm on the next frame (a one-frame flash) -> 0
#   crouches   capsule half height changes seen (the run really crouched; 0 = invalid run)
# PASS = spam_fix meets all three limits with crouches >= 6. spam_old is the first version, for the A/B (expected to show the pop).
# usage: python crouch_pop_check.py <rows.csv>   (also writes check.txt next to it)
import csv, os, sys

LIM_MESH, LIM_ROOT, LIM_SPIKE = 0.5, 1.0, 3.0


def stats(rows):
    mesh_step = root_step = 0.0
    spikes = crouches = 0
    prev = None
    deltas = []   # (pelvis, head) per-frame change
    for r in rows:
        floor = r['actor_z'] - r['half_height']
        cur = {'mesh': r['mesh_z'], 'root': r['root_z'] - floor, 'pelvis': r['pelvis_z'], 'head': r['head_z'], 'hh': r['half_height']}
        if prev:
            mesh_step = max(mesh_step, abs(cur['mesh'] - prev['mesh']))
            root_step = max(root_step, abs(cur['root'] - prev['root']))
            if abs(cur['hh'] - prev['hh']) > 1.0:
                crouches += 1
            deltas.append((cur['pelvis'] - prev['pelvis'], cur['head'] - prev['head']))
        prev = cur
    for a, b in zip(deltas, deltas[1:]):
        for k in (0, 1):
            if abs(a[k]) > LIM_SPIKE and abs(b[k]) > LIM_SPIKE and a[k] * b[k] < 0:
                spikes += 1
                break
    return mesh_step, root_step, spikes, crouches


def main(path):
    phases = {}
    with open(path) as f:
        for r in csv.DictReader(f):
            row = {k: (float(v) if k != 'phase' else v) for k, v in r.items()}
            phases.setdefault(r['phase'], []).append(row)
    out = []
    verdict = 'FAIL'
    for ph in ('spam_fix', 'spam_old'):
        rows = phases.get(ph, [])
        if len(rows) < 10:
            out.append(f'{ph}: no rows')
            continue
        m, r, s, c = stats(rows)
        fps = len(rows) / max(1e-6, sum(x['dt'] for x in rows))
        out.append(f'{ph}: mesh_step {m:.2f} cm, root_step {r:.2f} cm, spikes {s}, crouches {c}, {fps:.0f} fps')
        if ph == 'spam_fix' and c >= 6 and m <= LIM_MESH and r <= LIM_ROOT and s == 0:
            verdict = 'PASS'
        elif ph == 'spam_fix' and c < 6:
            verdict = 'INVALID (no crouch)'
    out.append(f'ESITO: {verdict} (limits mesh_step <= {LIM_MESH}, root_step <= {LIM_ROOT}, spikes 0 on spam_fix)')
    text = '\n'.join(out)
    print(text)
    with open(os.path.join(os.path.dirname(os.path.abspath(path)), 'check.txt'), 'w') as f:
        f.write(text + '\n')


if __name__ == '__main__':
    main(sys.argv[1])
