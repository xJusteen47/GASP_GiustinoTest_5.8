import unreal

EAL = unreal.EditorAssetLibrary

FOLDERS = {
    "Jab": [
        "AS_OverlordJab1", "AS_OverlordJab2", "LS_OverlordJab1", "LS_OverlordJab2",
        "LS_OverlordJab_Preview", "AM_OverlordJab",
        "AS_JabV11_Build", "AS_JabV11_RT60", "AS_JabV11_Sparse", "AS_JabV12_Build",
        "LS_JabV11_Build", "LS_JabV11_RT60", "LS_JabV11_Sparse", "LS_JabV12_Build",
        "AM_JabV12_Build"
    ],
    "FTilt": [
        "AS_GanonFTiltV31_Build", "AS_GanonFTiltV31_Build_VarA", "AS_GanonFTiltV31_Build_VarB",
        "AS_GanonFTiltV31_Build_VarC", "AS_GanonFTiltV31_Build_VarD", "AS_TestFTiltVarDArm",
        "LS_GanonFTiltV31_Build", "LS_GanonFTiltV31_Build_VarA", "LS_GanonFTiltV31_Build_VarB",
        "LS_GanonFTiltV31_Build_VarC", "LS_GanonFTiltV31_Build_VarD", "LS_TestFTiltVarDArm",
        "AM_GanonFTiltV31_Build", "AS_GanonAxisProbe_Build", "LS_GanonAxisProbe_Build"
    ],
    "DAir": [
        "AM_OverlordDAirV1_Build", "AM_OverlordDAirV2_Build", "AM_OverlordDAirV3_Build",
        "AM_OverlordDAirV4_Build", "AM_OverlordDAirV4_Build_VarA", "AM_OverlordDAirV4_Build_VarB", "AM_OverlordDAirV4_Build_VarC",
        "AS_OverlordDAirV1_Build", "AS_OverlordDAirV2_Build", "AS_OverlordDAirV3_Build",
        "AS_OverlordDAirV4_Build", "AS_OverlordDAirV4_Build_VarA", "AS_OverlordDAirV4_Build_VarB", "AS_OverlordDAirV4_Build_VarC",
        "AS_OverlordDAirV4_Build_VarD", "AS_OverlordDAirV4_Build_VarF", "AS_OverlordDAirV4_Build_VarG",
        "AS_OverlordDAirV4_Build_VarH", "AS_OverlordDAirV4_Build_VarI", "AS_OverlordDAirV4_Build_VarJ",
        "AS_OverlordDAirV4_Build_VarK", "AS_OverlordDAirV4_Build_VarL", "AS_OverlordDAirV4_Build_VarM",
        "AS_OverlordDAirV5_Build", "AS_OverlordDAirV6_Build", "AS_OverlordDAirV7_Build",
        "LS_OverlordDAirV1_Build", "LS_OverlordDAirV2_Build", "LS_OverlordDAirV3_Build",
        "LS_OverlordDAirV4_Build", "LS_OverlordDAirV4_Build_VarA", "LS_OverlordDAirV4_Build_VarB", "LS_OverlordDAirV4_Build_VarC",
        "LS_OverlordDAirV4_Build_VarD", "LS_OverlordDAirV4_Build_VarF", "LS_OverlordDAirV4_Build_VarG",
        "LS_OverlordDAirV4_Build_VarH", "LS_OverlordDAirV4_Build_VarI", "LS_OverlordDAirV4_Build_VarJ",
        "LS_OverlordDAirV4_Build_VarK", "LS_OverlordDAirV4_Build_VarL", "LS_OverlordDAirV4_Build_VarM",
        "LS_OverlordDAirV4_VarM_ManualCopy", "LS_OverlordDAirV5_Build", "LS_OverlordDAirV6_Build", "LS_OverlordDAirV7_Build"
    ],
    "Dropkick": [
        "AM_OverlordDropkick", "AS_OverlordDropkickV1_Build", "LS_OverlordDropkickV1_Build",
        "AS_OverlordDropkick_Startup", "AS_OverlordDropkick_Loop", "AS_OverlordDropkick_Land",
        "LS_OverlordDropkick_Startup", "LS_OverlordDropkick_Loop", "LS_OverlordDropkick_Land"
    ]
}

def run():
    moved_count = 0
    errors = []
    
    for subfolder, asset_names in FOLDERS.items():
        target_dir = f"/Game/AnimPipeline/Rig/{subfolder}"
        EAL.make_directory(target_dir)
        
        for name in asset_names:
            src = f"/Game/AnimPipeline/Rig/{name}"
            dst = f"{target_dir}/{name}"
            
            if EAL.does_asset_exist(src):
                ok = EAL.rename_asset(src, dst)
                if ok:
                    moved_count += 1
                else:
                    errors.append(f"Failed to move {src} -> {dst}")
            elif EAL.does_asset_exist(dst):
                # Already in target folder
                pass
            else:
                # Not found
                pass
                
    print(f"Reorganization complete: successfully moved {moved_count} assets into subfolders.")
    if errors:
        print("Errors encountered:", errors)
    return {"moved": moved_count, "errors": errors}

if __name__ == "__main__":
    run()
