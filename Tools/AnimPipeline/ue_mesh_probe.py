import unreal, json, os
def main(frame=0, anim_path="/Game/AnimPipeline/Pilot/AS_Pilot_JabV9"):
    import importlib, sys
    sys.path.append(os.environ.get("ANIMPIPELINE_DIR") or os.path.dirname(os.path.abspath(__file__)))
    import capture_ue; importlib.reload(capture_ue)
    capture_ue.set_frame(anim_path, int(frame))
    comp = capture_ue._actor("PilotChar").skeletal_mesh_component
    mesh = unreal.DynamicMesh()
    opts = unreal.GeometryScriptCopyMeshFromComponentOptions()
    res = unreal.GeometryScript_SceneUtils.copy_mesh_from_component(comp, mesh, opts, False)
    out = {"res": str(res)[:200]}
    try:
        out["verts"] = unreal.GeometryScript_MeshQueries.get_vertex_count(mesh)
        pos = unreal.GeometryScript_MeshQueries.get_vertex_position(mesh, 100)
        out["v100"] = [round(x, 3) for x in (pos[0].x, pos[0].y, pos[0].z)] if isinstance(pos, tuple) else str(pos)
    except Exception as e:
        out["err"] = str(e)[:300]
    return json.dumps(out)
