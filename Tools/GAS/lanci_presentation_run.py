# LANCI-1A2A (2026-11-24): headless entry for setup_presentation_table.run() (that module only defines run()). Saves DA_GSTN_Presentation_Default.
# UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<proj>/Tools/GAS/lanci_presentation_run.py" -unattended -nullrhi
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import setup_presentation_table as t  # noqa: E402

# Headless the plugin content is not scanned yet: does_asset_exist says no and run() would try to create the table over the existing one
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([t.ROOT], True)
unreal.log(f'[lanci_presentation] {t.run()}')
