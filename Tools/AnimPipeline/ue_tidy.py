import unreal
P = "/Game/AnimPipeline/Pilot/"
deleted = [n for n in ("AS_Test_A", "AS_Test_B") if unreal.EditorAssetLibrary.does_asset_exist(P + n) and unreal.EditorAssetLibrary.delete_asset(P + n)]
saved = [n for n in ("AS_Pilot_JabBlockout", "AS_Pilot_JabV1", "AS_Pilot_JabV2", "AS_Pilot_JabV3", "AS_Pilot_JabV4", "L_PilotCapture")
         if unreal.EditorAssetLibrary.save_asset(P + n, only_if_is_dirty=False)]
result = {"deleted": deleted, "saved": saved}
