# HITBOX-EDITOR H1 (2026-11-15): keeps the kit setup scripts from overwriting assets edited by hand (hitboxes placed in the timeline editor,
# animations retouched in Unreal, montage tweaks).
#
# How it knows. Every asset a setup script writes is recorded in a LEDGER (Tools/GAS/setup_guard_ledger.json, versioned with the content) as
# the sha256 of its .uasset right after the script saved it. The sha256 of a file is also its Git LFS oid, so a fresh checkout matches the
# ledger byte for byte. On the next run, before touching an existing asset, the script compares:
#   new       no .uasset yet                          -> write (and record)
#   owned     file sha256 == ledger, package not dirty -> write (and record): nobody touched it since the script made it
#   edited    file sha256 != ledger                    -> KEEP: saved by someone else (the editor, a hand edit, another branch's build)
#   unsaved   the package is dirty in the open editor  -> KEEP: hand edits not saved yet
#   unknown   file exists, no ledger entry             -> KEEP: made before the ledger existed, or by hand
# KEEP means the script leaves that asset alone (it may only re-link a montage that a refresh left empty: see the kit scripts) and says so
# in its output. force=True (or a list of asset names, or env SETUP_FORCE=1 / SETUP_FORCE=TL_Overlord_Jab1,AS_...) overwrites anyway, after
# copying the old .uasset to Saved/Backups/SetupGuard/<time>/.
#
# Rule for threads: commit the ledger together with the .uasset files a setup script wrote; a ledger that lags only makes the next run KEEP
# (safe), never overwrite.
#
# Pure Python except Guard.for_unreal(): the decision, the ledger and the CLI are unit tested without the editor (Tools/GAS/tests).
# CLI (no editor):  python Tools/GAS/setup_guard.py status            every ledger asset: owned / edited / missing
#                   python Tools/GAS/setup_guard.py adopt <asset...>  record the CURRENT files as script-owned (e.g. after merging a branch
#                                                                     whose build changed them; never on files the owner edited)
import datetime
import hashlib
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.dirname(os.path.dirname(HERE))
LEDGER = os.path.join(HERE, 'setup_guard_ledger.json')

NEW, OWNED, EDITED, UNSAVED, UNKNOWN = 'new', 'owned', 'edited', 'unsaved', 'unknown'
WRITABLE = (NEW, OWNED)


def asset_file(project_dir, asset_path):
    """/Game/A/B (or /Game/A/B.B) -> <project>/Content/A/B.uasset"""
    if not asset_path.startswith('/Game/'):
        raise ValueError(f'not a /Game/ path: {asset_path}')
    rel = asset_path[len('/Game/'):].split('.', 1)[0]
    return os.path.join(project_dir, 'Content', *rel.split('/')) + '.uasset'


def file_sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def classify(file_exists, file_hash, ledger_hash, dirty=False):
    """The state of one asset; only NEW and OWNED may be written without force"""
    if not file_exists:
        return UNSAVED if dirty else NEW   # created in this editor session and never saved: someone's work
    if dirty:
        return UNSAVED
    if ledger_hash is None:
        return UNKNOWN
    return OWNED if file_hash == ledger_hash else EDITED


def parse_force(value):
    """True/False, an iterable of asset names, or the env text ('1', 'all', 'TL_A,AS_B') -> True, False or a set of names"""
    if value is None or value is False:
        return False
    if value is True:
        return True
    if isinstance(value, str):
        v = value.strip()
        if v.lower() in ('', '0', 'false', 'no'):
            return False
        if v.lower() in ('1', 'true', 'yes', 'all'):
            return True
        return {n.strip() for n in v.split(',') if n.strip()}
    return {str(n) for n in value}


def load_ledger(path=LEDGER):
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        return json.load(f).get('assets', {})


def save_ledger(entries, path=LEDGER):
    with open(path, 'w', newline='\n') as f:
        json.dump({'version': 1, 'assets': entries}, f, indent=1, sort_keys=True)
        f.write('\n')


class Guard:
    """One per script run. may_write() before any change to an existing asset, record() after saving what was written"""

    def __init__(self, script, force=False, log=print, project_dir=PROJECT_DIR, ledger_path=LEDGER, is_dirty=None, now=None):
        self.script = script
        self.force = parse_force(force)
        self.log = log
        self.project_dir = project_dir
        self.ledger_path = ledger_path
        self.is_dirty = is_dirty or (lambda asset_path: False)
        self.now = now or (lambda: datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
        self.entries = load_ledger(ledger_path)
        self.states = {}      # asset path -> state at first check
        self.kept = []        # asset paths left alone
        self.forced = []      # asset paths overwritten by force (backed up first)
        self.touched = []     # kept asset paths this run changed anyway (a re-linked montage): saved, never recorded
        self._backup_dir = None

    @staticmethod
    def name(asset_path):
        return asset_path.rsplit('/', 1)[-1].split('.', 1)[0]

    def state(self, asset_path):
        if asset_path not in self.states:
            path = asset_file(self.project_dir, asset_path)
            exists = os.path.exists(path)
            entry = self.entries.get(asset_path)
            self.states[asset_path] = classify(exists, file_sha256(path) if exists else None, entry and entry.get('sha256'),
                                               bool(self.is_dirty(asset_path)))
        return self.states[asset_path]

    def precheck(self, asset_paths):
        """Classify every asset the script may touch BEFORE it changes anything: its own edits would mark packages dirty"""
        for p in asset_paths:
            self.state(p)

    def _forced(self, asset_path):
        return self.force is True or (isinstance(self.force, set) and self.name(asset_path) in self.force)

    def may_write(self, asset_path):
        """True when the script may change (or delete and remake) the asset. Logs once per asset when it may not"""
        s = self.state(asset_path)
        if s in WRITABLE:
            return True
        if self._forced(asset_path):
            if asset_path not in self.forced:
                self._backup(asset_path)
                self.forced.append(asset_path)
                self.log(f'FORCE: overwriting {s} {self.name(asset_path)} (old file in {self._backup_dir or "no file"})')
            return True
        if asset_path not in self.kept:
            self.kept.append(asset_path)
            self.log(f'KEPT: {self.name(asset_path)} is {s} ({_WHY[s]}); not overwritten. force=["{self.name(asset_path)}"] to replace it')
        return False

    def _backup(self, asset_path):
        src = asset_file(self.project_dir, asset_path)
        if not os.path.exists(src):
            return
        if not self._backup_dir:
            self._backup_dir = os.path.join(self.project_dir, 'Saved', 'Backups', 'SetupGuard', self.now())
        os.makedirs(self._backup_dir, exist_ok=True)
        shutil.copy2(src, os.path.join(self._backup_dir, os.path.basename(src)))

    def record(self, asset_paths):
        """After saving: the files as they are now become script-owned. Only pass assets this run was allowed to write"""
        for p in asset_paths:
            if not (self.state(p) in WRITABLE or p in self.forced):
                raise RuntimeError(f'record() on {p}, which this run kept')
            path = asset_file(self.project_dir, p)
            if os.path.exists(path):
                self.entries[p] = {'sha256': file_sha256(path), 'script': self.script}
        save_ledger(self.entries, self.ledger_path)

    def relink_montage(self, asset_path, timeline, montage):
        """The one change allowed on a KEPT timeline: a refresh remade the montage it played and left its Montage field empty. Anything
        else (another montage chosen by hand, a version bump) is only reported. Never on a timeline with unsaved changes"""
        data = timeline.get_editor_property('data')
        current = data.get_editor_property('montage')
        if current is None and montage is not None and self.state(asset_path) != UNSAVED:
            data.set_editor_property('montage', montage)
            timeline.set_editor_property('data', data)
            self.touched.append(asset_path)
            self.log(f'MERGE: {self.name(asset_path)} kept, only its empty Montage re-linked to {montage.get_name()}')
            return True
        if current is not None and montage is not None and current.get_name() != montage.get_name():
            self.log(f'WARNING: {self.name(asset_path)} (kept) plays {current.get_name()}, the script built {montage.get_name()}: '
                     f'set it in the timeline editor or force=["{self.name(asset_path)}"]')
        return False

    def save_and_record(self, assets, save):
        """save(asset, only_if_dirty) for each asset; an asset kept by this run is saved only if relink_montage() changed it, and only the
        written ones are recorded. Assets the guard never checked (Blueprints, weapon, kit) are saved as before"""
        written = []
        for a in assets:
            p = a.get_path_name().split('.', 1)[0]
            if p not in self.states:
                save(a, False)
            elif self.state(p) in WRITABLE or p in self.forced:
                save(a, False)
                written.append(p)
            elif p in self.touched:
                save(a, True)
        self.record(written)
        return written

    def summary(self):
        if not self.kept and not self.forced:
            return 'guard: nothing edited by hand was found'
        return (f'guard: kept {len(self.kept)} ({", ".join(self.name(p) for p in self.kept) or "-"}), '
                f'forced {len(self.forced)} ({", ".join(self.name(p) for p in self.forced) or "-"})')

    @classmethod
    def for_unreal(cls, script, force=False, log=print, env='SETUP_FORCE'):
        """Inside the editor: project dir from unreal.Paths, dirty packages from the editor, force also from the environment"""
        import unreal
        project = os.path.abspath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
        dirty = {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
        env_force = parse_force(os.environ.get(env))
        if env_force is True or force is True:
            force = True
        elif env_force or force:
            force = (parse_force(force) or set()) | (env_force or set())
        return cls(script, force=force, log=log, project_dir=project,
                   is_dirty=lambda asset_path: asset_path.split('.', 1)[0] in dirty)


_WHY = {
    EDITED: 'saved after the script made it: hand edit or another build',
    UNSAVED: 'open with unsaved changes in the editor',
    UNKNOWN: 'not in the setup ledger: made by hand or before the ledger',
}


def _status(project_dir=PROJECT_DIR, ledger_path=LEDGER, out=print):
    counts = {}
    for p, e in sorted(load_ledger(ledger_path).items()):
        f = asset_file(project_dir, p)
        s = 'missing' if not os.path.exists(f) else (OWNED if file_sha256(f) == e['sha256'] else EDITED)
        counts[s] = counts.get(s, 0) + 1
        if s != OWNED:
            out(f'{s:8} {p}')
    out('ledger: ' + ', '.join(f'{v} {k}' for k, v in sorted(counts.items())) if counts else 'ledger: empty')


def _adopt(paths, project_dir=PROJECT_DIR, ledger_path=LEDGER, out=print):
    entries = load_ledger(ledger_path)
    for p in paths:
        f = asset_file(project_dir, p)
        if not os.path.exists(f):
            raise SystemExit(f'no file for {p}: {f}')
        entries[p] = {'sha256': file_sha256(f), 'script': 'adopt'}
    save_ledger(entries, ledger_path)
    out(f'adopted {len(paths)} assets')


if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == 'status':
        _status()
    elif len(sys.argv) >= 3 and sys.argv[1] == 'adopt':
        _adopt(sys.argv[2:])
    else:
        print('usage: setup_guard.py status | adopt /Game/<path> ...')
