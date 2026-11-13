import unreal, json
def main():
    out = {"GeometryScript_SceneUtils": hasattr(unreal, "GeometryScript_SceneUtils"), "DynamicMesh": hasattr(unreal, "DynamicMesh")}
    if out["GeometryScript_SceneUtils"]:
        out["fns"] = [f for f in dir(unreal.GeometryScript_SceneUtils) if "mesh" in f.lower()][:20]
    out["skinned_fns"] = [f for f in dir(unreal.SkinnedMeshComponent) if "vert" in f.lower() or "skinned" in f.lower()][:20]
    return json.dumps(out)
