"""Run one project script inside the editor with its env (K8 session, 2026-11-23): ue.call('run_python_file', entryPoint='run',
kwargs={'script': <repo-relative .py>, 'env': {...}}). The scripts read os.environ at import time, so the env is set first."""
import os
import runpy

import unreal


def run(script, env=None):
    for k, v in (env or {}).items():
        os.environ[k] = str(v)
    path = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), script)
    runpy.run_path(path, run_name="__main__")
    return "OK " + script
