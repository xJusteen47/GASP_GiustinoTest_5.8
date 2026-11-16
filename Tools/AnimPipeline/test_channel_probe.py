import unreal

def run():
    full = "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1"
    seq = unreal.load_asset(full)
    for b in seq.get_spawnables() + seq.get_possessables():
        for t in b.get_tracks():
            if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
                sec = t.get_sections()[0]
                res = {}
                for c in sec.get_all_channels():
                    cn = str(c.channel_name)
                    if any(x in cn for x in ["upperarm_r_fk_ctrl.Rotation", "hand_r_fk_ctrl.Rotation", "chest_ctrl.Rotation", "pelvis_ctrl.Location"]):
                        res[cn] = [c.get_num_keys()]
                return res
    return {}

if __name__ == "__main__":
    run()
