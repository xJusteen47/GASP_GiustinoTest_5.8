# fthrow-overlord-1 (2026-11-23): run by pie_capture_move.py before Play when PIE_CAPTURE_PRE_PY points here. In memory only (nothing saved):
# the project's common F. throw becomes GA_Overlord_FThrow<V> and the neck hold point matches the pose of the card's frame 0, so the kit
# under test (KIT_Test_Overlord) throws with the new clip without editing any kit. Kits compile their throws when given, after this.
import os

import unreal

V = os.environ.get('FTHROW_VERSION', 'V1')
# card hold_point_component_cm (Poser: X left, Y forward, Z up from the feet) -> grabber root space (X forward, Y right, Z from the capsule centre)
HOLD = [float(v) for v in os.environ.get('FTHROW_HOLD_POINT', '38,-5,46').split(',')]
s = unreal.get_default_object(unreal.GSTN_LinkSettings)
cls = unreal.EditorAssetLibrary.load_blueprint_class(f'/Game/GASNPPTest/Combat/Overlord/GA_Overlord_FThrow{V}')
fb = s.get_editor_property('fallback_throws')
fb.set_editor_property('forward', unreal.SoftClassPath(cls.get_path_name()))
s.set_editor_property('fallback_throws', fb)
hp = s.get_editor_property('hold_pose')
hp.set_editor_property('b_hold_by_neck', True)
hp.set_editor_property('hold_point_cm', unreal.Vector(*HOLD))
s.set_editor_property('hold_pose', hp)
_pre_msg = f'fthrow pre: Forward fallback = {cls.get_path_name()}, hold point {HOLD}'
