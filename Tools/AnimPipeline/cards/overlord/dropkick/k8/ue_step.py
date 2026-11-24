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


def delete(path):
    """Delete one asset of this round's own (e.g. the game copy of the startup before the variant script re-copies it). Never use it
    on a shared clip: DROPKICK_KICK_REFRESH=1 would also delete AS_OverlordDropkick_SettleK6, which the APPROVED K7 montage plays."""
    lib = unreal.EditorAssetLibrary
    return f"deleted {path} {lib.delete_asset(path)}" if lib.does_asset_exist(path) else f"absent {path}"


def copy(src, dst):
    """Duplicate src -> dst only when dst does not exist (round_k8r3.json step 1: the K8R3 source take starts as K7's). Idempotent,
    so a re-run of the round never overwrites a take that author already wrote."""
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(dst):
        return f"exists {dst}"
    if not lib.does_asset_exist(src) or not lib.duplicate_asset(src, dst):
        raise RuntimeError(f"could not duplicate {src} -> {dst}")
    lib.save_asset(dst, False)
    return f"copied {src} -> {dst}"
