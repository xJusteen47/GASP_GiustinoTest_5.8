"""Capture assert (project rule 10, 2026-11-20): a review starts only if the capture shows the clip it was made for.

Before anyone looks at a video, this checks frames.jsonl (written by the PIE captures, one row per rendered 60 fps frame) and says
whether the EXPECTED clip really drives the pose in the EXPECTED window. Six review days were lost to videos that showed another clip
(audit_setup_agentico.md §2.3): the clip was not routed, a fallback idle played, or a montage never started.

Pure Python, no editor: runs in the cloud, on the desktop, and inside the editor (pie_capture_move.py calls check_sequence()).

    python capture_assert.py <frames.jsonl> <clip> <first>:<last> [--min-weight 0.8] [--coverage 0.9] [--out report.json]
    python capture_assert.py --spec <asserts.json> <capture_dir> [--out assert.txt]

  <clip>         asset name as the capture logs it (AS_GSTN_RunStop_L, AM_OverlordJab1...). Several names with "|" (a mirrored
                 pair, L|R); shell-style wildcards allowed (AS_GSTN_RunStop_*). Case is ignored. Matching entries are SUMMED: the
                 same clip can play twice in a blend stack while it blends into itself.
  <first>:<last> capture frames, inclusive, the "f" of frames.jsonl = the number in the PNG names (f000 = first row). Leave the
                 blend-in out of the window: the window is where the clip must OWN the pose.
  asserts.json   {"<sequence>": [{"clip": "...", "window": [first, last], "min_weight": 0.8, "coverage": 0.9}, ...], ...}
                 checks <capture_dir>/<sequence>/frames.jsonl for every entry (same as the PIE_CAPTURE_ASSERT file of
                 pie_capture_move.py and the "expect" key of its sequences).

Verdict: ASSERT: OK when the clip's weight is >= min_weight (0.8) on at least `coverage` (90 %) of the window's frames, and no frame
of the window is missing. Coverage 90 % tolerates a one-frame dip (a sync-group tick record that skips a frame), never a
different clip owning the pose. ASSERT: FAIL otherwise, with the clip that owned the worst frame. ASSERT: FAIL ... (catena di
cattura) when the rows carry no clip data at all: the capture chain is broken, the review must not start.

Where the weights come from: combat.anim in each row, written by UGSTN_GASNPPTestComponent (C++): the tick records of the anim
graph (Blend Stack, motion matching and sequence players) plus the active montages, "asset weight time", heaviest first, max 4+.
Older rows without montage weights: the row's "montage" field [name, position(, weight)] counts as weight 1.0 only with
--montage-implicit (opt-in, stated in the output) because the montage's blend weight is unknown there.

Prints at most 5 lines; exit 0 OK, 1 FAIL, 2 usage / missing file.
"""
import fnmatch
import json
import os
import sys

MIN_WEIGHT = 0.8
COVERAGE = 0.9


def _parse_entry(s):
    """'AS_Name 0.87 1.234' -> ('AS_Name', 0.87). Names have no spaces in UE; tolerate them anyway (weight = second to last)."""
    parts = str(s).rsplit(' ', 2)
    if len(parts) < 3:
        return None
    try:
        return parts[0], float(parts[1])
    except ValueError:
        return None


def _matcher(clip):
    pats = [p.strip().lower() for p in clip.split('|') if p.strip()]

    def match(name):
        n = name.lower()
        return any(fnmatch.fnmatchcase(n, p) for p in pats)
    return match


def frame_players(row, montage_implicit=False):
    """[(asset, weight), ...] for one frames.jsonl row, or None when the row has no clip data at all."""
    combat = row.get('combat') or {}
    anim = combat.get('anim')
    players = None
    if anim is not None:
        players = [e for e in (_parse_entry(x) for x in anim) if e]
    mont = row.get('montage')
    if isinstance(mont, list) and mont and mont[0] not in (None, 'error'):
        name = str(mont[0])
        weighted = len(mont) >= 3 and isinstance(mont[2], (int, float))
        if players is None and not (weighted or montage_implicit):
            return None   # an old row: a montage plays but neither its weight nor the graph's clips were logged
        players = players if players is not None else []
        if not any(p[0] == name for p in players):
            if weighted:
                players.append((name, float(mont[2])))
            elif montage_implicit:
                players.append((name, 1.0))
    return players


def load_rows(path):
    rows = {}
    with open(path, encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                r = json.loads(line)
            except ValueError:
                continue
            if isinstance(r.get('f'), int):
                rows[r['f']] = r
    return rows


def check_rows(rows, clip, window, min_weight=MIN_WEIGHT, coverage=COVERAGE, montage_implicit=False):
    """rows: {frame: row}. Returns a dict with 'ok' and every number the verdict uses."""
    first, last = int(window[0]), int(window[1])
    match = _matcher(clip)
    res = {'clip': clip, 'window': [first, last], 'min_weight': min_weight, 'coverage_needed': coverage, 'frames': {}}
    if last < first:
        res.update(ok=False, reason=f'window {first}:{last} is empty')
        return res
    missing, nodata, good = [], [], 0
    worst = None
    for f in range(first, last + 1):
        row = rows.get(f)
        if row is None:
            missing.append(f); continue
        players = frame_players(row, montage_implicit)
        if players is None:
            nodata.append(f); continue
        w = sum(wt for n, wt in players if match(n))
        top = max(players, key=lambda p: p[1]) if players else ('<nothing>', 0.0)
        res['frames'][f] = round(w, 3)
        if w >= min_weight - 1e-6:
            good += 1
        if worst is None or w < worst[1]:
            worst = (f, w, top)
    n = last - first + 1
    res['missing_frames'] = missing
    res['no_clip_data_frames'] = nodata
    if nodata and len(nodata) == n - len(missing):
        res.update(ok=False, chain=True, reason='no clip weights in the rows (catena di cattura: build the C++ that logs combat.anim, or --montage-implicit for an old montage capture)')
        return res
    if missing:
        res.update(ok=False, reason=f'{len(missing)} window frames missing from frames.jsonl (first {missing[0]}): capture too short')
        return res
    measured = n - len(nodata)
    cov = good / float(measured) if measured else 0.0
    res['coverage'] = round(cov, 3)
    res['min_in_window'] = round(worst[1], 3) if worst else 0.0
    if worst:
        res['worst_frame'] = worst[0]
        res['worst_owner'] = [worst[2][0], round(worst[2][1], 3)]
    res['ok'] = bool(measured) and cov >= coverage - 1e-9 and not nodata
    if nodata and not res.get('reason'):
        res['reason'] = f'{len(nodata)} frames without clip data'
    return res


def line(res, seq=None):
    tag = f'{seq} ' if seq else ''
    w = res['window']
    if res.get('ok'):
        return (f"ASSERT: OK {tag}{res['clip']} f{w[0]}-f{w[1]}: weight >= {res['min_weight']} on {res['coverage'] * 100:.0f}% "
                f"(min {res['min_in_window']:.2f})")
    if res.get('reason'):
        return f"ASSERT: FAIL {tag}{res['clip']} f{w[0]}-f{w[1]}: {res['reason']}"
    owner = res.get('worst_owner') or ['?', 0]
    return (f"ASSERT: FAIL {tag}{res['clip']} f{w[0]}-f{w[1]}: weight >= {res['min_weight']} on {res.get('coverage', 0) * 100:.0f}% "
            f"(< {res['coverage_needed'] * 100:.0f}%), f{res.get('worst_frame')} weight {res.get('min_in_window', 0):.2f}, "
            f"owned by {owner[0]} {owner[1]:.2f}")


def check_sequence(frames_path, expects, montage_implicit=False):
    """expects: list of {"clip", "window", "min_weight"?, "coverage"?}. Returns [result, ...] (used by pie_capture_move.py)."""
    if not os.path.exists(frames_path):
        return [{'clip': e.get('clip', '?'), 'window': e.get('window', [0, 0]), 'ok': False, 'reason': f'no {frames_path}'}
                for e in expects]
    rows = load_rows(frames_path)
    return [check_rows(rows, e['clip'], e['window'], e.get('min_weight', MIN_WEIGHT), e.get('coverage', COVERAGE),
                       e.get('montage_implicit', montage_implicit)) for e in expects]


def check_spec(spec, capture_dir, montage_implicit=False):
    """spec: {sequence: [expect, ...]}. Returns (ok, [(sequence, result), ...])."""
    out = []
    for seq, expects in spec.items():
        if seq.startswith('_'):
            continue
        if isinstance(expects, dict):
            expects = [expects]
        for r in check_sequence(os.path.join(capture_dir, seq, 'frames.jsonl'), expects, montage_implicit):
            out.append((seq, r))
    return bool(out) and all(r['ok'] for _, r in out), out


def summary_lines(ok, results):
    """First line = the gate ("ASSERT: OK" / "ASSERT: FAIL n/m"), then one line per failing (or, if none, per) check."""
    bad = [(s, r) for s, r in results if not r['ok']]
    head = 'ASSERT: OK' if ok else f'ASSERT: FAIL {len(bad)}/{len(results)}'
    if not results:
        head = 'ASSERT: FAIL no expectations (empty spec)'
    return [head] + [line(r, s) for s, r in (bad or results)]


def _arg(argv, name, default=None, cast=str):
    if name in argv:
        i = argv.index(name)
        v = argv[i + 1]
        del argv[i:i + 2]
        return cast(v)
    return default


def main(argv):
    argv = list(argv)
    out = _arg(argv, '--out')
    minw = _arg(argv, '--min-weight', MIN_WEIGHT, float)
    cov = _arg(argv, '--coverage', COVERAGE, float)
    spec = _arg(argv, '--spec')
    implicit = '--montage-implicit' in argv
    if implicit:
        argv.remove('--montage-implicit')
    if spec:
        if len(argv) != 1 or not os.path.exists(spec):
            print('usage: capture_assert.py --spec <asserts.json> <capture_dir>'); return 2
        ok, results = check_spec(json.load(open(spec, encoding='utf-8')), argv[0], implicit)
        lines = summary_lines(ok, results)
        out = out or os.path.join(argv[0], 'assert.txt')
        with open(out, 'w', encoding='utf-8') as f:
            f.write(lines[0] + '\n' + '\n'.join(line(r, s) for s, r in results) + '\n')
        for s in lines[:4]:
            print(s)
        print(f'report: {out}')
        return 0 if ok else 1
    if len(argv) != 3 or ':' not in argv[2]:
        print('usage: capture_assert.py <frames.jsonl> <clip> <first>:<last> | --spec <asserts.json> <capture_dir>'); return 2
    frames, clip, win = argv
    if not os.path.exists(frames):
        print(f'ASSERT: FAIL no {frames}'); return 2
    a, b = win.split(':')
    res = check_rows(load_rows(frames), clip, (int(a), int(b)), minw, cov, implicit)
    out = out or os.path.join(os.path.dirname(os.path.abspath(frames)), 'assert.json')
    json.dump(res, open(out, 'w', encoding='utf-8'), indent=1)
    print(line(res))
    if implicit:
        print('note: montages without a logged weight counted as 1.0 (--montage-implicit)')
    print(f'report: {out}')
    return 0 if res['ok'] else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
