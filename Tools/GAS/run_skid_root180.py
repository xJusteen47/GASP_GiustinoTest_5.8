# One editor run: build the 180-exact skid clip (root_fix_skid_180.py), then point the skid layer at it (setup_skid_anim_layer.py).
# Run with Tools/CI/Run-Timed.ps1 -Preset Capture -Script Tools/GAS/run_skid_root180.py
import os
import runpy

HERE = os.path.dirname(os.path.abspath(__file__))
runpy.run_path(os.path.join(HERE, 'root_fix_skid_180.py'), run_name='rootfix')
os.environ['SKID_CLIP'] = '/Game/GSTN/Animation/Ported57/SkidOwner/SS_180_Turn_Root180'
runpy.run_path(os.path.join(HERE, 'setup_skid_anim_layer.py'), run_name='__main__')
# quit, or Run-Timed kills the editor at the limit and the next start blocks on the "restore packages" dialog
import unreal  # noqa: E402
unreal.EditorAssetLibrary.save_directory('/Game/GSTN/Animation/Layers', only_if_is_dirty=True, recursive=False)
unreal.SystemLibrary.quit_editor()
