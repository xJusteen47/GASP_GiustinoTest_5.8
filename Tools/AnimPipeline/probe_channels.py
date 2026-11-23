import unreal

def run():
    full = "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1"
    seq = unreal.load_asset(full)
    if not seq:
        return {"error": "Not found"}
    chans = []
    for b in seq.get_spawnables() + seq.get_possessables():
        for t in b.get_tracks():
            if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
                sec = t.get_sections()[0]
                for c in sec.get_all_channels():
                    chans.append(str(c.channel_name))
    return sorted(chans)

if __name__ == "__main__":
    run()
