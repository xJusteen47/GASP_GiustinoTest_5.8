"""Editor side of animpipe (runs inside Unreal through the bridge's run_python_file). One Level Sequence per take:
skeletal mesh spawnable + body Control Rig track; keys live on the rig's controls, the AnimSequence is a bake.

Entry points return a short JSON string; anything long is written to the path given in `out`.
  roundtrip(anim, rig, name, fps_num, fps_den, keep_frames, out)  anim -> rig keys -> (optional sparse) -> AnimSequence
  tangent_probe(name, out)                                          per-key tangents/weights on a rig control channel
"""
import json, os
import unreal

MESH = "/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
DIR = "/Game/AnimPipeline/Rig"
EAL = unreal.EditorAssetLibrary


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _rig_class(rig):
    a = unreal.load_asset(rig)
    return a.generated_class() if hasattr(a, "generated_class") else a


def _new(name, cls, factory):
    full = f"{DIR}/{name}"
    if EAL.does_asset_exist(full):
        EAL.delete_asset(full)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DIR, cls, factory)


def _rig_section(binding):
    for t in binding.get_tracks():
        if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
            return t, t.get_sections()[0]
    return None, None


def _sequence(anim, name, rate):
    """Level Sequence with the mannequin as a spawnable and the source anim on it, open in Sequencer."""
    src = unreal.load_asset(anim)
    n = src.get_editor_property("number_of_sampled_frames") if hasattr(src, "number_of_sampled_frames") else None
    length = unreal.AnimationLibrary.get_sequence_length(src) if hasattr(unreal.AnimationLibrary, "get_sequence_length") else src.get_play_length()
    seq = _new(f"LS_{name}", unreal.LevelSequence, unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(rate)
    end = int(round(length * rate.numerator / rate.denominator))
    seq.set_playback_start(0); seq.set_playback_end(end)   # the export includes the end frame
    actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0, 0, 0))
    actor.skeletal_mesh_component.set_skinned_asset_and_update(unreal.load_asset(MESH)) if hasattr(actor.skeletal_mesh_component, "set_skinned_asset_and_update") else actor.skeletal_mesh_component.set_skeletal_mesh(unreal.load_asset(MESH))
    binding = seq.add_spawnable_from_instance(actor)
    actor.destroy_actor()
    tr = binding.add_track(unreal.MovieSceneSkeletalAnimationTrack)
    sec = tr.add_section(); sec.set_range(0, end + 1)
    p = sec.get_editor_property("params"); p.set_editor_property("animation", src)
    tw = p.get_editor_property("play_rate"); tw.set_fixed_play_rate(0.99999); p.set_editor_property("play_rate", tw)   # at t == length the section loops back to frame 0: keep the last frame inside the clip
    sec.set_editor_property("params", p)
    LSE = unreal.LevelSequenceEditorBlueprintLibrary
    LSE.open_level_sequence(seq)
    LSE.refresh_current_level_sequence()
    LSE.set_current_time(0)              # the spawnable must exist in the editor world before the bake can find its mesh
    return seq, binding, end, length


def roundtrip(anim, rig, name, fps_num=60, fps_den=1, keep_frames=None, out=None):
    rate = unreal.FrameRate(int(fps_num), int(fps_den))
    seq, binding, end, length = _sequence(anim, name, rate)
    opts = unreal.AnimSeqExportOption()
    ok = unreal.ControlRigSequencerLibrary.bake_to_control_rig(_world(), seq, _rig_class(rig), opts, False, 0.001, binding)
    track, sec = _rig_section(binding)
    res = {"bake_ok": bool(ok), "end": end, "length": length, "rig_track": track is not None}
    if sec is None:
        return json.dumps(res)
    chans = sec.get_all_channels()
    tick = seq.get_tick_resolution()
    keys_before = sum(len(c.get_keys()) for c in chans)
    removed = 0
    if keep_frames:
        keep = set(int(f) for f in keep_frames)
        for c in chans:
            for k in list(c.get_keys()):
                f = k.get_time(unreal.MovieSceneTimeUnit.DISPLAY_RATE).frame_number.value
                if f not in keep:
                    c.remove_key(k); removed += 1
    res.update({"channels": len(chans), "keys_before": keys_before, "removed": removed,
                "keys_after": sum(len(c.get_keys()) for c in chans), "tick": [tick.numerator, tick.denominator],
                "controls": len([c for c in chans if "." not in c.channel_name.__str__()])})
    # bake back to an AnimSequence
    skel = unreal.load_asset(anim).get_editor_property("skeleton")
    fac = unreal.AnimSequenceFactory(); fac.set_editor_property("target_skeleton", skel)
    dst = _new(f"AS_{name}", unreal.AnimSequence, fac)
    eo = unreal.AnimSeqExportOption()
    for prop, val in (("use_custom_frame_rate", True), ("custom_frame_rate", rate)):
        try:
            eo.set_editor_property(prop, val)
        except Exception as e:
            res.setdefault("export_opt_missing", []).append(prop)
    # the spawnable only exists while the sequence is open: SequencerTools.export_anim_sequence finds no skel mesh
    res["export_ok"] = bool(unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(dst, eo, binding, False))
    res["out_asset"] = dst.get_path_name().split(".")[0]
    try:
        res["out_frames"] = dst.get_editor_property("number_of_sampled_keys")
    except Exception:
        pass
    res["out_length"] = dst.get_play_length()
    EAL.save_asset(seq.get_path_name().split(".")[0]); EAL.save_asset(res["out_asset"])
    unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
    if out:
        json.dump(res, open(out, "w"), indent=1)
    return json.dumps(res)


def _ctrl(chan):
    return str(chan.channel_name).split(".")[0]


def _frame(k):
    return k.get_time(unreal.MovieSceneTimeUnit.DISPLAY_RATE).frame_number.value


# spacing profile -> (leave weight of the segment's first key, arrive weight of its last key) as a fraction of the
# segment; None = smart auto tangents (no stop at the key). Weighted segments get flat tangents: their keys are pose hits.
PROFILE_W = {"charge_long": (0.6, 0.12), "arrive_1_2": (0.12, 0.6), "hold_recoil": (1 / 3, 1 / 3), "limb_led_recovery": None}
# card part -> control name fragments (CR_Mannequin_Body); a part lags by shifting its interior keys later
PIN_TOL = {"Location": 1.0, "Rotation": 2.0}                    # cm, degrees
PART_CTRLS = {"chest": ("spine_05", "chest", "spine_04"), "head": ("head", "neck"), "forearm": ("lowerarm",), "hand": ("hand_",),
              "arm_l": ("upperarm_l", "lowerarm_l", "hand_l"), "arm_r": ("upperarm_r", "lowerarm_r", "hand_r"),
              "leg_l": ("thigh_l", "calf_l", "foot_l"), "leg_r": ("thigh_r", "calf_r", "foot_r"),
              "hand_l": ("hand_l", "hand_l_ik"), "hand_r": ("hand_r", "hand_r_ik"),
              "leg_l_ik": ("foot_l_ik",), "leg_r_ik": ("foot_r_ik",)}   # locks (animpipe/locks.py) read these; part_delays use the first four


def _seg_profile(segments, f):
    """Profile of the card segment that contains frame f (a <= f < b); shifted keys take the profile where they land."""
    for a, b, p in segments:
        if a <= f < b:
            return p
    return None



# --- owner's "Manuale" layer -------------------------------------------------------------------------------------------
# The build deletes and recreates LS_<name> from the card, so hand edits on the base section would be lost. The owner keys
# his tweaks on an ADDITIVE section of the rig track instead (the build creates it empty and makes it the section to key):
# its keys are read before the rebuild, written back on the new additive section, and the export bakes base + layer, so
# the clip and every check see the edit. A JSON copy of the layer is written each build (restored only if LS_ is gone).
_ADD = unreal.MovieSceneBlendType.ADDITIVE


def _additive_sections(seq):
    for b in seq.get_spawnables() + seq.get_possessables():
        for t in b.get_tracks():
            if isinstance(t, unreal.MovieSceneControlRigParameterTrack):
                for s in t.get_sections():
                    if s.get_blend_type().blend_type == _ADD:
                        yield s


def _read_manual(name, backup=None, card_copy=None):
    """{channel: [[frame, value, interp name, tangent mode name, arrive, leave], ...]} from the additive sections of the old
    LS_<name>; without the sequence the Saved backup JSON, then the copy in the card's git folder (animpipe capture)."""
    full = f"{DIR}/LS_{name}"
    if not EAL.does_asset_exist(full):
        for f in (backup, card_copy):
            try:
                if f and os.path.exists(f):
                    return json.load(open(f))
            except (OSError, ValueError):
                pass
        return {}
    seq = unreal.load_asset(full)
    LSE = unreal.LevelSequenceEditorBlueprintLibrary
    LSE.open_level_sequence(seq)
    keys = {}
    for s in _additive_sections(seq):
        for c in s.get_all_channels():
            if isinstance(c, unreal.MovieSceneScriptingFloatChannel):
                for k in c.get_keys():
                    keys.setdefault(str(c.channel_name), []).append(
                        [_frame(k), k.get_value(), k.get_interpolation_mode().name, k.get_tangent_mode().name,
                         k.get_arrive_tangent(), k.get_leave_tangent()])
    LSE.close_level_sequence()
    return keys


def _apply_manual(track, end, keys, backup=None):
    """New additive section over the whole clip with the saved keys; it becomes the section to key for the owner."""
    sec = track.add_section()
    sec.set_blend_type(_ADD)
    sec.set_range(0, end + 1)
    sec.set_row_index(1)              # its own row under the card's section, so the owner sees and picks it
    by = {str(c.channel_name): c for c in sec.get_all_channels() if isinstance(c, unreal.MovieSceneScriptingFloatChannel)}
    n, missing = 0, []
    for ch, ks in keys.items():
        c = by.get(ch)
        if c is None:
            missing.append(ch); continue
        for f, v, im, tm, at, lt in ks:
            k = c.add_key(unreal.FrameNumber(int(f)), float(v), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE)
            k.set_value(float(v))
            k.set_interpolation_mode(getattr(unreal.RichCurveInterpMode, im))
            k.set_tangent_mode(getattr(unreal.RichCurveTangentMode, tm))
            if tm in ("RCTM_USER", "RCTM_BREAK"):
                k.set_arrive_tangent(at); k.set_leave_tangent(lt)
            n += 1
    track.set_section_to_key(sec)
    if backup:
        json.dump(keys, open(backup, "w"), indent=1)
    return {"keys": n, "channels": len(keys) - len(missing), "missing": missing[:10]}


def build(spec):
    """Card -> keys on the body rig -> AnimSequence. spec: {name, source_anim, rig, fps_num, fps_den, poses (anim frames),
    segments [[f0, f1, profile]], part_delays {part: frames}, planted [control names], out}.
    The poses are sampled from source_anim (an existing move now; the owner's Step FBX once imported): bake the anim onto
    the rig, keep only the pose frames, shape the in-betweens with tangents per spacing profile, lag the parts, pin the
    planted IK feet to their first key. No per-frame keys are ever written."""
    rate = unreal.FrameRate(int(spec["fps_num"]), int(spec["fps_den"]))
    name = spec["name"]
    manual = _read_manual(name, spec.get("manual_out"), spec.get("manual_in"))
    seq, binding, end, length = _sequence(spec["source_anim"], name, rate)
    seq.set_playback_end(end + 1)        # the rig bake stops before the playback end: without this the last frame has no key
    ok = unreal.ControlRigSequencerLibrary.bake_to_control_rig(_world(), seq, _rig_class(spec["rig"]), unreal.AnimSeqExportOption(),
                                                                False, 0.001, binding)
    seq.set_playback_end(end)
    track, sec = _rig_section(binding)
    res = {"bake_ok": bool(ok), "end": end, "length": length}
    if sec is None:
        return json.dumps(res)
    keep = set(int(f) for f in spec["poses"])
    segs = spec["segments"]
    # opt-in per pose (card `ease_out`): a flat key whose next segment has no weights leaves with this fraction of the
    # segment as leave weight, so it eases out of the pose instead of leaving at full speed. Absent = old behaviour.
    ease = {int(f): w for f, w in spec.get("ease_out", {}).items()}
    chans = [c for c in sec.get_all_channels() if isinstance(c, unreal.MovieSceneScriptingFloatChannel)]
    res["controls"] = sorted(set(_ctrl(c) for c in chans))
    res["keys_before"] = sum(len(c.get_keys()) for c in chans)
    planted = tuple(spec.get("planted", []))
    delays = {p: int(d) for p, d in spec.get("part_delays", {}).items() if int(d)}
    shifted, pinned = {}, set()
    dropped = edited = blended = 0
    last = max(keep)
    for c in chans:
        # edit the baked keys in place (remove / set_value / set_time): re-adding keys with add_key lost the values
        for k in list(c.get_keys()):
            if _frame(k) not in keep:
                c.remove_key(k)
        ctrl = _ctrl(c)
        cname = str(c.channel_name)
        for d in spec.get("drop", []):
            if d["control"] == ctrl and f".{d.get('attr', '')}" in cname:
                for k in list(c.get_keys()):
                    if _frame(k) in d["frames"]:
                        c.remove_key(k); dropped += 1
        for e in spec.get("edits", []):
            if e["control"] == ctrl and f".{e['attr']}." in cname:
                axis = "XYZ".index(cname.rsplit(".", 1)[1][-1]) if cname.rsplit(".", 1)[1][-1] in "XYZ" else None
                if axis is not None and e["delta"][axis]:
                    for k in c.get_keys():
                        if _frame(k) in e["frames"]:
                            k.set_value(k.get_value() + e["delta"][axis]); edited += 1
        for hb in spec.get("hitstop_blend", []):
            # pull the key at `frame` toward the key at `toward` (both pose keys): no pop out of a hitbox-active frame
            if hb.get("match") and not any(m in ctrl for m in hb["match"]):
                continue
            by = {_frame(k): k for k in c.get_keys()}
            k, t = by.get(int(hb["frame"])), by.get(int(hb["toward"]))
            if k is None or t is None:
                continue
            v, tv = k.get_value(), t.get_value()
            if ".Rotation." in cname:                              # Euler channel: blend the short way round
                tv = v + ((tv - v + 180.0) % 360.0 - 180.0)
            k.set_value(v + float(hb["amount"]) * (tv - v)); blended += 1
        keys = c.get_keys()
        if not keys:
            continue
        if ctrl in planted:           # planted foot: snap small drift back to the first key (a real step is left alone)
            v0 = keys[0].get_value()
            tol = PIN_TOL["Location" if ".Location." in str(c.channel_name) else "Rotation"]
            for k in keys[1:]:
                if 0 < abs(k.get_value() - v0) <= tol:
                    k.set_value(v0); pinned.add(ctrl)
        for part, d in delays.items():
            if any(s in ctrl for s in PART_CTRLS.get(part, ())):
                for k in reversed(keys[1:-1]):                        # back to front: a key never lands on an unmoved one
                    f = _frame(k)
                    if f + d < last:
                        k.set_time(unreal.FrameNumber(f + d), 0.0, unreal.MovieSceneTimeUnit.DISPLAY_RATE)
                    else:
                        c.remove_key(k)
                shifted.setdefault(part, set()).add(ctrl)
                break
        keys = c.get_keys()
        frames = [_frame(k) for k in keys]
        for i, k in enumerate(keys):
            out_w = PROFILE_W.get(_seg_profile(segs, frames[i])) if i + 1 < len(keys) else None
            in_w = PROFILE_W.get(_seg_profile(segs, frames[i - 1])) if i else None
            k.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_CUBIC)
            if out_w is None and in_w is None:
                k.set_tangent_mode(unreal.RichCurveTangentMode.RCTM_SMART_AUTO)
                continue
            k.set_tangent_mode(unreal.RichCurveTangentMode.RCTM_USER)
            k.set_tangent_weight_mode(unreal.RichCurveTangentWeightMode.RCTWM_WEIGHTED_BOTH)
            k.set_arrive_tangent(0.0); k.set_leave_tangent(0.0)
            spf = rate.denominator / rate.numerator                    # seconds per display frame
            if out_w is not None:
                k.set_leave_tangent_weight(out_w[0] * (frames[i + 1] - frames[i]) * spf)
            elif frames[i] in ease and i + 1 < len(keys):
                k.set_leave_tangent_weight(ease[frames[i]] * (frames[i + 1] - frames[i]) * spf)
            if in_w is not None:
                k.set_arrive_tangent_weight(in_w[1] * (frames[i] - frames[i - 1]) * spf)
    res.update({"keys_after": sum(len(c.get_keys()) for c in chans), "pinned": sorted(pinned), "dropped": dropped, "edited": edited, "blended": blended,
                "shifted": {p: sorted(v) for p, v in shifted.items()}})
    res["manual"] = _apply_manual(track, end, manual, spec.get("manual_out"))
    skel = unreal.load_asset(spec["source_anim"]).get_editor_property("skeleton")
    fac = unreal.AnimSequenceFactory(); fac.set_editor_property("target_skeleton", skel)
    dst = _new(f"AS_{name}", unreal.AnimSequence, fac)
    eo = unreal.AnimSeqExportOption()
    eo.set_editor_property("use_custom_frame_rate", True); eo.set_editor_property("custom_frame_rate", rate)
    res["export_ok"] = bool(unreal.ControlRigSequencerLibrary.export_anim_sequence_from_sequencer(dst, eo, binding, False))
    res["out_asset"] = dst.get_path_name().split(".")[0]
    res["out_length"] = dst.get_play_length()
    if spec.get("lock_lengths", True):
        res["lock_lengths"] = _lock_lengths(dst, unreal.load_asset(spec["source_anim"]))
    EAL.save_asset(seq.get_path_name().split(".")[0]); EAL.save_asset(res["out_asset"])
    unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
    if spec.get("out"):
        json.dump(res, open(spec["out"], "w"), indent=1)
    return json.dumps({k: v for k, v in res.items() if k not in ("controls", "shifted")})


FREE_TRANSLATION = ("root", "pelvis", "ik_", "vb ")   # bones whose translation is motion, not length


def _lock_lengths(dst, src):
    """Rig stretch off, on the bake: every bone except root/pelvis/ik_* keeps the source's frame-0 local translation on
    every frame (rotation untouched), so bone lengths are exact (gate 0.1 cm). Returns the largest change it removed (cm)."""
    ctrl = dst.get_editor_property("controller")
    model = dst.get_editor_property("data_model")
    n = dst.get_editor_property("number_of_sampled_keys")
    names = [str(b) for b in unreal.AnimationLibrary.get_animation_track_names(dst)]
    worst, done, wb, per = 0.0, 0, "", {}
    ctrl.open_bracket(unreal.Text("animpipe lock lengths"), False)
    for b in names:
        if b.startswith(FREE_TRANSLATION):
            continue
        ref = unreal.AnimationLibrary.get_bone_pose_for_frame(src, b, 0, False).translation
        pos, rot, scl = [], [], []
        for f in range(n):
            t = unreal.AnimationLibrary.get_bone_pose_for_frame(dst, b, f, False)
            dd = (t.translation - ref).length()
            per[b] = max(per.get(b, 0.0), dd)
            if dd > worst:
                worst, wb = dd, f"{b}@{f}"
            pos.append(ref); rot.append(t.rotation); scl.append(t.scale3d)
        ctrl.set_bone_track_keys(b, pos, rot, scl, False)
        done += 1
    ctrl.close_bracket(False)
    big = sorted(((round(v, 2), k) for k, v in per.items() if v > 0.1), reverse=True)[:8]
    return {"bones": done, "removed_max_cm": round(worst, 3), "at": wb, "over_0.1": big, "frames": n}


def probe(name, out=None):
    """Channel names, key counts and value spans on the rig track of LS_<name>, for writing card edits."""
    seq = unreal.load_asset(f"{DIR}/LS_{name}")
    unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)
    res = {}
    for b in seq.get_spawnables() + seq.get_possessables():
        t, sec = _rig_section(b)
        if sec is None:
            continue
        for c in sec.get_all_channels():
            ks = c.get_keys() if hasattr(c, "get_keys") else []
            if not ks:
                continue
            try:
                vals = [k.get_value() for k in ks]
                res[str(c.channel_name)] = [len(ks), round(min(vals), 2), round(max(vals), 2), [_frame(k) for k in ks], [round(v, 2) for v in vals]] if not isinstance(vals[0], bool) else [len(ks), vals]
            except Exception as e:
                res[str(c.channel_name)] = str(e)[:80]
    unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
    if out:
        json.dump(res, open(out, "w"), indent=1)
    return json.dumps({"channels": len(res)})


def tangent_probe(name, out=None):
    """On the rig track of LS_<name>: pick hand_l_ctrl Location X (or the first float channel with 2+ keys), set user cubic
    tangents and weights on two keys, read them back and sample the curve at 240 fps between them."""
    seq = unreal.load_asset(f"{DIR}/LS_{name}")
    unreal.LevelSequenceEditorBlueprintLibrary.open_level_sequence(seq)
    res = {}
    for b in seq.get_spawnables() + seq.get_possessables():
        t, sec = _rig_section(b)
        if sec is None:
            continue
        chans = [c for c in sec.get_all_channels() if isinstance(c, unreal.MovieSceneScriptingFloatChannel) and len(c.get_keys()) >= 2]
        # the channel that moves most between its first two keys (a flat pair cannot show a tangent change)
        pick = max(chans, key=lambda c: abs(c.get_keys()[1].get_value() - c.get_keys()[0].get_value())) if chans else None
        if pick is None:
            break
        k0, k1 = pick.get_keys()[0], pick.get_keys()[1]
        f0 = k0.get_time(unreal.MovieSceneTimeUnit.DISPLAY_RATE).frame_number.value
        f1 = k1.get_time(unreal.MovieSceneTimeUnit.DISPLAY_RATE).frame_number.value
        tick = seq.get_tick_resolution()
        rng = unreal.SequencerScriptingRange(has_start_value=True, has_end_value=True,
                                             inclusive_start=int(f0 * tick.numerator / tick.denominator / 60),
                                             exclusive_end=int((f1 + 1) * tick.numerator / tick.denominator / 60))
        before = [round(v, 3) for v in pick.evaluate_keys(rng, unreal.FrameRate(240, 1))]
        for k in (k0, k1):
            k.set_interpolation_mode(unreal.RichCurveInterpMode.RCIM_CUBIC)
            k.set_tangent_mode(unreal.RichCurveTangentMode.RCTM_USER)
            k.set_tangent_weight_mode(unreal.RichCurveTangentWeightMode.RCTWM_WEIGHTED_BOTH)
        k0.set_leave_tangent(0.0); k0.set_leave_tangent_weight(0.5)
        k1.set_arrive_tangent(0.0); k1.set_arrive_tangent_weight(0.05)
        after = [round(v, 3) for v in pick.evaluate_keys(rng, unreal.FrameRate(240, 1))]
        res = {"channel": str(pick.channel_name), "frames": [f0, f1],
               "readback": [k0.get_leave_tangent_weight(), k1.get_arrive_tangent_weight(), str(k1.get_tangent_mode())],
               "curve_changed": before != after, "before": before, "after": after}
        break
    unreal.LevelSequenceEditorBlueprintLibrary.close_level_sequence()
    if out:
        json.dump(res, open(out, "w"), indent=1)
    return json.dumps({k: v for k, v in res.items() if k not in ("before", "after")})
