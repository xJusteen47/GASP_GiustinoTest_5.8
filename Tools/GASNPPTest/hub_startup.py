# Project Python startup script (Config/DefaultEngine.ini, PythonScriptPluginSettings.StartupScripts): turns on the test hub's automation, so a
# designer only opens L_Test_Hub and presses Play (test_hub.auto_start). Never fails the editor start: a problem is logged as a warning.
import os
import sys

import unreal

try:
    here = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), 'Tools', 'GASNPPTest')
    if here not in sys.path:
        sys.path.insert(0, here)
    import test_hub
    unreal.log(f'[hub_startup] {test_hub.auto_start()}')
except Exception as e:   # noqa: BLE001
    unreal.log_warning(f'[hub_startup] test hub automation off: {e!r}')
