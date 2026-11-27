"""Regression on the APPROVED poses (owner's "7 deterministic additions" no. 7, 2026-11-23; rule 13b): a version the owner approved is
LOCKED, and no thread may overwrite it. This script is what makes that rule enforceable instead of a sentence in a doc.

Rule 13b says an approved version gets a git tag `approved/<move>-<version>` and a row in `docs/notes/anim-approvate.md`, and work goes
on a VARIANT. Nothing checked it: a re-import, a "small fix" or an auto_fix pass could rewrite the approved asset, and the only way to
notice was the owner saying the video no longer looked right. So, for every row and every tag:

  asset in git      the approved .uasset (Content/, Git LFS) is compared with the SAME path AT THE TAG: `git rev-parse <tag>:<path>` vs
                    the blob of the file now. For an LFS file the blob is the pointer, which carries the content sha256, so the check
                    sees a changed animation even though git never stores it. No lock file needed: the tag IS the baseline.
  asset after tag   an asset committed after its tag (run start v4r5) has no blob at the tag: its git blob is recorded once and
                    deliberately with `--record <tag>` in the same locks file, and must stay exactly that blob.
  file not in git   the .casc of the row lives under `Saved/` (gitignored), so git cannot be the baseline. Its sha256 is recorded, once
                    and deliberately, with `--record <tag>`, in `docs/notes/anim-approvate-locks.json`. A row whose files are not in
                    git and have NO lock entry is a FAIL that says exactly which command records it: unverifiable is not "fine".
  joints            when BOTH `docs/notes/approved-dumps/<slug>.json` (the baseline dump, committed at approval) and the row's current
                    dump exist, every joint is compared frame by frame and the largest deviation must stay under --tol-cm (default
                    0.1 cm = a tenth of a millimetre of rig: re-export noise, not a pose change). Different frame counts are a FAIL.

    python Tools/AnimPipeline/capdet/regress.py [check] [--tol-cm 0.1] [--out report.json] [--tag <tag>] [--allow-missing-tag]
    python Tools/AnimPipeline/capdet/regress.py --record <tag> [--note "why"]      record the non-git files of that row (deliberate)
    python Tools/AnimPipeline/capdet/regress.py --list                             what the table and the tags say, no checking

Rows are read from the markdown table of `docs/notes/anim-approvate.md`:
`| <move> | <version> | <asset name> (<path to the source scene>) | <note> | <tag> |`. The asset name is resolved to the versioned
file by searching the tracked files for `<name>.uasset`; a name that resolves to nothing is reported, and is a FAIL unless the row
also names a file that IS checkable.

Hooked to the build: `Tools/CI/Run-Timed.ps1 -Preset Build` runs this first and refuses to build when it FAILs (`-SkipRegress` to
bypass, which the output states). The intent is that a thread cannot compile, capture and hand over work that silently moved an
approved pose.

Prints at most 5 lines, the first "REGRESS: OK <n> locked" or "REGRESS: FAIL <first reason>"; the detail is in the --out report.
Exit 0 OK, 1 a locked pose changed or cannot be verified, 2 usage / not a git repo.
"""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
TABLE = os.path.join('docs', 'notes', 'anim-approvate.md')
REGISTRY = os.path.join('Tools', 'AnimPipeline', 'registro-clip.csv')
LOCKS = os.path.join('docs', 'notes', 'anim-approvate-locks.json')
DUMPS = os.path.join('docs', 'notes', 'approved-dumps')
TOL_CM = 0.1

sys.path.insert(0, HERE)
import cache  # noqa: E402  (file_hash: same content-only hashing as the capture cache)


def git(*args, ok=(0,)):
    r = subprocess.run(['git'] + list(args), cwd=ROOT, capture_output=True, text=True)
    return (r.stdout.strip() if r.returncode in ok else None), r.returncode


def tracked():
    out, rc = git('ls-files')
    return (out or '').splitlines() if rc == 0 else []


def registry_rows(path):
    """The approved rows of Tools/AnimPipeline/registro-clip.csv (owner 3A, 2026-11-24: the registry is the source of truth and
    anim-approvate.md is a generated view with other columns). Only `approvata` rows carry a tag (rule 13b); a `bloccata` row
    without a tag (K7) is protected by the guard, not by this gate."""
    import csv
    out = []
    with open(path, encoding='utf-8', newline='') as fh:
        for r in csv.DictReader(fh):
            if (r.get('stato') or '').strip() != 'approvata':
                continue
            # "AS_X + AM_X": every asset of the row is kept; resolve() splits them (D1 2026-11-27: the AM was never checked)
            asset = (r.get('asset_ue') or '').strip()
            source = (r.get('carta') or '').split(' (', 1)[0].split(',')[0].strip()
            if source.startswith('cards/'):          # the registry names cards relative to Tools/AnimPipeline
                source = 'Tools/AnimPipeline/' + source
            out.append({'move': r['mossa'], 'version': r['versione'], 'asset': asset, 'source': source,
                        'note': r.get('parole_owner', ''), 'tag': f"approved/{r['mossa']}-{r['versione']}"})
    return out


def rows(table=None):
    """Approved rows: the registry when present, else (old checkouts, tests) the markdown table.
    Returns [{'move', 'version', 'asset', 'source', 'note', 'tag'}]."""
    reg = os.path.join(ROOT, REGISTRY)
    if table is None and os.path.exists(reg):
        return registry_rows(reg)
    p = table or os.path.join(ROOT, TABLE)
    if not os.path.exists(p):
        return []
    out = []
    with open(p, encoding='utf-8') as fh:
        for line in fh:
            line = line.strip()
            if not line.startswith('|'):
                continue
            cells = [c.strip() for c in line.strip('|').split('|')]
            if len(cells) < 5 or set(cells[0]) <= set('-: ') or cells[0].lower() in ('mossa', 'move'):
                continue       # the header or the separator row
            # "AS_Name (Saved/.../Name.casc), rows Variant 20 / 30 (Handoff)": the name is what precedes the FIRST "(",
            # the source is the FIRST parenthesised group; anything after it is a note, not a path.
            asset = cells[2].split('(', 1)[0].strip()
            m = re.search(r'\(([^)]*)\)', cells[2])
            source = (m.group(1) if m else '').split(',')[0].strip()
            out.append({'move': cells[0], 'version': cells[1], 'asset': asset, 'source': source,
                        'note': cells[3], 'tag': cells[4].strip('` ')})
    return out


def resolve(row, files):
    """The files of a row: (in_git, not_in_git). Looks each asset name up among the tracked files; a row may name several assets
    joined by "+" ("AS_OverlordDAirV9 + AM_OverlordDAirV9"), each one is checked."""
    in_git, loose = [], []
    for name in (n.strip() for n in (row['asset'] or '').split('+')):
        if name:
            in_git += sorted(f for f in files if os.path.basename(f) == name + '.uasset' or os.path.basename(f) == name)
    src = row.get('source')
    if src and not src.lower().startswith(('row', 'rig')):
        src = src.replace('\\', '/')
        (in_git if src in files else loose).append(src)
    return sorted(set(in_git)), sorted(set(loose))


def _moved_at_tag(tag, path):
    """[(path at the tag, blob)] of the files at the tag with the same file name as `path` (an asset moved since approval)."""
    out, rc = git('ls-tree', '-r', tag)
    if rc != 0 or not out:
        return []
    base = os.path.basename(path)
    hits = []
    for line in out.splitlines():
        meta, _, p = line.partition('\t')
        if os.path.basename(p) == base:
            hits.append((p, meta.split()[2]))
    return hits


def locks():
    p = os.path.join(ROOT, LOCKS)
    if not os.path.exists(p):
        return {}
    with open(p, encoding='utf-8') as fh:
        return json.load(fh)


def _dump(path):
    with open(path, encoding='utf-8') as fh:
        d = json.load(fh)
    return d.get('joints', d)


def joints_delta(base_path, now_path):
    """Largest per-joint, per-frame deviation in cm, or a reason string. Pure python: no numpy needed for a max of abs diffs."""
    a, b = _dump(base_path), _dump(now_path)
    shared = sorted(set(a) & set(b))
    if not shared:
        return None, 'the two dumps share no joint'
    worst, where = 0.0, None
    for j in shared:
        ja, jb = a[j], b[j]
        if len(ja) != len(jb):
            return None, f'{j}: {len(ja)} frames at approval, {len(jb)} now'
        for f, (pa, pb) in enumerate(zip(ja, jb)):
            d = max(abs(float(x) - float(y)) for x, y in zip(pa, pb))
            if d > worst:
                worst, where = d, f'{j} f{f}'
    return worst, where


MOVE_PROOFS = os.path.join(DUMPS, 'moves')


def _move_proof(tag, path, at_tag, now):
    """Relative path of the proof (prove_moved.py) that `path` at blob `now` is the approved blob `at_tag` moved, else None.
    The proof must name both blobs exactly and stay within its tolerance (0.001 cm / 0.001 deg)."""
    f = os.path.join(ROOT, MOVE_PROOFS, f"{tag.replace('/', '-')}__{os.path.basename(path)}.json")
    if not os.path.exists(f):
        return None
    with open(f, encoding='utf-8') as fh:
        d = json.load(fh)
    tol = d.get('tolerance') or {}
    ok = (d.get('tag') == tag and d.get('approved_blob') == at_tag and d.get('moved_blob') == now
          and d.get('moved_path') == path.replace('\\', '/')
          and d.get('max_cm') is not None and d['max_cm'] <= min(tol.get('cm', 0.001), 0.001)
          and d.get('max_deg') is not None and d['max_deg'] <= min(tol.get('deg', 0.001), 0.001))
    return os.path.relpath(f, ROOT).replace('\\', '/') if ok else None


PUBLISHED = 'Content/AnimPipeline/Published/'


def _published_sha(path):
    """sha256 recorded by publish.json for a file of a Published/<move>/<ver>/ folder, else None (not a published copy)."""
    p = path.replace('\\', '/')
    if not p.startswith(PUBLISHED):
        return None
    meta = os.path.join(ROOT, os.path.dirname(p), 'publish.json')
    if not os.path.exists(meta):
        return None
    with open(meta, encoding='utf-8') as fh:
        return (json.load(fh).get('files') or {}).get(os.path.basename(p))


def check_row(row, files, lk, tol_cm=TOL_CM, allow_missing_tag=False):
    """One row -> {'ok', 'why': [...], and every file it looked at}."""
    res = {'tag': row['tag'], 'move': row['move'], 'version': row['version'], 'checked': {}, 'why': []}
    tag = row['tag']
    if not tag:
        res['why'].append(f'{row["move"]} {row["version"]}: the row names no approved/* tag (rule 13b)')
        res['ok'] = False
        return res
    _, rc = git('rev-parse', '--verify', tag + '^{commit}')
    has_tag = rc == 0
    if not has_tag and not allow_missing_tag:
        res['why'].append(f'{tag}: tag missing here (git fetch origin "refs/tags/approved/*:refs/tags/approved/*")')
    in_git, loose = resolve(row, files)
    if not in_git and not loose:
        res['why'].append(f'{tag}: no file to check (asset "{row["asset"]}" is not among the tracked files and the row names no source)')
    for p in in_git:
        at_tag, rc_t = (git('rev-parse', f'{tag}:{p}') if has_tag else (None, 1))
        now, rc_n = git('hash-object', '--', p)
        if rc_n != 0 or now is None:
            res['why'].append(f'{tag}: {p} is gone from the worktree')
            continue
        if not has_tag:
            res['checked'][p] = {'now': now, 'at_tag': None, 'same': None}
            continue
        if rc_t != 0 or at_tag is None:
            # A copy made by `animpipe publish` (Content/AnimPipeline/Published/<move>/<ver>/) is newer than the tag by design:
            # its baseline is the sha256 that publish.json recorded at publish time (C5, 2026-11-24: the gate refused every build
            # after C4's publish). Same strictness as a lock: a different or missing record is a FAIL.
            pub = _published_sha(p)
            if pub is not None:
                h = cache.file_hash(p)
                res['checked'][p] = {'now': h, 'published': pub, 'same': h == pub}
                if h != pub:
                    res['why'].append(f'{tag}: {p} CHANGED since publish (sha256 {pub[:10]} -> {h[:10]})')
                continue
            # A Content reorganisation moves the approved asset without touching it (phase 2b, 2026-11-24): the baseline is the
            # file with the same name at the tag, and it must be the SAME blob. Several candidates or a different blob stay a FAIL.
            moved = _moved_at_tag(tag, p)
            if len(moved) == 1:
                at_tag = moved[0][1]
                res['checked'][p] = {'now': now, 'at_tag': at_tag, 'moved_from': moved[0][0], 'same': at_tag == now}
                if at_tag != now:
                    # The move itself rewrites the package name inside the .uasset. Accepted only with a recorded bone-by-bone
                    # proof (prove_moved.py, owner choice B 2026-11-24) that names THIS approved blob and THIS moved blob:
                    # any later edit of the file changes its blob and the gate FAILs again.
                    proof = _move_proof(tag, p, at_tag, now)
                    if proof:
                        res['checked'][p]['proof'] = proof
                        res['checked'][p]['same'] = True
                    else:
                        res['why'].append(f'{tag}: {p} (moved from {moved[0][0]}) CHANGED since the tag '
                                          f'(approved blob {at_tag[:10]}, now {now[:10]}); if only moved, prove it on the '
                                          f'desktop: python Tools/AnimPipeline/capdet/prove_moved.py --tag {tag}')
                continue
            # Committed AFTER the tag (run start v4r5: the tag af857cff only holds the registry row, the .uasset arrived 11 minutes
            # later in 2e146cd7): the tag cannot be the baseline, so the git blob is recorded once and deliberately with
            # `--record <tag>`, like a file outside git. Exact blob or FAIL: any later edit of the file changes its blob.
            rec = (lk.get(tag) or {}).get('files', {}).get(p) or {}
            if rec.get('blob'):
                res['checked'][p] = {'now': now, 'locked_blob': rec['blob'], 'same': rec['blob'] == now}
                if rec['blob'] != now:
                    res['why'].append(f'{tag}: {p} CHANGED since its recorded baseline (blob {rec["blob"][:10]} -> {now[:10]})')
                continue
            res['why'].append(f'{tag}: {p} does not exist at the tag (approved under another path, or committed after the tag: '
                              f'record its blob once with regress.py --record {tag})')
            continue
        res['checked'][p] = {'now': now, 'at_tag': at_tag, 'same': at_tag == now}
        if at_tag != now:
            res['why'].append(f'{tag}: {p} CHANGED since the tag (approved blob {at_tag[:10]}, now {now[:10]})')
    for p in loose:
        rec = (lk.get(tag) or {}).get('files', {}).get(p)
        if not rec and in_git:
            # The pose itself is checked through the versioned asset against the tag; a source scene that lives only on
            # one machine (Saved/*.casc) or a doc folder cannot be checked from another checkout. Reported, not failed
            # (docstring: FAIL only when the row names nothing checkable). Record a lock to make it count.
            res.setdefault('unchecked', []).append(p)
            continue
        if not rec:
            res['why'].append(f'{tag}: {p} is not in git and has no lock '
                              f'(record it once: regress.py --record {tag})')
            continue
        h = cache.file_hash(p)
        res['checked'][p] = {'now': h, 'locked': rec.get('sha256'), 'same': h == rec.get('sha256')}
        if h == 'missing':
            res['why'].append(f'{tag}: {p} is missing (locked at approval)')
        elif h != rec.get('sha256'):
            res['why'].append(f'{tag}: {p} CHANGED since the lock (sha256 {rec["sha256"][:10]} -> {h[:10]})')
    slug = tag.replace('/', '-')
    base_dump = os.path.join(ROOT, DUMPS, slug + '.json')
    now_dump = (lk.get(tag) or {}).get('dump')
    now_dump = os.path.join(ROOT, now_dump) if now_dump else None
    if os.path.exists(base_dump) and now_dump and os.path.exists(now_dump):
        worst, where = joints_delta(base_dump, now_dump)
        res['joints'] = {'baseline': os.path.relpath(base_dump, ROOT), 'now': os.path.relpath(now_dump, ROOT),
                         'worst_cm': worst, 'where': where, 'tol_cm': tol_cm}
        if worst is None:
            res['why'].append(f'{tag}: dumps not comparable: {where}')
        elif worst > tol_cm:
            res['why'].append(f'{tag}: joints moved {worst:.3f} cm at {where} (tolerance {tol_cm} cm)')
    res['ok'] = not res['why']
    return res


def check(tol_cm=TOL_CM, only_tag=None, allow_missing_tag=False, table=None):
    """(ok, results, extra_tags). extra_tags = approved/* tags with no row in the table: the doc and the tags must agree."""
    files = tracked()
    lk = locks()
    rs = [r for r in rows(table) if not only_tag or r['tag'] == only_tag]
    results = [check_row(r, files, lk, tol_cm, allow_missing_tag) for r in rs]
    out, rc = git('tag', '-l', 'approved/*')
    tags = set((out or '').splitlines()) - {''}
    extra = sorted(tags - {r['tag'] for r in rows(table)}) if not only_tag else []
    return bool(results) and all(r['ok'] for r in results) and not extra, results, extra


def record(tag, note=None, table=None):
    """Record the sha256 of the row's non-git files. Deliberate and explicit: this is what blesses a baseline."""
    rs = [r for r in rows(table) if r['tag'] == tag]
    if not rs:
        return False, [f'REGRESS: FAIL no row with tag {tag} in {TABLE}']
    files = tracked()
    lk = locks()
    row = rs[0]
    in_git, loose = resolve(row, files)
    # Tracked files the tag does not hold (committed after it) and that no other rule covers (moved, published copy): their
    # git blob becomes the baseline. The blob of an LFS file is its pointer, so it is the same on every machine.
    late = []
    for p in in_git:
        if git('rev-parse', f'{tag}:{p}')[1] == 0 or _published_sha(p) is not None or len(_moved_at_tag(tag, p)) == 1:
            continue
        blob, rc = git('hash-object', '--', p)
        if rc == 0 and blob:
            first, _ = git('log', '--full-history', '--diff-filter=A', '--date-order', '--format=%h %cI', '--', p)
            late.append((p, blob, (first or '').splitlines()[-1:] or ['']))
    if not loose and not late:
        return False, [f'REGRESS: nothing to record for {tag}: every file of the row is in git (the tag is the baseline)']
    entry = dict(lk.get(tag) or {})
    entry.setdefault('files', {})
    for p, blob, first in late:
        entry['files'][p] = {'blob': blob, 'added_in': first[0]}
    missing = []
    for p in loose:
        h = cache.file_hash(p)
        if h == 'missing':
            missing.append(p)
            continue
        entry['files'][p] = {'sha256': h, 'bytes': os.path.getsize(os.path.join(ROOT, p))}
    if note:
        entry['note'] = note
    entry['move'], entry['version'] = row['move'], row['version']
    if missing and not late:
        return False, [f'REGRESS: FAIL {tag}: {len(missing)} files not on this machine, first {missing[0]}',
                       'record on the machine that holds Saved/ (the desktop)']
    lk[tag] = entry
    os.makedirs(os.path.dirname(os.path.join(ROOT, LOCKS)), exist_ok=True)
    with open(os.path.join(ROOT, LOCKS), 'w', encoding='utf-8') as fh:
        json.dump(lk, fh, indent=1, sort_keys=True)
    out = [f'REGRESS: recorded {tag}: {len(entry["files"])} files ({len(late)} committed after the tag)', f'commit {LOCKS}']
    if missing:
        out.append(f'not recorded here (not on this machine, stays unchecked): {", ".join(missing[:2])}')
    return True, out


def lines(ok, results, extra):
    bad = [r for r in results if not r['ok']]
    n_files = sum(len(r['checked']) for r in results)
    if not results:
        return [f'REGRESS: FAIL no approved rows in {TABLE} (rule 13b: an approved version gets a row and a tag)']
    if ok:
        return [f'REGRESS: OK {len(results)} locked versions, {n_files} files unchanged']
    head = f'REGRESS: FAIL {len(bad)}/{len(results)} locked versions'
    why = [w for r in bad for w in r['why']]
    if extra:
        why.append(f'{len(extra)} approved/* tags with no row in {TABLE}: {", ".join(extra[:3])}')
    return [head + (f': {why[0]}' if why else '')] + why[1:4]


def main(argv):
    argv = list(argv)
    tol, out, only, rec, note = TOL_CM, None, None, None, None
    for flag in ('--tol-cm', '--out', '--tag', '--record', '--note'):
        if flag in argv:
            i = argv.index(flag); v = argv[i + 1]; del argv[i:i + 2]
            if flag == '--tol-cm': tol = float(v)
            elif flag == '--out': out = v
            elif flag == '--tag': only = v
            elif flag == '--record': rec = v
            else: note = v
    allow = '--allow-missing-tag' in argv
    if allow:
        argv.remove('--allow-missing-tag')
    listing = '--list' in argv
    if listing:
        argv.remove('--list')
    if argv and argv != ['check']:
        print('usage: regress.py [check] [--tol-cm N] [--out r.json] [--tag t] [--allow-missing-tag] | --record <tag> [--note "..."] | --list')
        return 2
    if git('rev-parse', '--git-dir')[1] != 0:
        print('REGRESS: FAIL not a git repository (the approved/* tags are the baseline)')
        return 2
    if rec:
        ok, ls = record(rec, note)
        print('\n'.join(ls[:5]))
        return 0 if ok else 1
    if listing:
        files = tracked()
        for r in rows():
            ig, lo = resolve(r, files)
            print(f'{r["tag"] or "(no tag)"}  {r["move"]} {r["version"]}  git:{len(ig)} loose:{len(lo)}')
        return 0
    ok, results, extra = check(tol, only, allow)
    rep = {'ok': ok, 'tol_cm': tol, 'extra_tags': extra, 'rows': results}
    out = out or os.path.join(ROOT, 'Saved', 'Capdet', 'regress.json')
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, 'w', encoding='utf-8') as fh:
        json.dump(rep, fh, indent=1)
    ls = lines(ok, results, extra)
    print('\n'.join(ls[:4]))
    print(f'report: {out}')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
