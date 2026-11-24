# fthrow-capture-fix (2026-11-24): exec'd by pie_capture_move.py BEFORE Play (PIE_CAPTURE_PRE_PY). The F. throw capture gets the grab setup of the
# network link tests (link_scenarios): the link types registered and the link steps in the hit rules (prepare_editor + link_setup), the saved link
# test kit KIT_Test_Link on the grabber AND the dummy (applied by the sequence, 'dummy_kit'). KIT_Test_Overlord has no link slots, so Interact
# grabbed nothing and Attack played the jab (v1r2 capture.log, ASSERT FAIL).
# KIT_Test_Link binds its own forward throw (GA_ThrowForward_Test): IN MEMORY ONLY the kit's link extension Throws.Forward becomes the Overlord
# F. throw, which the loadout binds in place of the kit's row (UGSTN_LinkSlots::ApplyKitThrows). Back/Up/Down null = the fallback set plays.
# The capture editor quits without saving, so the kit on disk is untouched. Never run this in an editor the owner is working in.
import unreal
import link_scenarios as ls

FTHROW = '/Game/GASNPPTest/Combat/Overlord/GA_Overlord_FThrowV1.GA_Overlord_FThrowV1_C'
KIT = '/Game/GASNPPTest/Combat/' + ls.CONTENT_KIT

_msgs = [f'prepare_editor {ls.prepare_editor()}', f'link_setup {ls.link_setup()}']
_kit = unreal.load_asset(KIT)
_cls = unreal.load_class(None, FTHROW)
if _kit is None or _cls is None:
    raise RuntimeError(f'missing asset: kit {_kit}, throw class {_cls}')
_ok = ls.Lib.set_kit_throws(_kit, _cls, None, None, None)
_msgs.append(f'{ls.CONTENT_KIT} Throws.Forward = GA_Overlord_FThrowV1 (in memory): {_ok}; throws now {ls.Lib.get_kit_throws(_kit)}')
_pre_msg = 'fthrow pre: ' + ' | '.join(_msgs)
