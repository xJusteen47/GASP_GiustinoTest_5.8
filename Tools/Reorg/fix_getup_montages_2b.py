"""Phase 2b leftover: the GSTNDefence test get-ups point at the reaction montages by their old path.

reorg_apply.py resaves the referencers the Asset Registry reports; GSTNDefence is off by default, so its plugin content was not
among them and the three GA_Getup*_Test kept /Game/GASNPPTest/Combat/Reaction/AM_React_* after the redirectors were removed
(the hub smoke logged "cannot find object"). This sets their Montage default to the moved asset and saves them.

Run headless: UnrealEditor-Cmd.exe GASP_Giustino2.uproject -run=pythonscript -script="<abs>/Tools/Reorg/fix_getup_montages_2b.py"
"""
import unreal

ROWS = {
    '/GSTNDefence/Test/GA_GetupNeutral_Test': '/Game/GSTN/Combat/Reactions/AM_React_GetUp',
    '/GSTNDefence/Test/GA_GetupRoll_Test': '/Game/GSTN/Combat/Reactions/AM_React_Roll',
    '/GSTNDefence/Test/GA_GetupRollBack_Test': '/Game/GSTN/Combat/Reactions/AM_React_RollBack',
}
for bp_path, montage_path in ROWS.items():
    bp = unreal.load_asset(bp_path)
    montage = unreal.load_asset(montage_path)
    if not bp or not montage:
        print(f'GETUP FIX: FAIL {bp_path} bp={bool(bp)} montage={bool(montage)}')
        continue
    cdo = unreal.get_default_object(bp.generated_class())
    # Only the timeline's montage changes (as Tools/GAS/setup_reaction_anims.py writes it): frames, FAF and travel stay as they are.
    tl = cdo.get_editor_property('timeline')
    tl.set_editor_property('montage', montage)
    cdo.set_editor_property('timeline', tl)
    ok = unreal.EditorAssetLibrary.save_asset(bp_path, only_if_is_dirty=False)
    now = cdo.get_editor_property('timeline').get_editor_property('montage')
    print(f'GETUP FIX: {"OK" if ok else "NON SALVATO"} {bp_path} -> {now.get_path_name() if now else None}')
