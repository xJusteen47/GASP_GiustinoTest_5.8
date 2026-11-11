import os, glob, json

HERE = os.path.dirname(os.path.abspath(__file__))

# 1. Update ue_rig.py
ue_rig_file = os.path.join(HERE, "animpipe", "ue_rig.py")
with open(ue_rig_file, "r", encoding="utf-8") as f:
    c = f.read()

target = """def _find_seq_path(name, folder=None):
    if folder:
        full = f"{folder}/LS_{name}"
        if EAL.does_asset_exist(full):
            return full
    full = f"{DIR}/LS_{name}"
    if EAL.does_asset_exist(full):
        return full
    for sub in ("Jab", "FTilt", "DAir", "Dropkick"):
        sub_full = f"{DIR}/{sub}/LS_{name}"
        if EAL.does_asset_exist(sub_full):
            return sub_full
    return full"""

replacement = """def _find_seq_path(name, folder=None):
    if folder:
        full = f"{folder}/LS_{name}"
        if EAL.does_asset_exist(full):
            return full
    full = f"{DIR}/LS_{name}"
    if EAL.does_asset_exist(full):
        return full
    subdirs = [
        f"{DIR}/Overlord/Jab/Jab1", f"{DIR}/Overlord/Jab/Jab2", f"{DIR}/Overlord/Jab",
        f"{DIR}/Overlord/Dropkick/Startup", f"{DIR}/Overlord/Dropkick/Loop", f"{DIR}/Overlord/Dropkick/Land", f"{DIR}/Overlord/Dropkick",
        f"{DIR}/Overlord/FTilt", f"{DIR}/Overlord/DAir",
        f"{DIR}/Pilot/Jab", f"{DIR}/Pilot/Tests"
    ]
    for sub in subdirs:
        sub_full = f"{sub}/LS_{name}"
        if EAL.does_asset_exist(sub_full):
            return sub_full
    try:
        assets = EAL.list_assets(DIR, recursive=True, include_folder=False)
        target = f"/LS_{name}."
        for a in assets:
            if target in str(a):
                return str(a).split(".")[0]
    except Exception:
        pass
    return full"""

# Handle both CRLF and LF
c_norm = c.replace("\r\n", "\n")
target_norm = target.replace("\r\n", "\n")
replacement_norm = replacement.replace("\r\n", "\n")
if target_norm in c_norm:
    c_updated = c_norm.replace(target_norm, replacement_norm)
    with open(ue_rig_file, "w", encoding="utf-8", newline="\n") as f:
        f.write(c_updated)
    print("Updated animpipe/ue_rig.py")
else:
    print("Target not found in ue_rig.py (may already be updated)")

# 2. Update Card anim paths
cards_dir = os.path.join(HERE, "cards")

PATH_REPLACEMENTS = {
    # Jab
    "/Game/AnimPipeline/Rig/AS_OverlordJab1": "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/AS_OverlordJab1",
    "/Game/AnimPipeline/Rig/Jab/AS_OverlordJab1": "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/AS_OverlordJab1",
    "/Game/AnimPipeline/Rig/AS_OverlordJab2": "/Game/AnimPipeline/Rig/Overlord/Jab/Jab2/AS_OverlordJab2",
    "/Game/AnimPipeline/Rig/Jab/AS_OverlordJab2": "/Game/AnimPipeline/Rig/Overlord/Jab/Jab2/AS_OverlordJab2",
    
    # Dropkick
    "/Game/AnimPipeline/Rig/AS_OverlordDropkick_Startup": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Startup/AS_OverlordDropkick_Startup",
    "/Game/AnimPipeline/Rig/Dropkick/AS_OverlordDropkick_Startup": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Startup/AS_OverlordDropkick_Startup",
    "/Game/AnimPipeline/Rig/AS_OverlordDropkick_Loop": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Loop/AS_OverlordDropkick_Loop",
    "/Game/AnimPipeline/Rig/Dropkick/AS_OverlordDropkick_Loop": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Loop/AS_OverlordDropkick_Loop",
    "/Game/AnimPipeline/Rig/AS_OverlordDropkick_Land": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Land/AS_OverlordDropkick_Land",
    "/Game/AnimPipeline/Rig/Dropkick/AS_OverlordDropkick_Land": "/Game/AnimPipeline/Rig/Overlord/Dropkick/Land/AS_OverlordDropkick_Land",
    "/Game/AnimPipeline/Rig/AS_OverlordDropkickV1_Build": "/Game/AnimPipeline/Rig/Overlord/Dropkick/AS_OverlordDropkickV1_Build",
    "/Game/AnimPipeline/Rig/Dropkick/AS_OverlordDropkickV1_Build": "/Game/AnimPipeline/Rig/Overlord/Dropkick/AS_OverlordDropkickV1_Build",

    # FTilt
    "/Game/AnimPipeline/Rig/FTilt/": "/Game/AnimPipeline/Rig/Overlord/FTilt/",
    "/Game/AnimPipeline/Rig/AS_GanonFTilt": "/Game/AnimPipeline/Rig/Overlord/FTilt/AS_GanonFTilt",

    # DAir
    "/Game/AnimPipeline/Rig/DAir/": "/Game/AnimPipeline/Rig/Overlord/DAir/",
    "/Game/AnimPipeline/Rig/AS_OverlordDAir": "/Game/AnimPipeline/Rig/Overlord/DAir/AS_OverlordDAir",

    # Pilot
    "/Game/AnimPipeline/Rig/AS_Jab": "/Game/AnimPipeline/Rig/Pilot/Jab/AS_Jab",
}

for root, _, files in os.walk(cards_dir):
    for fn in files:
        if fn.endswith(".json"):
            fp = os.path.join(root, fn)
            with open(fp, "r", encoding="utf-8") as f:
                card_data = f.read()
            orig = card_data
            for k, v in PATH_REPLACEMENTS.items():
                card_data = card_data.replace(f'"{k}', f'"{v}')
            if card_data != orig:
                with open(fp, "w", encoding="utf-8") as f:
                    f.write(card_data)
                print(f"Updated card anim path in {fn}")
