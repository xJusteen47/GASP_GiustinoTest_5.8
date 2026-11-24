"""import-veloce-1 (2026-11-24): bridge sample cache, round runner skip/order, timing report, editor session job queue. Offline:
the editor is a stub module, the bridge a counting function. The cache tests are the quality proof in miniature: a served answer
must equal what the editor returned for that clip, and any change of the clip on disk (or unsaved edits) must reach the editor."""
import importlib, json, os, runpy, sys, tempfile, types, unittest

TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, TOOLS)
from animpipe import samplecache as SC  # noqa: E402

BONES = ["pelvis", "hand_l", "hand_r", "head"]


def bone(name, f, salt=0.0):
    return {"name": name, "location": {"x": f + salt, "y": len(name), "z": 1.5}, "rotation": {"pitch": 0, "yaw": f * 2.0, "roll": 0}}


class Editor:
    """Fake bridge: answers sample_pose from the 'clip' (salt = its content), counts calls."""

    def __init__(self):
        self.calls, self.salt = [], 0.0

    def __call__(self, method, params, timeout):
        self.calls.append((method, json.loads(json.dumps(params))))
        if method == "read_anim_sequence":
            return {"numFrames": 16, "salt": self.salt}
        return {"assetPath": params["assetPath"], "samples": [{"frame": f, "bones": [bone(b, f, self.salt) for b in params["boneNames"]]}
                                                                 for f in params["frames"]]}


class SampleCacheTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.root, self.cache = os.path.join(self.tmp, "proj"), os.path.join(self.tmp, "cache")
        self.asset = "/Game/AnimPipeline/Rig/AS_Test"
        self.file = os.path.join(self.root, "Content", "AnimPipeline", "Rig", "AS_Test.uasset")
        os.makedirs(os.path.dirname(self.file))
        open(self.file, "wb").write(b"clip v1")
        self.ed = Editor()
        SC._dirty_seen.clear()
        for k in ("ANIMPIPE_SAMPLE_CACHE_VERIFY", "ANIMPIPE_TIMING"):
            os.environ.pop(k, None)

    def call(self, bones, frames, asset=None, dirty=None):
        return SC.cached_call(self.ed, "sample_pose", {"assetPath": asset or self.asset, "boneNames": bones, "frames": frames,
                                                       "space": "component"}, 60, root=self.root, folder=self.cache, dirty=dirty)

    def test_hit_is_identical_to_editor_answer(self):
        live = self.ed("sample_pose", {"assetPath": self.asset, "boneNames": BONES, "frames": [0, 1, 2]}, 60)
        self.ed.calls.clear()
        a = self.call(BONES, [0, 1, 2])
        b = self.call(BONES, [0, 1, 2])
        self.assertEqual(len(self.ed.calls), 1)
        self.assertEqual(a, b)
        self.assertEqual(b["samples"], live["samples"])
        self.assertEqual(b["assetPath"], self.asset)

    def test_prefetch_serves_other_bone_sets_in_requested_order(self):
        self.call(["hand_l", "head"], [0, 1])            # e.g. legibility, round 1
        self.call(["pelvis"], [0, 1])                     # e.g. build: miss, but learns the union
        self.ed.calls.clear()
        open(self.file, "wb").write(b"clip v2")           # rebuild: new key
        self.call(["pelvis"], [0, 1])                     # build of round 2: one call carries every bone seen
        self.assertEqual(sorted(self.ed.calls[0][1]["boneNames"]), ["hand_l", "head", "pelvis"])
        r = self.call(["head", "hand_l"], [0, 1])         # legibility of round 2: served
        self.assertEqual(len(self.ed.calls), 1)
        self.assertEqual([b["name"] for b in r["samples"][0]["bones"]], ["head", "hand_l"])

    def test_changed_clip_on_disk_asks_the_editor(self):
        self.call(BONES, [0])
        open(self.file, "wb").write(b"clip v2")
        self.ed.salt = 5.0
        r = self.call(BONES, [0])
        self.assertEqual(len(self.ed.calls), 2)
        self.assertEqual(r["samples"][0]["bones"][0]["location"]["x"], 5.0)

    def test_dirty_package_and_plugin_assets_bypass(self):
        self.call(BONES, [0], dirty=lambda a: True)
        self.call(BONES, [0], dirty=lambda a: True)
        self.assertEqual(len(self.ed.calls), 2)
        self.assertFalse(os.path.isdir(self.cache) and any(f.endswith("_component.json") for f in os.listdir(self.cache)))
        self.call(BONES, [0], asset="/GSTNCombat/AS_Plugin")
        self.call(BONES, [0], asset="/GSTNCombat/AS_Plugin")
        self.assertEqual(len(self.ed.calls), 4)

    def test_refused_answers_are_not_cached(self):
        blocked = lambda m, p, t: {"dialogBlocking": True}
        r = SC.cached_call(blocked, "sample_pose", {"assetPath": self.asset, "boneNames": BONES, "frames": [0]}, 60, root=self.root,
                           folder=self.cache)
        self.assertTrue(r["dialogBlocking"])
        self.call(BONES, [0])
        self.assertEqual(len(self.ed.calls), 1)

    def test_verify_mode_logs_zero_difference(self):
        log = os.path.join(self.tmp, "t.jsonl")
        os.environ["ANIMPIPE_TIMING"] = log
        self.call(BONES, [0, 1])
        os.environ["ANIMPIPE_SAMPLE_CACHE_VERIFY"] = "1"
        try:
            self.call(BONES, [0, 1])
        finally:
            os.environ.pop("ANIMPIPE_SAMPLE_CACHE_VERIFY")
            os.environ.pop("ANIMPIPE_TIMING")
        self.assertEqual(len(self.ed.calls), 2)
        v = [json.loads(l) for l in open(log) if '"verify"' in l]
        self.assertEqual(v[0]["max_diff"], 0.0)

    def test_read_anim_sequence_cached_per_clip(self):
        f = lambda: SC.cached_call(self.ed, "read_anim_sequence", {"assetPath": self.asset}, 60, root=self.root, folder=self.cache)
        self.assertEqual(f(), f())
        self.assertEqual(len(self.ed.calls), 1)

    def test_cache_off_without_folder(self):
        os.environ.pop("ANIMPIPE_SAMPLE_CACHE", None)
        SC.cached_call(self.ed, "sample_pose", {"assetPath": self.asset, "boneNames": BONES, "frames": [0]}, 60, root=self.root)
        SC.cached_call(self.ed, "sample_pose", {"assetPath": self.asset, "boneNames": BONES, "frames": [0]}, 60, root=self.root)
        self.assertEqual(len(self.ed.calls), 2)


class RoundRunnerTest(unittest.TestCase):
    def setUp(self):
        import round_runner
        self.rr = importlib.reload(round_runner)
        self.tmp = tempfile.mkdtemp()
        self.rr.ROOT, self.rr.DATA = self.tmp, os.path.join(self.tmp, "Saved", "AnimPipeline")
        self.rr.SESSION = os.path.join(self.tmp, "Saved", "AnimPipeSession")
        self.rr.JOBS = os.path.join(self.rr.SESSION, "jobs")
        os.makedirs(os.path.join(self.tmp, "w"))
        self.inp = os.path.join(self.tmp, "w", "in.txt")
        open(self.inp, "w").write("a")
        self.boots = []
        self.rr.boot_session = lambda *a: self.boots.append(a)

    def round(self, steps):
        p = os.path.join(self.tmp, "round.json")
        json.dump({"name": "t", "cwd": "w", "steps": steps}, open(p, "w"))
        return p

    def ran(self):
        tdir = os.path.join(self.rr.DATA, "timing")
        last = sorted(os.listdir(tdir))[-1]
        recs = [json.loads(l) for l in open(os.path.join(tdir, last))]
        return {r["phase"]: r for r in recs if r.get("phase") not in ("round",)}

    def test_skip_when_inputs_unchanged_and_rerun_when_changed(self):
        step = {"id": "s", "editor": False, "cmd": ["python", "-c", "open('out.txt','w').write('x')"], "inputs": ["in.txt"],
                "outputs": ["out.txt"]}
        p = self.round([step])
        self.assertEqual(self.rr.main([p]), 0)
        import time; time.sleep(1.1)
        self.assertEqual(self.rr.main([p]), 0)
        self.assertTrue(self.ran()["s"].get("skipped"))
        open(self.inp, "w").write("b")
        time.sleep(1.1)
        self.rr.main([p])
        self.assertFalse(self.ran()["s"].get("skipped"))
        self.assertEqual(self.boots, [])                   # offline steps never boot an editor

    def test_ok_pattern_failure_is_not_stamped_and_stops(self):
        steps = [{"id": "a", "editor": False, "cmd": ["python", "-c", "print('author x (exit 1)')"], "inputs": ["in.txt"],
                  "ok_pattern": r"exit 0\)"},
                 {"id": "b", "editor": False, "cmd": ["python", "-c", "print(1)"]}]
        self.assertEqual(self.rr.main([self.round(steps)]), 1)
        self.assertFalse(os.path.exists(os.path.join(self.rr.DATA, "round_runner", "t", "a.stamp")))
        self.assertNotIn("b", self.ran())

    def test_offline_gate_fail_quits_the_booting_editor(self):
        steps = [{"id": "gate", "editor": False, "cmd": ["python", "-c", "import sys; sys.exit(1)"]},
                 {"id": "build", "editor": True, "cmd": ["python", "-c", "print(1)"]}]
        self.assertEqual(self.rr.main([self.round(steps)]), 1)
        self.assertEqual(len(self.boots), 1)               # boot overlapped the offline gate
        self.assertTrue(os.path.exists(os.path.join(self.rr.SESSION, "quit.txt")))


class TimingReportTest(unittest.TestCase):
    def test_report_from_jsonl_and_ue_log(self):
        import import_timing_report as R
        tmp = tempfile.mkdtemp()
        jl = os.path.join(tmp, "k_1.jsonl")
        recs = [{"phase": "round", "event": "start", "name": "k"}, {"phase": "editor_boot", "sec": 41.0, "ok": True},
                {"phase": "build", "sec": 80.0, "exit": 0}, {"phase": "check", "skipped": True},
                {"method": "sample_pose", "sec": 0.5, "cache": "miss"}, {"method": "sample_pose", "sec": 0.01, "cache": "hit"},
                {"method": "sample_pose", "cache": "verify", "max_diff": 0.0}, {"phase": "round", "event": "end", "sec": 130, "exit": 0}]
        open(jl, "w").write("\n".join(json.dumps(r) for r in recs) + "\n")
        log = os.path.join(tmp, "s.log.ue.log")
        open(log, "w").write("\n".join([
            "[2026.11.24-08.00.00:000][  0]LogInit: start",
            "[2026.11.24-08.00.05:000][  0]LogShaderCompilers: Display: Compiling 120 shaders",
            "[2026.11.24-08.00.35:000][  0]LogShaderCompilers: Display: done",
            "[2026.11.24-08.00.36:000][  0]LogInit: Total Editor Startup Time, took=36.5",
            "[2026.11.24-08.00.40:000][  0]LogPython: [animpipe_session] ready",
            "[2026.11.24-08.01.40:000][  0]LogPython: [animpipe_session] job 20261124_1 ok in 12.5 s"]) + "\n")
        self.assertEqual(R.main([jl, "--ue-log=" + log[:-7]]), 0)
        rep = json.load(open(os.path.join(tmp, "k_1_report.json")))
        self.assertEqual(rep["ue"]["startup_s"], 36.5)
        self.assertEqual(rep["ue"]["spans"]["shaders"]["span_s"], 30.0)
        self.assertEqual(rep["ue"]["shader_max_pending"], 120)
        self.assertEqual(rep["ue"]["ready_s"], 40.0)
        self.assertEqual(rep["ue"]["jobs"][0]["sec"], 12.5)
        self.assertEqual(rep["verify_max_diff"], 0.0)


class EditorSessionJobsTest(unittest.TestCase):
    """editor_session.py on a stub `unreal`: sync jobs, boot jobs whose QUIT_EDITOR ends the job (not the editor), env restored."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.cbs, self.quit, self.console = {}, [], []
        tmp, cbs, quit_, console = self.tmp, self.cbs, self.quit, self.console
        u = types.ModuleType("unreal")

        class Paths:
            convert_relative_path_to_full = staticmethod(lambda p: p)
            project_saved_dir = staticmethod(lambda: os.path.join(tmp, "Saved"))
            project_dir = staticmethod(lambda: tmp)

        class SystemLibrary:
            execute_console_command = staticmethod(lambda w, c: console.append(c))
            quit_editor = staticmethod(lambda: quit_.append(1))

        n = [0]

        def reg(fn):
            n[0] += 1; cbs[n[0]] = fn; return n[0]

        u.Paths, u.SystemLibrary, u.log = Paths, SystemLibrary, lambda m: None
        u.register_slate_post_tick_callback = reg
        u.unregister_slate_post_tick_callback = lambda h: cbs.pop(h, None)
        u.get_editor_subsystem = lambda c: (_ for _ in ()).throw(RuntimeError("no PIE in tests"))
        u.LevelEditorSubsystem = object
        self.real = sys.modules.get("unreal")
        sys.modules["unreal"] = u
        runpy.run_path(os.path.join(TOOLS, "editor_session.py"))
        self.jobs = os.path.join(tmp, "Saved", "AnimPipeSession", "jobs")

    def tearDown(self):
        if self.real is None:
            sys.modules.pop("unreal", None)
        else:
            sys.modules["unreal"] = self.real

    def tick(self, n=1):
        for _ in range(n):
            for fn in list(self.cbs.values()):
                fn(0.016)

    def job(self, jid, spec):
        json.dump(spec, open(os.path.join(self.jobs, jid + ".json"), "w"))

    def done(self, jid):
        p = os.path.join(self.jobs, jid + ".done.json")
        return json.load(open(p)) if os.path.exists(p) else None

    def test_sync_and_boot_jobs(self):
        open(os.path.join(self.tmp, "sync.py"), "w").write("import os\ndef go(x):\n    return os.environ['J_ENV'] + str(x)\n")
        open(os.path.join(self.tmp, "boot.py"), "w").write(
            "import unreal\nst = {'n': 0}\n"
            "def _t(dt):\n    st['n'] += 1\n"
            "    if st['n'] == 3:\n        unreal.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')\n"
            "unreal.SystemLibrary.execute_console_command(None, 't.MaxFPS 0')\n"
            "st['h'] = unreal.register_slate_post_tick_callback(_t)\n")
        self.tick(31)
        self.assertTrue(os.path.exists(os.path.join(self.tmp, "Saved", "AnimPipeSession", "ready.txt")))
        os.environ.pop("J_ENV", None)
        self.job("a", {"script": "sync.py", "entry": "go", "kwargs": {"x": 7}, "env": {"J_ENV": "e"}})
        self.tick(1)
        self.assertEqual(self.done("a")["result"], "e7")
        self.assertNotIn("J_ENV", os.environ)                 # env restored for the next job
        self.job("b", {"script": "boot.py", "mode": "boot"})
        self.tick(1)
        self.assertEqual(len(self.cbs), 2)                    # session tick + the job's own
        self.tick(4)
        d = self.done("b")
        self.assertTrue(d and d["ok"], d)
        self.assertEqual(self.quit, [])                       # the editor stays up
        self.assertEqual(self.console, ["t.MaxFPS 0"])        # QUIT_EDITOR swallowed, other commands pass
        self.assertEqual(len(self.cbs), 1)                    # job callbacks unregistered
        open(os.path.join(self.tmp, "Saved", "AnimPipeSession", "quit.txt"), "w").write("q")
        self.tick(1)
        self.assertEqual(self.quit, [1])


if __name__ == "__main__":
    unittest.main()
