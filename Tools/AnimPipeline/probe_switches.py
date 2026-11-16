import unreal

def run():
    full = "/Game/AnimPipeline/Rig/Overlord/Jab/Jab1/LS_OverlordJab1"
    seq = unreal.load_asset(full)
    for b in seq.get_spawnables() + seq.get_possessables():
        for t in b.get_tracks():
            if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
                sec = t.get_sections()[0]
                switches = {}
                for c in sec.get_all_channels():
                    name = str(c.channel_name)
                    if "switch" in name.lower() or "mode" in name.lower():
                        keys = c.get_keys()
                        switches[name] = [k.get_value() for k in keys[:3]] if keys else "no keys"
                return switches
    return {}

if __name__ == "__main__":
    run()
