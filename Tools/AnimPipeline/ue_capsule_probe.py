import unreal, json
def main():
    out = {}
    for path in ("/Game/Blueprints/SandboxCharacter_Mover", "/Game/Blueprints/SandboxCharacter_CMC"):
        try:
            bp = unreal.load_asset(path)
            cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
            cdo = unreal.get_default_object(cls)
            comps = {}
            for c in cdo.get_components_by_class(unreal.ActorComponent):
                d = {"class": c.get_class().get_name()}
                if isinstance(c, unreal.CapsuleComponent):
                    d["half_height"] = c.get_unscaled_capsule_half_height(); d["radius"] = c.get_unscaled_capsule_radius()
                if isinstance(c, unreal.SceneComponent):
                    d["rel_loc"] = [round(v, 1) for v in c.get_editor_property("relative_location").to_tuple()]
                comps[c.get_name()] = d
            out[path] = comps
        except Exception as e:
            out[path] = str(e)
    return out
