"""Run stop v2 (pose-based clip): read-only dump of the GASP clips the new clip starts from and ends on, in component space.
Writes Saved/RunStop2/dump.json: per clip {fps, frames, root motion flags, per frame {bone: [x,y,z,qx,qy,qz,qw]}} (root included).
Headless: UnrealEditor-Cmd <uproject> -nullrhi -ExecCmds="py <copy of this file>"."""
import json, os, traceback
import unreal

OUT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + "RunStop2/dump.json"
CLIPS = {
    "idle": "/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Neutral_Stand_Idle_Loop",
    "run": "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F",
    "stopL": "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Stop_F_Lfoot",
    "stopR": "/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Stop_F_Rfoot",
    "tmpl": "/Game/AnimPipeline/Pilot/AS_Pilot_GanonFTilt_V2",
}
if os.environ.get("RS2_CLIPS"):   # name=path;name=path (and RS2_OUT) to sample other clips
    CLIPS = dict(x.split("=", 1) for x in os.environ["RS2_CLIPS"].split(";"))
    OUT = os.path.join(os.path.dirname(OUT), os.environ.get("RS2_OUT", "dump2.json"))
BONES = ["root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01", "neck_02", "head"]
for s in "lr":
    BONES += [f"clavicle_{s}", f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}", f"thigh_{s}", f"calf_{s}", f"foot_{s}", f"ball_{s}"]
PARENT = {"root": None, "pelvis": "root", "spine_01": "pelvis", "spine_02": "spine_01", "spine_03": "spine_02", "spine_04": "spine_03",
          "spine_05": "spine_04", "neck_01": "spine_05", "neck_02": "neck_01", "head": "neck_02"}
for s in "lr":
    PARENT.update({f"clavicle_{s}": "spine_05", f"upperarm_{s}": f"clavicle_{s}", f"lowerarm_{s}": f"upperarm_{s}", f"hand_{s}": f"lowerarm_{s}",
                   f"thigh_{s}": "pelvis", f"calf_{s}": f"thigh_{s}", f"foot_{s}": f"calf_{s}", f"ball_{s}": f"foot_{s}"})


def comp(seq, f):
    loc = {b: unreal.AnimationLibrary.get_bone_pose_for_frame(seq, b, f, False) for b in BONES}
    out = {}
    def get(b):
        if b not in out:
            p = PARENT[b]
            out[b] = loc[b] if p is None else unreal.MathLibrary.compose_transforms(loc[b], get(p))
        return out[b]
    res = {}
    for b in BONES:
        t = get(b); q = t.rotation; l = t.translation
        res[b] = [l.x, l.y, l.z, q.x, q.y, q.z, q.w]
    return res


data = {}
try:
    for k, path in CLIPS.items():
        seq = unreal.load_asset(path)
        n = unreal.AnimationLibrary.get_num_frames(seq)
        ln = seq.get_play_length()
        d = {"path": path, "frames": n, "fps": n / ln if ln else 0,
             "enable_root_motion": seq.get_editor_property("enable_root_motion"),
             "force_root_lock": seq.get_editor_property("force_root_lock"),
             "root_motion_root_lock": str(seq.get_editor_property("root_motion_root_lock")),
             "pose": {}}
        for f in range(n + 1):
            d["pose"][f] = comp(seq, f)
        data[k] = d
    data["ok"] = True
except Exception:
    data["error"] = traceback.format_exc()
os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(data, open(OUT, "w"))
unreal.SystemLibrary.quit_editor()
