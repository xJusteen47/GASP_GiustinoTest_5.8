# Skid SS_180_Turn2 (re-retargeted copy) - FAIL (gioco/dati clip)
Capture 2026-11-18 07:42, KIT_Test_Overlord, mv_skid_base, 3 PIE players. Grid grid_rf_az90.png (frames 48-92), mp4 skid_rf.mp4 (0.25x).
Facing not judgeable: from frame 56 to 84 the body sinks into the floor (frames.jsonl pelvis z -74..-109 cm vs -8 standing). Cause: RTG_RootCorrection re-applied to a clip that was already root-fixed (port log: FIXED pelvis local (78,-25,-3)). Fix: SKID_CLIP=SkidOwner/SS_180_Turn2_Src + setup_skid_anim_layer.py, then recapture.
