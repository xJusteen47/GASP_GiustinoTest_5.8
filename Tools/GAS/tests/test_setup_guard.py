"""Tools/GAS/setup_guard.py without the editor: the five asset states, force (all, by name, from the env text), the backup, the ledger,
the one merge allowed on a kept timeline (re-link an emptied montage) and which assets save_and_record saves and records.
Run: python Tools/GAS/tests/test_setup_guard.py (or python -m pytest Tools/GAS/tests). Writes only to temp folders."""
import json
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import setup_guard as G  # noqa: E402

TL = '/Game/GSTN/Characters/Overlord/Moves/Jab/TL_Overlord_Jab1'
AS = '/Game/GSTN/Characters/Overlord/Moves/Jab/AS_OverlordJab1V3'


class Project:
    """A temp project folder with Content/ and a ledger beside it"""

    def __init__(self):
        self.dir = tempfile.mkdtemp()
        self.ledger = os.path.join(self.dir, 'ledger.json')

    def write(self, asset, data):
        path = G.asset_file(self.dir, asset)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            f.write(data)

    def guard(self, force=False, dirty=(), lines=None):
        return G.Guard('test', force=force, log=(lines.append if lines is not None else (lambda m: None)), project_dir=self.dir,
                       ledger_path=self.ledger, is_dirty=lambda p: p in dirty, now=lambda: 'T')

    def close(self):
        shutil.rmtree(self.dir)


def _owned(p, asset, data=b'script v1'):
    """The asset as the script left it: written, saved and recorded"""
    p.write(asset, data)
    g = p.guard()
    g.precheck([asset])
    g.states[asset] = G.NEW   # as if the script had just created it
    g.record([asset])


# --- pure decisions -------------------------------------------------------------------------------------------------------------------

def test_asset_file():
    assert G.asset_file('/p', '/Game/A/B') == os.path.join('/p', 'Content', 'A', 'B.uasset')
    assert G.asset_file('/p', '/Game/A/B.B') == os.path.join('/p', 'Content', 'A', 'B.uasset')
    try:
        G.asset_file('/p', '/Engine/X')
        assert False
    except ValueError:
        pass


def test_classify_table():
    assert G.classify(False, None, None) == G.NEW
    assert G.classify(False, None, 'h', dirty=True) == G.UNSAVED       # made in this session, never saved
    assert G.classify(True, 'h', 'h') == G.OWNED
    assert G.classify(True, 'h', 'h', dirty=True) == G.UNSAVED         # owner editing it right now
    assert G.classify(True, 'h2', 'h') == G.EDITED
    assert G.classify(True, 'h', None) == G.UNKNOWN


def test_parse_force():
    assert G.parse_force(None) is False and G.parse_force(False) is False and G.parse_force('') is False and G.parse_force('0') is False
    assert G.parse_force(True) is True and G.parse_force('1') is True and G.parse_force('all') is True
    assert G.parse_force('TL_A, AS_B') == {'TL_A', 'AS_B'}
    assert G.parse_force(['TL_A']) == {'TL_A'}


# --- the guard on files ---------------------------------------------------------------------------------------------------------------

def test_new_and_owned_are_written():
    p = Project()
    try:
        assert p.guard().may_write(TL)                 # no file yet
        _owned(p, TL)
        g = p.guard()
        assert g.state(TL) == G.OWNED and g.may_write(TL) and not g.kept
    finally:
        p.close()


def test_hand_edit_is_kept_and_logged_once():
    p = Project()
    try:
        _owned(p, TL)
        p.write(TL, b'owner moved the hitbox')         # saved from the timeline editor
        lines = []
        g = p.guard(lines=lines)
        assert g.state(TL) == G.EDITED
        assert not g.may_write(TL) and not g.may_write(TL)
        assert len([l for l in lines if l.startswith('KEPT')]) == 1 and 'TL_Overlord_Jab1' in lines[0]
        assert 'kept 1 (TL_Overlord_Jab1)' in g.summary()
    finally:
        p.close()


def test_unknown_and_unsaved_are_kept():
    p = Project()
    try:
        p.write(TL, b'made before the ledger')
        assert p.guard().state(TL) == G.UNKNOWN and not p.guard().may_write(TL)
        _owned(p, AS)
        assert not p.guard(dirty={AS}).may_write(AS)   # unsaved changes in the open editor
    finally:
        p.close()


def test_precheck_freezes_state_before_the_script_writes():
    p = Project()
    try:
        _owned(p, TL)
        dirty = set()
        g = p.guard(dirty=dirty)
        g.precheck([TL])
        dirty.add(TL)                                  # the script's own change dirties the package...
        assert g.may_write(TL)                         # ...and must not turn it into "unsaved hand edits"
    finally:
        p.close()


def test_force_all_backs_up_then_writes():
    p = Project()
    try:
        _owned(p, TL)
        p.write(TL, b'owner edit')
        lines = []
        g = p.guard(force=True, lines=lines)
        assert g.may_write(TL) and g.forced == [TL]
        backup = os.path.join(p.dir, 'Saved', 'Backups', 'SetupGuard', 'T', 'TL_Overlord_Jab1.uasset')
        assert open(backup, 'rb').read() == b'owner edit'
        assert lines[0].startswith('FORCE')
        p.write(TL, b'script v2')
        g.record([TL])                                 # a forced asset becomes script-owned again
        assert p.guard().state(TL) == G.OWNED
    finally:
        p.close()


def test_force_by_name_only_hits_that_asset():
    p = Project()
    try:
        for a in (TL, AS):
            _owned(p, a)
            p.write(a, b'hand edit')
        g = p.guard(force='TL_Overlord_Jab1')
        assert g.may_write(TL) and not g.may_write(AS)
    finally:
        p.close()


def test_record_refuses_a_kept_asset():
    p = Project()
    try:
        p.write(TL, b'by hand')
        g = p.guard()
        assert not g.may_write(TL)
        try:
            g.record([TL])
            assert False, 'record() must not adopt a hand edit'
        except RuntimeError:
            pass
    finally:
        p.close()


def test_ledger_is_sorted_json_with_lfs_style_hash():
    p = Project()
    try:
        _owned(p, TL, b'abc')
        d = json.load(open(p.ledger))
        assert d['version'] == 1
        # sha256 of the file == its Git LFS oid
        assert d['assets'][TL]['sha256'] == 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
    finally:
        p.close()


# --- merge and save -------------------------------------------------------------------------------------------------------------------

class Obj:
    """Stands in for an unreal object: get/set_editor_property, get_name, get_path_name"""

    def __init__(self, path, **props):
        self.path, self.props = path, props

    def get_editor_property(self, k):
        return self.props.get(k)

    def set_editor_property(self, k, v):
        self.props[k] = v

    def get_name(self):
        return self.path.rsplit('/', 1)[-1]

    def get_path_name(self):
        return f'{self.path}.{self.get_name()}'


def test_relink_only_an_emptied_montage():
    p = Project()
    try:
        p.write(TL, b'by hand')
        lines = []
        g = p.guard(lines=lines)
        assert not g.may_write(TL)
        data = Obj('data', montage=None)
        tl = Obj(TL, data=data)
        new = Obj('/Game/X/AM_OverlordJab1V3')
        assert g.relink_montage(TL, tl, new) and data.props['montage'] is new and g.touched == [TL]
        other = Obj('/Game/X/AM_Hand')
        data.props['montage'] = other                  # owner chose another montage: never replaced, only reported
        assert not g.relink_montage(TL, tl, new) and data.props['montage'] is other
        assert any(l.startswith('WARNING') for l in lines)
    finally:
        p.close()


def test_no_relink_on_unsaved_timeline():
    p = Project()
    try:
        _owned(p, TL)
        g = p.guard(dirty={TL})
        data = Obj('data', montage=None)
        assert not g.relink_montage(TL, Obj(TL, data=data), Obj('/Game/X/AM'))
        assert data.props['montage'] is None
    finally:
        p.close()


def test_save_and_record():
    p = Project()
    try:
        kept, relinked, owned, plain = TL, TL + '_B', AS, '/Game/X/GA_Overlord_Jab1'
        for a in (kept, relinked):
            p.write(a, b'by hand')
        _owned(p, owned)
        g = p.guard()
        g.precheck([kept, relinked, owned])
        assert not g.may_write(kept) and not g.may_write(relinked) and g.may_write(owned)
        g.relink_montage(relinked, Obj(relinked, data=Obj('d', montage=None)), Obj('/Game/X/AM'))
        saved = []
        written = g.save_and_record([Obj(a) for a in (kept, relinked, owned, plain)], lambda a, only_dirty: saved.append((a.path, only_dirty)))
        assert saved == [(relinked, True), (owned, False), (plain, False)]   # the kept one is not saved at all
        assert written == [owned]
        assert set(G.load_ledger(p.ledger)) == {owned}
    finally:
        p.close()


def test_cli_status_and_adopt():
    p = Project()
    try:
        p.write(TL, b'merged from a desktop build')
        out = []
        G._adopt([TL], project_dir=p.dir, ledger_path=p.ledger, out=out.append)
        G._status(project_dir=p.dir, ledger_path=p.ledger, out=out.append)
        assert out[-1] == 'ledger: 1 owned'
        p.write(TL, b'owner edit')
        G._status(project_dir=p.dir, ledger_path=p.ledger, out=out.append)
        assert out[-2].startswith('edited') and out[-1] == 'ledger: 1 edited'
    finally:
        p.close()


if __name__ == '__main__':
    tests = [v for k, v in sorted(globals().items()) if k.startswith('test_')]
    for t in tests:
        t()
    print(f'{len(tests)} passed')
