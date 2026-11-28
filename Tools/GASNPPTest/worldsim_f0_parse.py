# Reads the editor log of a Tools/GASNPPTest/worldsim_f0.py run and prints the F0 table: for every configuration and phase (steady,
# forced), each world sim's report (role, world, frame, acc, ticks, resim ticks, avg tick ms, avg resim tick ms, worst game frame ms)
# and the reconciles logged inside the phase: forced ones, Sync/Aux mismatches of the world sim model (their Received/Local states
# carry "Accumulator") and mismatches of other models. Plain Python: python -I worldsim_f0_parse.py <ue.log> [out.txt]
import re, sys

REPORT = re.compile(r'WorldSim \[(\w+) (\S+?)( parallel)?\] frame (-?\d+) acc ([0-9a-f]+) \| ticks (\d+) \(resim (\d+)\) \| avg tick ([\d.]+) ms, '
                    r'avg resim tick ([\d.]+) ms \| game frames (\d+) \(with resim (\d+)\) \| worst game frame ([\d.]+) ms over (\d+) ticks')


def parse(path):
    lines = open(path, encoding='utf-8', errors='replace').read().splitlines()
    phases, cur, out = [], None, []
    for i, ln in enumerate(lines):
        m = re.search(r'WSF0 BEGIN (\S+) (\w+)', ln)
        if m:
            cur = {'label': m.group(1), 'phase': m.group(2), 'reports': [], 'forced': 0, 'ws_mismatch': 0, 'other_mismatch': 0,
                   'ws_fields': {}, 'offset_resync': 0, 'ws_examples': []}
            phases.append(cur)
            continue
        if cur is None:
            continue
        if 'WSF0 END' in ln:
            # The Report lines of the closing gstn.WorldSim.Report come right before the END marker (same tick)
            cur = None
            continue
        r = REPORT.search(ln)
        if r and 'LogGSTNWorldSimSpawn' in ln:
            tag = re.match(r'\[[^\]]*\]\[\s*(\d+)\]', ln)
            cur['reports'].append(dict(tag=tag.group(1) if tag else '?', role=r.group(1), world=r.group(2), parallel=bool(r.group(3)), frame=int(r.group(4)), acc=r.group(5),
                                       ticks=int(r.group(6)), resim=int(r.group(7)), avg=float(r.group(8)), avg_resim=float(r.group(9)),
                                       frames=int(r.group(10)), frames_resim=int(r.group(11)), worst=float(r.group(12)), worst_ticks=int(r.group(13))))
        elif 'Reconcile activated due to ForceReconcile' in ln:
            cur['forced'] += 1
        elif 'ReconcileFromInputOffset' in ln:
            cur['offset_resync'] += 1
        elif 'Reconcile required due to Sync/Aux mismatch' in ln:
            block = '\n'.join(lines[i:i + 12])
            if 'Accumulator' in block:
                cur['ws_mismatch'] += 1
                if len(cur['ws_examples']) < 2:
                    cur['ws_examples'].append(block)
            else:
                cur['other_mismatch'] += 1
        elif 'ReconcileField:' in ln:
            f = re.search(r'ReconcileField:\s*(.*)$', ln)
            key = f.group(1).strip() if f else '?'
            if key.startswith('Frame:') or key.startswith('Accumulator:'):
                cur['ws_fields'][key] = cur['ws_fields'].get(key, 0) + 1
    return phases


DUMP = re.compile(r'SimQuery \[(\w+) (\S+?)\] frame (\d+) hash ([0-9a-f]+) queries (\d+)')


def parse_dumps(path):
    """{label: {(role, world): {frame: (hash, queries)}}} from the lines between WSF0 DUMP BEGIN/END"""
    # Every PIE world has the map's name, so a world's block is the run of consecutive lines with rising frames: a new block starts when
    # the role changes or the frame does not rise; blocks are numbered in order (#0, #1, ...)
    dumps, cur, prev = {}, None, None
    for ln in open(path, encoding='utf-8', errors='replace'):
        m = re.search(r'WSF0 DUMP BEGIN (\S+)', ln)
        if m:
            cur, prev = dumps.setdefault(m.group(1), {}), None
            continue
        if 'WSF0 DUMP END' in ln:
            cur = None
            continue
        d = DUMP.search(ln) if cur is not None else None
        if d:
            role, frame = d.group(1), int(d.group(3))
            if prev is None or prev[0] != role or frame <= prev[1]:
                key = (role, f'#{len(cur)}')
            prev = (role, frame)
            cur.setdefault(key, {})[frame] = (d.group(4), int(d.group(5)))
    return dumps


def compare_dumps(dumps):
    out = []
    for label, per_world in dumps.items():
        host = next((v for (role, _), v in per_world.items() if role in ('host', 'server')), None)
        out.append(f'== SimQuery hashes after {label}: worlds {[f"{r} {w} ({len(v)} frames, {min(v) if v else -1}..{max(v) if v else -1})" for (r, w), v in per_world.items()]}')
        if host is None:
            out.append('   no host dump')
            continue
        for (role, world), frames in per_world.items():
            if role not in ('client',):
                continue
            common = sorted(set(frames) & set(host))
            bad = [f for f in common if frames[f][0] != host[f][0]]
            line = f'   {world}: {len(common)} common frames, {len(common) - len(bad)} equal, {len(bad)} different'
            if bad:
                f = bad[0]
                line += f'; first different frame {f}: host {host[f][0]} ({host[f][1]} queries) vs client {frames[f][0]} ({frames[f][1]} queries)'
            out.append(line)
    return out


def main():
    phases = parse(sys.argv[1])
    out = []
    for p in phases:
        out.append(f"== {p['label']} {p['phase']}: forced reconciles {p['forced']}, world sim mismatches {p['ws_mismatch']} "
                   f"(fields {p['ws_fields']}), other model mismatches {p['other_mismatch']}, offset resyncs {p['offset_resync']}")
        # The closing gstn.WorldSim.Report logs one line per world sim in one engine frame: take that block. Every PIE world carries the map
        # name, so the lines are told apart by order (numbered as they come: host first, then the clients)
        closing = [r for r in p['reports'] if r['tag'] == p['reports'][-1]['tag']] if p['reports'] else []
        for n, r in enumerate(closing):
            out.append(f"   {r['role']:6} #{n} {r['world']:12} frame {r['frame']:6} acc {r['acc']} ticks {r['ticks']:5} resim {r['resim']:5} "
                       f"avg {r['avg']:.3f} ms resim avg {r['avg_resim']:.3f} ms worst frame {r['worst']:.3f} ms ({r['worst_ticks']} ticks) "
                       f"game frames {r['frames']} with resim {r['frames_resim']}")
        for ex in p['ws_examples']:
            out.append('   example world sim mismatch:\n      ' + ex.replace('\n', '\n      '))
    out += compare_dumps(parse_dumps(sys.argv[1]))
    text = '\n'.join(out) if out else 'no WSF0 phases in the log'
    print(text)
    if len(sys.argv) > 2:
        open(sys.argv[2], 'w', encoding='utf-8').write(text + '\n')


if __name__ == '__main__':
    main()
