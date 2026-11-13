import unreal, json
# Can per-key tangents (arrive/leave slope and weight) be set on Sequencer channels from Python in UE 5.8?
# Test on a pilot Level Sequence: one transform track, Location X keyed 0 -> 100 over 10 frames (a strike-like build),
# then the same curve with user tangents and weights shaped for "slow start, fast arrival". Sampled at 240 fps.
PATH = "/Game/AnimPipeline/Pilot"
NAME = "LS_Pilot_TangentTest"

def main():
    at = unreal.AssetToolsHelpers.get_asset_tools()
    full = f"{PATH}/{NAME}"
    if unreal.EditorAssetLibrary.does_asset_exist(full):
        unreal.EditorAssetLibrary.delete_asset(full)
    seq = at.create_asset(NAME, PATH, unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(60, 1))
    seq.set_playback_start(0); seq.set_playback_end(20)
    binding = seq.add_spawnable_from_class(unreal.Actor)
    tr = binding.add_track(unreal.MovieScene3DTransformTrack)
    sec = tr.add_section(); sec.set_range(0, 20)
    chans = sec.get_all_channels()
    lx = chans[0]                                       # Location.X
    k0 = lx.add_key(unreal.FrameNumber(0), 0.0, 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE)
    k1 = lx.add_key(unreal.FrameNumber(10), 100.0, 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE)
    tick = seq.get_tick_resolution()                    # keys live in ticks; evaluate at 240 fps
    def sample():
        r = unreal.SequencerScriptingRange(has_start_value=True, has_end_value=True, inclusive_start=0,
                                           exclusive_end=int(11 * tick.numerator / tick.denominator / 60))
        vals = lx.evaluate_keys(r, unreal.FrameRate(240, 1))
        return [round(v, 2) for v in vals]
    out = {"methods": [m for m in dir(k1) if "tangent" in m.lower() or "interp" in m.lower()]}
    out["auto"] = sample()
    for k in (k0, k1):
        k.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_CUBIC)
        k.set_tangent_mode(unreal.RichCurveTangentMode.RCTM_USER)
        k.set_tangent_weight_mode(unreal.RichCurveTangentWeightMode.RCTWM_WEIGHTED_BOTH)
    # slow start (flat leave tangent, long weight) and fast arrival (steep arrive tangent, short weight)
    k0.set_leave_tangent(0.0); k0.set_leave_tangent_weight(0.6)
    k1.set_arrive_tangent(0.03); k1.set_arrive_tangent_weight(0.05)
    out["user"] = sample()
    out["readback"] = {"k0_leave": k0.get_leave_tangent(), "k0_leave_w": k0.get_leave_tangent_weight(),
                       "k1_arrive": k1.get_arrive_tangent(), "k1_arrive_w": k1.get_arrive_tangent_weight(),
                       "k1_mode": str(k1.get_tangent_mode()), "tick": [tick.numerator, tick.denominator]}
    unreal.EditorAssetLibrary.save_asset(full)
    return json.dumps(out)
