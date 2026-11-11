import unreal

def run():
    full = "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1"
    seq = unreal.load_asset(full)
    for b in seq.get_spawnables() + seq.get_possessables():
        for t in b.get_tracks():
            if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
                sec = t.get_sections()[0]
                chans = {}
                for c in sec.get_all_channels():
                    cn = str(c.channel_name)
                    ctrl = cn.split(".")[0]
                    prop = ".".join(cn.split(".")[1:])
                    chans.setdefault(ctrl, []).append(prop)
                return {k: v for k, v in sorted(chans.items()) if any(x in k for x in ["pelvis", "spine", "chest", "head", "neck", "arm_l", "arm_r", "hand", "clavicle", "foot_l_ik", "foot_r_ik"])}
    return {}

if __name__ == "__main__":
    run()
