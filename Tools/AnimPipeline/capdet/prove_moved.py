"""Record the proof that an approved AnimSequence moved WITHOUT changing (owner choice B, 2026-11-24), so regress.py accepts it.

Moving an asset in Unreal rewrites the package name inside the .uasset, so its git blob always changes and the approved-pose gate
(regress.py) FAILs although the animation is the same. The owner asked for a hash of the animation data alone; it is not exact to
the bit (a moved sequence recompresses: 0.000003 deg on dair v9), so the gate binds the two FILE hashes to a bone-by-bone proof
instead: this tool puts the approved file (from the tag) back at its old path, compares every bone, every frame, the curves and the
notifies with the moved one (Tools/Reorg/compare_moved_anim.py, 0.001 cm / 0.001 deg), removes the restored file, and only on
CONFRONTO: OK writes docs/notes/approved-dumps/moves/<tag>__<file>.json with the approved blob and the moved blob. regress.py
accepts a moved file only when such a proof names exactly the blob at the tag and the blob now: touch the file after the proof
and the gate FAILs again.

Desktop, editor closed (it starts UnrealEditor-Cmd once per moved file):
    python Tools/AnimPipeline/capdet/prove_moved.py --tag approved/dair-v9 [--dry]
Engine: env UE_ENGINE_DIR, default C:\\Program Files\\Epic Games\\UE_5.9\\Engine (same as Run-Timed.ps1).
Prints at most 5 lines; exit 0 when every moved file of the tag has a proof, 1 otherwise.
"""
import datetime
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import regress  # noqa: E402

ROOT = regress.ROOT
PROOFS = os.path.join('docs', 'notes', 'approved-dumps', 'moves')
COMPARE = os.path.join(ROOT, 'Tools', 'Reorg', 'compare_moved_anim.py')
RESULT = os.path.join(ROOT, 'Saved', 'Logs', 'ci', 'compare_moved_anim.txt')


def proof_path(tag, path):
    return os.path.join(ROOT, PROOFS, f"{tag.replace('/', '-')}__{os.path.basename(path)}.json")


def pkg(path):
    """Content/A/B.uasset -> /Game/A/B"""
    return '/Game/' + os.path.splitext(path.replace('\\', '/'))[0][len('Content/'):]


def moved_files(tag):
    """[(now path, now blob, path at tag, blob at tag)] of the row's tracked files that moved since the tag."""
    files = regress.tracked()
    out = []
    for row in [r for r in regress.rows() if r['tag'] == tag]:
        in_git, _ = regress.resolve(row, files)
        for p in in_git:
            if regress.git('rev-parse', f'{tag}:{p}')[1] == 0:
                continue                    # not moved: the normal blob check applies
            if regress._published_sha(p) is not None:
                continue                    # a Published/ copy has its own baseline (publish.json)
            cand = regress._moved_at_tag(tag, p)
            if len(cand) != 1:
                continue                    # ambiguous or new: regress.py reports it, no proof can bless it
            now, _ = regress.git('hash-object', '--', p)
            out.append((p, now, cand[0][0], cand[0][1]))
    return out


def prove(tag, p, now, old_path, old_blob, editor, dry):
    old_abs = os.path.join(ROOT, old_path)
    if os.path.exists(old_abs):
        return f'FAIL {p}: {old_path} exists on disk, will not overwrite it'
    if dry:
        return f'DRY {p} <- {old_path} ({old_blob[:10]})'
    raw = subprocess.run(['git', 'cat-file', 'blob', old_blob], cwd=ROOT, capture_output=True, check=True).stdout
    if raw.startswith(b'version https://git-lfs'):
        raw = subprocess.run(['git', 'lfs', 'smudge', '--', old_path], cwd=ROOT, input=raw, capture_output=True, check=True).stdout
    made = []
    d = os.path.dirname(old_abs)
    while not os.path.isdir(d):
        made.append(d)
        d = os.path.dirname(d)
    os.makedirs(os.path.dirname(old_abs), exist_ok=True)
    try:
        with open(old_abs, 'wb') as fh:
            fh.write(raw)
        if os.path.exists(RESULT):
            os.remove(RESULT)
        env = dict(os.environ, CMP_OLD=pkg(old_path), CMP_NEW=pkg(p))
        uproject = os.path.join(ROOT, 'GASP_Giustino2.uproject')
        subprocess.run([editor, uproject, '-run=pythonscript', f'-script={COMPARE}', '-unattended', '-nosplash', '-nullrhi'],
                       cwd=ROOT, env=env, capture_output=True, timeout=1800)
    finally:
        if os.path.exists(old_abs):
            os.remove(old_abs)
        for m in made:                      # deepest first
            if os.path.isdir(m) and not os.listdir(m):
                os.rmdir(m)
    if not os.path.exists(RESULT):
        return f'FAIL {p}: no result from the editor ({RESULT})'
    with open(RESULT, encoding='utf-8') as fh:
        text = fh.read()
    if 'CONFRONTO: OK' not in text:
        last = text.strip().splitlines()[-1] if text.strip() else '(empty)'
        return f'FAIL {p}: {last}'
    m = re.search(r'max delta ([0-9.]+) cm, ([0-9.]+) deg', text)
    fr = re.search(r'frames (\d+) / (\d+), tracce (\d+)', text)
    proof = {'tag': tag, 'approved_path': old_path, 'approved_blob': old_blob, 'moved_path': p, 'moved_blob': now,
             'max_cm': float(m.group(1)) if m else None, 'max_deg': float(m.group(2)) if m else None,
             'frames': int(fr.group(1)) if fr else None, 'tracks': int(fr.group(3)) if fr else None,
             'tolerance': {'cm': 0.001, 'deg': 0.001}, 'tool': 'Tools/Reorg/compare_moved_anim.py',
             'date': datetime.datetime.now(datetime.timezone.utc).strftime('%Y-%m-%dT%H:%MZ'), 'log': text.strip().splitlines()}
    out = proof_path(tag, p)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', encoding='utf-8') as fh:
        json.dump(proof, fh, indent=1)
    return f'OK {p}: {proof["max_cm"]} cm, {proof["max_deg"]} deg -> {os.path.relpath(out, ROOT)}'


def main(argv):
    if '--tag' not in argv:
        print(__doc__.strip().splitlines()[0])
        print('usage: prove_moved.py --tag approved/<move>-<ver> [--dry]')
        return 2
    tag = argv[argv.index('--tag') + 1]
    eng = os.environ.get('UE_ENGINE_DIR', r'C:\Program Files\Epic Games\UE_5.9\Engine')
    editor = os.path.join(eng, 'Binaries', 'Win64', 'UnrealEditor-Cmd.exe')
    todo = moved_files(tag)
    if not todo:
        print(f'PROVE: nothing moved for {tag}')
        return 0
    res = [prove(tag, *t, editor=editor, dry='--dry' in argv) for t in todo]
    bad = [r for r in res if r.startswith('FAIL')]
    print(f'PROVE: {"FAIL" if bad else "OK"} {len(todo) - len(bad)}/{len(todo)} moved files of {tag}')
    print('\n'.join(res[:4]))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
