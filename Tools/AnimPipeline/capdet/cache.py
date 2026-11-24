"""Hash cache for capture and build (owner's "7 deterministic additions" no. 6, 2026-11-23): if nothing that feeds a capture changed,
do not capture again; if nothing that feeds the build changed, do not build again.

A capdet capture costs up to 15 minutes of desktop and an editor session; re-running it on the same poses, the same curve sheet, the
same spec and the same binaries can only produce the same frames (that is what capdet.py's --verify proves). So the key of a capture is
the hash of its INPUTS, and a run whose key is already stored reuses the stored outputs instead of starting the editor.

    python Tools/AnimPipeline/capdet/cache.py key   <spec.json> [--kind capture|build]     print the key
    python Tools/AnimPipeline/capdet/cache.py check <spec.json> [--kind ...]                exit 0 = HIT (fresh), 1 = MISS (stale)
    python Tools/AnimPipeline/capdet/cache.py save  <spec.json> --out <dir> [--kind ...]    store <dir> under the current key
    python Tools/AnimPipeline/capdet/cache.py reuse <spec.json> --out <dir> [--kind ...]    copy the stored outputs into <dir>
    python Tools/AnimPipeline/capdet/cache.py clear [--kind ...] [--all]                    drop entries

What goes into the key, from the spec's "cache" block (paths relative to the repo root, globs allowed, `**` recurses):
  cache.inputs        the poses, the curve sheet and the data of the move: the .casc, the exported joints json, the curve sheet md,
                      the data asset or montage json. These decide the CAPTURE key.
  cache.build_inputs  what a rebuild would change: by default the C++ that the capture runs (Source/**, Plugins/**/Source/**) and the
                      .uproject. These decide the BUILD key, and they are part of the capture key too: new binaries can move a pose.
Plus, always, the spec itself with the volatile keys dropped (`monitor`, `max_minutes`, `cache` and anything under "_"): seed, cvars,
kit, sequence, expect, frames all change the key, so a different window or a different seed never reuses another run's frames.

A file is hashed by its CONTENT (sha256, 1 MiB blocks), never by mtime: a git checkout rewrites mtimes and would invalidate
everything, and a touch would not change a pose. An input that does not exist is recorded as missing and still part of the key, so a
missing pose can never silently look like a hit. The key is the sha256 of that sorted list, so it is stable across machines.

Store: `Saved/Capdet/_cache/<kind>/<key>/` with `manifest.json` (every input and its hash, the spec digest, when it was stored, the
directory it came from) and `out/` (the stored outputs; the PNG frame folders are NOT copied unless --with-frames, because they are
large and post.py only needs frames.jsonl, fingerprint.json, post.json and the videos to tell the same story).

Prints at most 5 lines. Exit 0 OK / HIT, 1 MISS or nothing stored, 2 usage.
"""
import fnmatch
import glob
import hashlib
import json
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))
CACHE = os.path.join('Saved', 'Capdet', '_cache')
BUILD_INPUTS = ['Source/**/*.h', 'Source/**/*.cpp', 'Source/**/*.cs', 'Plugins/*/Source/**/*.h', 'Plugins/*/Source/**/*.cpp',
                'Plugins/*/Source/**/*.cs', 'GASP_Giustino2.uproject']
VOLATILE = {'monitor', 'max_minutes', 'cache', 'name'}       # none of these can change a single rendered pixel
# frame folders are the bulk of a capture and are never needed to re-read a verdict; everything else is small
SKIP_DIRS = {'behind', 'side', 'q34', 'close'}


def _expand(pats):
    """Sorted, de-duplicated repo-relative paths. A pattern that matches nothing yields the pattern itself (recorded as missing)."""
    out = set()
    for p in pats or []:
        p = p.replace('\\', '/')
        hits = [h for h in glob.glob(os.path.join(ROOT, p), recursive=True) if os.path.isfile(h)]
        if hits:
            out.update(os.path.relpath(h, ROOT).replace('\\', '/') for h in hits)
        else:
            out.add(p)
    return sorted(out)


def file_hash(path):
    """sha256 of the content, or 'missing'. Content, not mtime: a checkout rewrites mtimes but changes nothing."""
    full = path if os.path.isabs(path) else os.path.join(ROOT, path)
    if not os.path.isfile(full):
        return 'missing'
    h = hashlib.sha256()
    with open(full, 'rb') as fh:
        for b in iter(lambda: fh.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def spec_digest(spec):
    """The spec without the keys that cannot change a frame, canonically serialised."""
    clean = {k: v for k, v in (spec or {}).items() if k not in VOLATILE and not k.startswith('_')}
    return hashlib.sha256(json.dumps(clean, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def manifest(spec, kind='capture'):
    """Everything the key is made of: the file list with hashes, the spec digest, the kind."""
    c = (spec or {}).get('cache') or {}
    # an explicit [] means "no build inputs" (a capture that cannot depend on the C++); only an ABSENT key takes the default
    build = _expand(c['build_inputs'] if 'build_inputs' in c else BUILD_INPUTS)
    files = build if kind == 'build' else _expand(c.get('inputs')) + build
    m = {'kind': kind, 'spec_digest': spec_digest(spec) if kind != 'build' else None,
         'files': {p: file_hash(p) for p in sorted(set(files))}}
    m['missing'] = sorted(p for p, h in m['files'].items() if h == 'missing')
    m['key'] = hashlib.sha256(json.dumps([m['kind'], m['spec_digest'], sorted(m['files'].items())],
                                         sort_keys=True, separators=(',', ':')).encode()).hexdigest()[:32]
    return m


def entry_dir(key, kind='capture'):
    return os.path.join(ROOT, CACHE, kind, key)


def check(spec, kind='capture'):
    """(hit, manifest, entry_dir). A hit means: the key is stored AND its outputs are there."""
    m = manifest(spec, kind)
    d = entry_dir(m['key'], kind)
    hit = os.path.isfile(os.path.join(d, 'manifest.json')) and os.path.isdir(os.path.join(d, 'out'))
    return hit, m, d


def _copy(src, dst, with_frames=False):
    def ignore(path, names):
        return [n for n in names if not with_frames and n in SKIP_DIRS and os.path.isdir(os.path.join(path, n))]
    shutil.copytree(src, dst, dirs_exist_ok=True, ignore=ignore)


def save(spec, out, kind='capture', with_frames=False):
    """Store `out` under the current key. Replaces an entry with the same key: same inputs, so the newer run is as good."""
    m = manifest(spec, kind)
    d = entry_dir(m['key'], kind)
    if os.path.isdir(os.path.join(d, 'out')):
        shutil.rmtree(os.path.join(d, 'out'), ignore_errors=True)
    os.makedirs(os.path.join(d, 'out'), exist_ok=True)
    if os.path.isdir(out):
        _copy(out, os.path.join(d, 'out'), with_frames)
    m.update(stored=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), source=os.path.relpath(os.path.abspath(out), ROOT).replace('\\', '/'),
             with_frames=bool(with_frames))
    with open(os.path.join(d, 'manifest.json'), 'w', encoding='utf-8') as fh:
        json.dump(m, fh, indent=1)
    return m, d


def reuse(spec, out, kind='capture'):
    """Copy the stored outputs into `out`. (ok, manifest, lines)."""
    hit, m, d = check(spec, kind)
    if not hit:
        return False, m, [f'CACHE: MISS {kind} {m["key"]} (nothing stored)']
    os.makedirs(out, exist_ok=True)
    _copy(os.path.join(d, 'out'), out, with_frames=True)
    with open(os.path.join(d, 'manifest.json'), encoding='utf-8') as fh:
        stored = json.load(fh)
    note = '' if stored.get('with_frames') else ' (frame PNGs not stored: reports and videos only)'
    return True, m, [f'CACHE: HIT {kind} {m["key"]} stored {stored.get("stored")}{note}', f'reused into {out}']


def clear(kind='capture', all_kinds=False):
    n = 0
    roots = [os.path.join(ROOT, CACHE)] if all_kinds else [os.path.join(ROOT, CACHE, kind)]
    for r in roots:
        if os.path.isdir(r):
            n += len([x for x in os.listdir(r) if os.path.isdir(os.path.join(r, x))])
            shutil.rmtree(r, ignore_errors=True)
    return n


def lines(m, hit, kind):
    head = f'CACHE: {"HIT" if hit else "MISS"} {kind} {m["key"]}'
    out = [head, f'{len(m["files"])} input files hashed' + (f', {len(m["missing"])} MISSING' if m['missing'] else '')]
    if m['missing']:
        out.append('missing: ' + ', '.join(m['missing'][:3]))
    return out


def main(argv):
    argv = list(argv)
    kind, out, with_frames, all_kinds = 'capture', None, False, False
    for flag in ('--kind', '--out'):
        if flag in argv:
            i = argv.index(flag); v = argv[i + 1]; del argv[i:i + 2]
            if flag == '--kind': kind = v
            else: out = v
    for flag, name in (('--with-frames', 'with_frames'), ('--all', 'all_kinds')):
        if flag in argv:
            argv.remove(flag)
            if name == 'with_frames': with_frames = True
            else: all_kinds = True
    cmd = argv[0] if argv else ''
    if kind not in ('capture', 'build') or cmd not in ('key', 'check', 'save', 'reuse', 'clear'):
        print('usage: cache.py key|check|save|reuse|clear <spec.json> [--kind capture|build] [--out dir] [--with-frames] [--all]')
        return 2
    if cmd == 'clear':
        print(f'CACHE: cleared {clear(kind, all_kinds)} entries')
        return 0
    if len(argv) != 2 or not os.path.exists(argv[1]):
        print('usage: cache.py key|check|save|reuse <spec.json> [--kind capture|build] [--out dir]')
        return 2
    with open(argv[1], encoding='utf-8') as fh:
        spec = json.load(fh)
    if cmd == 'key':
        print(manifest(spec, kind)['key'])
        return 0
    if cmd == 'check':
        hit, m, _ = check(spec, kind)
        print('\n'.join(lines(m, hit, kind)[:5]))
        return 0 if hit else 1
    if not out:
        print('usage: cache.py save|reuse <spec.json> --out <dir>')
        return 2
    if cmd == 'save':
        m, d = save(spec, out, kind, with_frames)
        print(f'CACHE: SAVED {kind} {m["key"]}\n{d}')
        return 0
    ok, _, ls = reuse(spec, out, kind)
    print('\n'.join(ls[:5]))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
