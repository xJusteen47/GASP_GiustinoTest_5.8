"""fix_clips_ue.py's conservation guard wiring, with a fake `unreal` module (no editor): a clip whose filter fix would flatten a
held toe bend is refused (nothing written, not saved); a clip with a one-frame glitch is fixed, written and saved.
The fake implements only what the "filters" step and main() touch. Run: python tests/test_fix_guard_mock.py (or pytest)"""
import json, math, os, shutil, sys, tempfile, types

HERE = os.path.dirname(os.path.abspath(__file__))
CC = os.path.join(os.path.dirname(HERE), 'clipcheck')
N, FPS = 31, 30.0


class Quat:
    def __init__(self, x=0.0, y=0.0, z=0.0, w=1.0):
        self.x, self.y, self.z, self.w = x, y, z, w


class Vec:
    def __init__(self, x=0.0, y=0.0, z=0.0):
        self.x, self.y, self.z = x, y, z


class TF:
    def __init__(self, q, t=None):
        self.rotation, self.translation, self.scale3d = q, t or Vec(), Vec(1, 1, 1)


def qz(deg):
    h = math.radians(deg) / 2
    return (0.0, 0.0, math.sin(h), math.cos(h))


class Ctrl:
    def __init__(self, anim):
        self.anim, self.writes = anim, []

    def open_bracket(self, *a):
        pass

    def close_bracket(self, *a):
        pass

    def set_bone_track_keys(self, bone, ts, qs, ss, _):
        self.writes.append(bone)
        self.anim.tracks[bone] = [(q.x, q.y, q.z, q.w) for q in qs]


class Anim:
    def __init__(self, name, tracks):
        self.name, self.tracks, self.saved = name, tracks, False
        self.ctrl = Ctrl(self)

    def get_editor_property(self, k):
        return {'number_of_sampled_keys': N, 'controller': self.ctrl}[k]

    def get_play_length(self):
        return (N - 1) / FPS

    def get_path_name(self):
        return f'/Game/T/{self.name}.{self.name}'

    def get_name(self):
        return self.name


def fake_unreal(anims):
    u = types.ModuleType('unreal')

    class AL:
        @staticmethod
        def get_animation_track_names(a):
            return list(a.tracks)

        @staticmethod
        def get_bone_pose_for_frame(a, b, f, _):
            return TF(Quat(*a.tracks[b][f]))

    class EAL:
        @staticmethod
        def save_asset(path, only_if_is_dirty=False):
            anims[path.split('/')[-1]].saved = True
            return True

    class Paths:
        @staticmethod
        def project_saved_dir():
            return u._saved

        @staticmethod
        def convert_relative_path_to_full(p):
            return p

    class Reg:
        def scan_paths_synchronous(self, *a):
            pass

    class ARH:
        @staticmethod
        def get_asset_registry():
            return Reg()

    class ELS:
        reloaded = []

        @staticmethod
        def reload_packages(pk):
            ELS.reloaded += pk

    u.AnimationLibrary, u.EditorAssetLibrary, u.Paths, u.AssetRegistryHelpers = AL, EAL, Paths, ARH
    u.EditorLoadingAndSavingUtils = ELS
    u.AnimPoseExtensions = object
    u.Quat, u.Text = Quat, str
    u.load_asset = lambda p: anims[p.split('.')[-1]]
    u.load_package = lambda p: p
    return u


def test_guard_in_fixer():
    d = tempfile.mkdtemp()
    try:
        for f in ('fix_clips_ue.py', 'clip_filters.py', 'conserve.py'):
            shutil.copy(os.path.join(CC, f), d)
        still = [qz(0.0)] * N
        # toe bend held 6 frames then released in ONE frame (dash r3 shape): ease_snaps flattens it -> guard FAIL
        toe = [qz(0.0)] * 4 + [qz(v) for v in (10, 25, 40, 50, 58, 62)] + [qz(0.0)] * (N - 10)
        # a one-frame 120 deg glitch on a slow swing: deflick removes it -> guard OK
        glitch = [qz(30 * math.sin(2 * math.pi * f / (N - 1))) for f in range(N)]
        glitch[15] = (math.sin(math.radians(60)), 0.0, 0.0, math.cos(math.radians(60)))
        anims = {'AS_Toe': Anim('AS_Toe', {'root': still, 'ball_r': toe, 'foot_r': still}),
                 'AS_Glitch': Anim('AS_Glitch', {'root': still, 'hand_r': glitch, 'lowerarm_r': still})}
        u = fake_unreal(anims)
        u._saved = os.path.join(d, 'Saved')
        json.dump({'clips': {'Toe': {'asset': '/Game/T/AS_Toe.AS_Toe', 'fix': ['filters']},
                             'Glitch': {'asset': '/Game/T/AS_Glitch.AS_Glitch', 'fix': ['filters']}}},
                  open(os.path.join(d, 'clips.json'), 'w'))
        sys.modules['unreal'] = u
        os.environ['CLIPFIX_WRITE'] = '1'
        try:
            src = open(os.path.join(d, 'fix_clips_ue.py')).read()
            exec(compile(src, os.path.join(d, 'fix_clips_ue.py'), 'exec'), {'__file__': os.path.join(d, 'fix_clips_ue.py'), '__name__': 'x'})
        finally:
            os.environ.pop('CLIPFIX_WRITE', None)
            sys.modules.pop('unreal', None)
            for m in ('clip_filters', 'conserve'):
                sys.modules.pop(m, None)
            sys.path[:] = [p for p in sys.path if p != d]
        rep = json.load(open(os.path.join(d, 'Saved', 'ClipCheck', 'fix_report.json')))
        assert rep['Toe']['guard_fail'] == 'filters', rep['Toe']
        assert any('ball_r' in x for x in rep['Toe']['guard']['filters']['fails']), rep['Toe']['guard']
        assert anims['AS_Toe'].ctrl.writes == [] and not anims['AS_Toe'].saved and rep['Toe']['saved'] is False
        assert rep['Glitch'].get('guard_fail') is None and rep['Glitch']['guard']['filters']['ok'], rep['Glitch']
        assert anims['AS_Glitch'].ctrl.writes == ['hand_r'] and anims['AS_Glitch'].saved
    finally:
        shutil.rmtree(d)


if __name__ == '__main__':
    try:
        test_guard_in_fixer(); print('PASS test_guard_in_fixer\nESITO: OK (0 failed)')
    except AssertionError as e:
        print(f'FAIL test_guard_in_fixer: {e}\nESITO: FAIL (1 failed)'); sys.exit(1)
