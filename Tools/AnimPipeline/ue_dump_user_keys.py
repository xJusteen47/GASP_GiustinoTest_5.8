import unreal, json

def run(name="GanonFTiltV31_Build_VarD", frames="20,23,25,27,32,37"):
    full = f"/Game/AnimPipeline/Rig/LS_{name}"
    seq = unreal.load_asset(full)
    if not seq:
        return {"error": f"Asset {full} not found"}
    
    target_frames = [int(x) for x in frames.split(",") if x]
    res = {}
    for b in seq.get_spawnables() + seq.get_possessables():
        for t in b.get_tracks():
            if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
                sec = t.get_sections()[0]
                for c in sec.get_all_channels():
                    if isinstance(c, unreal.MovieSceneScriptingFloatChannel):
                        cname = str(c.channel_name)
                        if any(x in cname for x in ["upperarm_r", "lowerarm_r", "hand_r"]):
                            for k in c.get_keys():
                                f = k.get_time(unreal.MovieSceneTimeUnit.DISPLAY_RATE).frame_number.value
                                if f in target_frames:
                                    res.setdefault(f"f{f}", {})[cname] = round(k.get_value(), 2)
    return res
