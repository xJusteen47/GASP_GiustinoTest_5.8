# Editor-side, read-only: collision primitives of the UEFN mannequin physics asset (bone space), for the clearance check.
# Uses the project's GSTN_PhysicsAssetUtils.list_primitives (the PhysicsAsset body list is not exposed to Python).
import unreal, json
MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"

def main():
    pa = unreal.load_asset(MESH).get_editor_property("physics_asset")
    out = {"physics_asset": pa.get_path_name(), "prims": []}
    for p in unreal.GSTN_PhysicsAssetUtils.list_primitives(pa):
        s = p.get_editor_property("shape")
        e = s.get_editor_property("extent"); t = s.get_editor_property("local")
        q = t.rotation; tr = t.translation
        out["prims"].append({"bone": str(p.get_editor_property("bone")), "type": str(s.get_editor_property("type")).split(".")[-1],
                             "extent": [e.x, e.y, e.z], "quat": [q.x, q.y, q.z, q.w], "trans": [tr.x, tr.y, tr.z]})
    return json.dumps(out)
