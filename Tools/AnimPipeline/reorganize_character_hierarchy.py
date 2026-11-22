"""Reorganize Unreal Engine assets into Character -> Move -> Submove -> Asset hierarchy.

Target Hierarchy:
/Game/AnimPipeline/Rig/
  Overlord/
    Jab/
      Jab1/
        AS_OverlordJab1, LS_OverlordJab1
      Jab2/
        AS_OverlordJab2, LS_OverlordJab2
      AM_OverlordJab, LS_OverlordJab_Preview
    Dropkick/
      Startup/
        AS_OverlordDropkick_Startup, LS_OverlordDropkick_Startup
      Loop/
        AS_OverlordDropkick_Loop, LS_OverlordDropkick_Loop
      Land/
        AS_OverlordDropkick_Land, LS_OverlordDropkick_Land
      AM_OverlordDropkick, AS_OverlordDropkickV1_Build, LS_OverlordDropkickV1_Build
    FTilt/
      AS_GanonFTilt*, LS_GanonFTilt*, AM_GanonFTilt*, AS_TestFTilt*, LS_TestFTilt*
    DAir/
      AS_OverlordDAir*, LS_OverlordDAir*, AM_OverlordDAir*
  Pilot/
    Jab/
      AS_JabV11*, AS_JabV12*, LS_Jab*, AM_JabV12_Build
    Tests/
      AS_Jac*, LS_Jac*, AS_Prb*, LS_Prb*
"""
import unreal

EAL = unreal.EditorAssetLibrary

ASSET_MAPPING = {
    # --- Overlord Jab ---
    "AS_OverlordJab1": "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/AS_OverlordJab1",
    "LS_OverlordJab1": "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1",
    "AS_OverlordJab2": "/Game/AnimPipeline/Rig/Overlord/Jab/Jab2/AS_OverlordJab2",
    "LS_OverlordJab2": "/Game/AnimPipeline/Rig/Overlord/Jab/Jab2/LS_OverlordJab2",
    "AM_OverlordJab": "/Game/AnimPipeline/Rig/Overlord/Jab/AM_OverlordJab",
    "LS_OverlordJab_Preview": "/Game/AnimPipeline/Rig/Overlord/Jab/LS_OverlordJab_Preview",

    # --- Overlord Dropkick ---
    "AS_OverlordDropkick_Startup": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Startup/AS_OverlordDropkick_Startup",
    "LS_OverlordDropkick_Startup": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Startup/LS_OverlordDropkick_Startup",
    "AS_OverlordDropkick_Loop": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Loop/AS_OverlordDropkick_Loop",
    "LS_OverlordDropkick_Loop": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Loop/LS_OverlordDropkick_Loop",
    "AS_OverlordDropkick_Land": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Land/AS_OverlordDropkick_Land",
    "LS_OverlordDropkick_Land": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Land/LS_OverlordDropkick_Land",
    "AM_OverlordDropkick": "/Game/AnimPipeline/Rig/Overlord/Dropkick/AM_OverlordDropkick",
    "AS_OverlordDropkickV1_Build": "/Game/AnimPipeline/Rig/Overlord/Dropkick/AS_OverlordDropkickV1_Build",
    "LS_OverlordDropkickV1_Build": "/Game/AnimPipeline/Rig/Overlord/Dropkick/LS_OverlordDropkickV1_Build",

    # --- Pilot Jab ---
    "AS_JabV11_Build": "/Game/AnimPipeline/Rig/Pilot/Jab/AS_JabV11_Build",
    "AS_JabV11_RT60": "/Game/AnimPipeline/Rig/Pilot/Jab/AS_JabV11_RT60",
    "AS_JabV11_Sparse": "/Game/AnimPipeline/Rig/Pilot/Jab/AS_JabV11_Sparse",
    "AS_JabV12_Build": "/Game/AnimPipeline/Rig/Pilot/Jab/AS_JabV12_Build",
    "LS_JabV11_Build": "/Game/AnimPipeline/Rig/Pilot/Jab/LS_JabV11_Build",
    "LS_JabV11_RT60": "/Game/AnimPipeline/Rig/Pilot/Jab/LS_JabV11_RT60",
    "LS_JabV11_Sparse": "/Game/AnimPipeline/Rig/Pilot/Jab/LS_JabV11_Sparse",
    "LS_JabV12_Build": "/Game/AnimPipeline/Rig/Pilot/Jab/LS_JabV12_Build",
    "AM_JabV12_Build": "/Game/AnimPipeline/Rig/Pilot/Jab/AM_JabV12_Build",

    # --- Pilot Tests ---
    "AS_Jac1_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/AS_Jac1_Build",
    "AS_Jac2_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/AS_Jac2_Build",
    "AS_Jac3_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/AS_Jac3_Build",
    "LS_Jac1_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/LS_Jac1_Build",
    "LS_Jac2_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/LS_Jac2_Build",
    "LS_Jac3_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/LS_Jac3_Build",
    "AS_PrbG0_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/AS_PrbG0_Build",
    "AS_PrbG1_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/AS_PrbG1_Build",
    "LS_PrbG0_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/LS_PrbG0_Build",
    "LS_PrbG1_Build": "/Game/AnimPipeline/Rig/Pilot/Tests/LS_PrbG1_Build",
}

# Add FTilt
FTILT_NAMES = [
    "AS_GanonFTiltV31_Build", "AS_GanonFTiltV31_Build_VarA", "AS_GanonFTiltV31_Build_VarB",
    "AS_GanonFTiltV31_Build_VarC", "AS_GanonFTiltV31_Build_VarD", "AS_TestFTiltVarDArm",
    "LS_GanonFTiltV31_Build", "LS_GanonFTiltV31_Build_VarA", "LS_GanonFTiltV31_Build_VarB",
    "LS_GanonFTiltV31_Build_VarC", "LS_GanonFTiltV31_Build_VarD", "LS_TestFTiltVarDArm",
    "AM_GanonFTiltV31_Build", "AS_GanonAxisProbe_Build", "LS_GanonAxisProbe_Build"
]
for n in FTILT_NAMES:
    ASSET_MAPPING[n] = f"/Game/AnimPipeline/Rig/Overlord/FTilt/{n}"

# Add DAir
DAIR_NAMES = [
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
]
for n in DAIR_NAMES:
    ASSET_MAPPING[n] = f"/Game/AnimPipeline/Rig/Overlord/DAir/{n}"

POSSIBLE_SOURCES = [
    "/Game/AnimPipeline/Rig",
    "/Game/AnimPipeline/Rig/Jab",
    "/Game/AnimPipeline/Rig/FTilt",
    "/Game/AnimPipeline/Rig/DAir",
    "/Game/AnimPipeline/Rig/Dropkick"
]

def run():
    moved = 0
    already_there = 0
    errors = []
    
    for name, dst in ASSET_MAPPING.items():
        if EAL.does_asset_exist(dst):
            already_there += 1
            continue
            
        src_found = None
        for src_dir in POSSIBLE_SOURCES:
            candidate = f"{src_dir}/{name}"
            if EAL.does_asset_exist(candidate):
                src_found = candidate
                break
                
        if not src_found:
            errors.append(f"Source not found for {name}")
            continue
            
        target_dir = dst.rsplit("/", 1)[0]
        EAL.make_directory(target_dir)
        ok = EAL.rename_asset(src_found, dst)
        if ok:
            moved += 1
        else:
            errors.append(f"Failed to move {src_found} -> {dst}")

    # Clean up empty old directories
    old_dirs = [
        "/Game/AnimPipeline/Rig/Jab",
        "/Game/AnimPipeline/Rig/FTilt",
        "/Game/AnimPipeline/Rig/DAir",
        "/Game/AnimPipeline/Rig/Dropkick"
    ]
    cleaned = []
    for d in old_dirs:
        assets_left = EAL.list_assets(d, recursive=True, include_folder=False)
        if len(assets_left) == 0:
            EAL.delete_directory(d)
            cleaned.append(d)

    print(f"Hierarchy migration complete: moved {moved}, already in place {already_there}, errors {len(errors)}, cleaned dirs {cleaned}")
    return {"moved": moved, "already_there": already_there, "errors": errors, "cleaned_dirs": cleaned}

if __name__ == "__main__":
    run()
