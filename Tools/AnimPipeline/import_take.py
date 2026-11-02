"""Import a baked FBX from Saved/AnimPipeline as /Game/AnimPipeline/Pilot/<asset> (not saved). Usage: import_take.py <fbx name> <asset name>"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ue
fbx = os.path.join(ue.HERE, sys.argv[1]).replace("\\", "/")
r = ue.call("import_animation", {"filePath": fbx, "skeletonPath": "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin",
                                 "packagePath": "/Game/AnimPipeline/Pilot", "name": sys.argv[2], "removeRedundantKeys": False,
                                 "importSettings": {"animationLength": "FBXALIT_ExportedTime"}})
info = ue.call("read_anim_sequence", {"assetPath": f"/Game/AnimPipeline/Pilot/{sys.argv[2]}"})
print(r.get("success"), r.get("importedAssets"), info.get("numFrames"), info.get("frameRate"), round(info.get("sequenceLength", 0), 4))
