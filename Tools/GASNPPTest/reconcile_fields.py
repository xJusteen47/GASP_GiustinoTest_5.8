# Which sync field asked for each reconcile, per scenario, from a batch's UE log run with the cvar np.LogReconcileField 1.
#
#   NETGRAB_CVARS='np.LogReconcileField 1' (net_grab_driver.py) makes NetworkPrediction log "ReconcileField: <first mismatching field>" for every
#   ShouldReconcile that returned true. This script splits the log at the scenario verdict lines ("GASNPP <name>: PASS|FAIL") and counts the
#   fields inside each scenario. Full table in <log>.fields.txt; stdout max 5 lines (project rule 4).
#
#   python Tools/GASNPPTest/reconcile_fields.py Saved/Logs/ci/netgrab-<tag>.log.ue.log [scenario substring]
#
# Written for the neck-hold desync of link_spike_client_grabs_host (net grab test 2026-11-23): the code reading alone found no simulation reader of the
# grabbed's per-link anchor, so the next desktop run names the field instead of guessing it.

import collections
import re
import sys

VERDICT = re.compile(r'GASNPP (\S+): (PASS|FAIL)')
FIELD = re.compile(r'ReconcileField: (.*)$')


def main():
    if len(sys.argv) < 2:
        print('usage: reconcile_fields.py <ue log> [scenario substring]')
        return 2
    path = sys.argv[1]
    want = sys.argv[2] if len(sys.argv) > 2 else ''
    pending = collections.Counter()
    table = []
    for line in open(path, encoding='utf-8', errors='replace'):
        m = FIELD.search(line)
        if m:
            # The field text can carry values: keep the name part so equal fields group
            pending[m.group(1).strip()[:160]] += 1
            continue
        v = VERDICT.search(line)
        if v:
            table.append((v.group(1), v.group(2), pending))
            pending = collections.Counter()
    out_path = path + '.fields.txt'
    with open(out_path, 'w', encoding='utf-8') as out:
        for name, verdict, fields in table:
            out.write(f'{name} {verdict} {sum(fields.values())} reconcile fields\n')
            for field, n in fields.most_common():
                out.write(f'    {n:4d}  {field}\n')
    shown = [(n, v, f) for n, v, f in table if want in n]
    for name, verdict, fields in shown[:4]:
        top = '; '.join(f'{k} x{c}' for k, c in fields.most_common(2)) or 'none logged (is np.LogReconcileField 1 set?)'
        print(f'{name} {verdict}: {top}'[:300])
    print(f'{len(table)} scenarios, full table: {out_path}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
