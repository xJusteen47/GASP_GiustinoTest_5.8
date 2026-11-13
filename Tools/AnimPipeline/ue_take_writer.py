"""Unreal side of ue_write_take.py (run through the editor bridge, run_python_file): writes local bone tracks into an
AnimSequence under /Game/AnimPipeline/Pilot/, made as a copy of a template take (same skeleton; tracks we do not write keep
the template's keys). Never touches anything outside /Game/AnimPipeline/Pilot/."""
import json
import unreal

PILOT = "/Game/AnimPipeline/Pilot/"


def info(asset):
    anim = unreal.EditorAssetLibrary.load_asset(asset)
    ctrl = anim.get_editor_property("controller")
    model = anim.get_data_model() if hasattr(anim, "get_data_model") else None
    return repr({"model": str(model), "methods": [m for m in dir(model) if not m.startswith("_")][:80] if model else None})


def write(src, template, dest):
    if not dest.startswith(PILOT) or not template.startswith(PILOT):
        return repr({"error": "assets must be under " + PILOT})
    d = json.load(open(src))
    eal = unreal.EditorAssetLibrary
    if not eal.does_asset_exist(dest):
        eal.duplicate_asset(template, dest)
    anim = eal.load_asset(dest)
    ctrl = anim.get_editor_property("controller")
    n = int(d["keys"])
    ctrl.open_bracket(unreal.Text("GSTN write take"), True)
    ctrl.set_number_of_frames(unreal.FrameNumber(n - 1), True)
    done = []
    for b, keys in d["bones"].items():
        pos = [unreal.Vector(k[0], k[1], k[2]) for k in keys]
        rot = [unreal.Quat(k[3], k[4], k[5], k[6]) for k in keys]
        sc = [unreal.Vector(1.0, 1.0, 1.0) for _ in keys]
        if ctrl.set_bone_track_keys(unreal.Name(b), pos, rot, sc, True):
            done.append(b)
    ctrl.close_bracket(True)
    eal.save_asset(dest, only_if_is_dirty=False)
    return repr({"written": len(done), "missing": sorted(set(d["bones"]) - set(done)),
                 "length": anim.get_editor_property("sequence_length") if hasattr(anim, "sequence_length") else None})
