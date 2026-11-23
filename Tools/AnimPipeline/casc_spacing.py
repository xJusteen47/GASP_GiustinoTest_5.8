# Cascadeur side of `animpipe casc-spacing` (run in Cascadeur: the cascadeur-plus run_python tool, or csc_run.py).
# Shapes the in-betweens between the card's key frames with the key API only (interval interpolation, key tangent mode,
# left/right easing weights): no key is added, moved or re-posed, so the key-frame poses (owner locks included) cannot
# change; "apply" measures it and refuses to save if a key pose moved.
# Needs SPACING = path of a JSON plan written by `python -m animpipe casc-spacing` (animpipe/spacing.py):
#   tab, mode, bones, out, and per mode:
#   probe  "variants": [{"name", "segments": {"<a>": seg}}]: apply each in turn, write {name: {frame: {bone: xyz}}}.
#   fit    "target": {frame: {bone: xyz}} (the build's in-betweens in Cascadeur space, key-pose offsets removed),
#          "candidates": [seg], "fit": [segment start frames], "passes", "skip_layers": coordinate descent per segment
#          and layer on the worst bone gap (cm) over the in-between frames around the segment; writes the chosen
#          {"<a>": {"layers": {layer: seg}}} and the per-segment errors to `out`.
#   apply  "settings": {"<a>": seg | {"layers": {layer: seg}}}, then save the tab to its own file and export the joints
#          to "fbx" (for the --ref readback).
#   probe and fit only run on a throwaway copy (tab path containing "_spacelab").
# seg = {"interp": BEZIER | CLAMPED_BEZIER | LOW_AMPLITUDE_BEZIER | LINEAR, "tangents": Continuous | UserDefined,
#        "rw": right weight of the segment's first key (leaving it), "lw": left weight of its last key (arriving)}.
# Verified on Cascadeur 2026.2.3 (dair v4, 2026-11-03): weights only shape bezier intervals; on a bezier interval the
# knee can swing far outside both keys even with zero weights (the leg is not interpolated as global positions),
# LINEAR keeps it on the chord. That is why the fit picks the interpolation per layer, not only the weights.
import csc, json, time
import numpy as np
t0 = time.time()
S = json.load(open(globals()["SPACING"]))
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == S["tab"]][0]
sm.set_current_scene(tab)
ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer(); lv = ds.layers_viewer()
L = csc.layers.layer


def lname(lid):
    h = lv.layer(lid).header; n = getattr(h, "name", None); return str(n() if callable(n) else n)


LAYERS = {lname(l): l for l in lv.all_layer_ids()}
KEYS = sorted({f for l in LAYERS.values() for f in lv.layer(l).key_frame_indices()})
N = dv.get_animation_size()
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
POS = {}
for b in S.get("bones") or []:
    for d in dv.get_all_data_id(ids[b]):
        if dv.get_data_name(d) == "Position":
            POS[b] = d


def nxt(a):
    later = [k for k in KEYS if k > a]
    return later[0] if later else None


def _set(le, a, lid, seg):
    b = nxt(a)
    if b is None:
        return

    def first(s):
        if "interp" in seg:
            s.interval.interpolation = getattr(L.Interpolation, seg["interp"])
        if "tangents" in seg:
            s.key.tangents = getattr(L.Tangents, seg["tangents"])
        if "rw" in seg:
            s.key.right_weight_velocity = float(seg["rw"])
    le.change_section(a, lid, first)
    if "lw" in seg:
        def last(s):
            if "tangents" in seg:
                s.key.tangents = getattr(L.Tangents, seg["tangents"])
            s.key.left_weight_velocity = float(seg["lw"])
        le.change_section(b, lid, last)


def apply(segs):
    """segs = {"<a>": seg (every layer) | {"layers": {layer name: seg}}}."""
    def mod(model, update, su):
        le = model.layers_editor()
        for a, seg in segs.items():
            per = seg.get("layers") if "layers" in seg else {n: seg for n in LAYERS}
            for n, sg in per.items():
                if n in LAYERS:
                    _set(le, int(a), LAYERS[n], sg)
    return ds.modify_update("casc spacing", mod)


def read(frames=None):
    return {f: {b: [float(x) for x in dv.get_data_value(d, f)] for b, d in POS.items()} for f in (frames if frames is not None else range(N))}


def state():
    return {str(a): {n: {"interp": str(lv.layer(l).interval(a).interpolation).split(".")[-1],
                         "rw": round(float(lv.layer(l).key(a).right_weight_velocity), 4),
                         "lw": round(float(lv.layer(l).key(nxt(a)).left_weight_velocity), 4) if nxt(a) is not None else None}
                     for n, l in LAYERS.items()} for a in KEYS[:-1]}


mode = S["mode"]
if mode in ("probe", "fit"):
    assert "_spacelab" in str(tab.get_path_name()), tab.get_path_name()
if mode == "probe":
    out, oks = {}, []
    for v in S["variants"]:
        oks.append(apply(v["segments"]))
        out[v["name"]] = {f: {b: [round(x, 3) for x in p] for b, p in fr.items()} for f, fr in read().items()}
    json.dump(out, open(S["out"], "w"))
    print(json.dumps({"ok": all(oks), "variants": len(out), "keys": KEYS, "frames": N}))
elif mode == "fit":
    T = {int(f): {b: np.array(p) for b, p in fr.items()} for f, fr in S["target"].items()}
    cands = S["candidates"]
    skip = set(S.get("skip_layers") or [])

    def window(a):
        """In-between frames of the segment starting at a and of its neighbours (a weight on a key shapes both sides)."""
        i = KEYS.index(a)
        lo, hi = KEYS[max(i - 1, 0)], KEYS[min(i + 2, len(KEYS) - 1)]
        return [f for f in range(lo, hi + 1) if f not in KEYS]

    def score(frames, worst=False):
        """Sum over the frames of each frame's worst bone gap: a max over the whole window let the worst segment
        (a knee path no setting can fix) buy a few cm there with large losses on its neighbours (first fit, dair v4)."""
        C = read(frames)
        per = [max(float(np.linalg.norm(np.array(C[f][b]) - T[f][b])) for b in T[f]) for f in frames]
        return (max(per) if worst else sum(per)) if per else 0.0

    chosen = {str(a): {} for a in S["fit"]}
    before = {}
    oks = []
    for p in range(int(S.get("passes", 1))):
        for a in S["fit"]:
            fr = window(a)
            if not fr:
                continue
            if p == 0:
                before[str(a)] = round(score(fr), 2)
            for n in LAYERS:
                if n in skip:
                    continue
                best = None
                for c in cands:
                    oks.append(apply({str(a): {"layers": {n: c}}}))
                    e = score(fr)
                    if best is None or e < best[0] - 1e-3:
                        best = (e, c)
                oks.append(apply({str(a): {"layers": {n: best[1]}}}))
                chosen[str(a)][n] = best[1]
            if time.time() - t0 > float(S.get("budget_s", 500)):
                break
    after = {str(a): round(score(window(a)), 2) for a in S["fit"] if window(a)}
    seg_err = {}
    for a in S["fit"]:
        b = nxt(a)
        fr = [f for f in range(a + 1, b)] if b is not None else []
        seg_err[str(a)] = round(score(fr, worst=True), 2) if fr else 0.0
    json.dump({"settings": {a: {"layers": v} for a, v in chosen.items()}, "window_before": before, "window_after": after,
               "segment_cm": seg_err, "ok": all(oks)}, open(S["out"], "w"), indent=1)
    print(json.dumps({"ok": all(oks), "segment_cm": seg_err, "sec": round(time.time() - t0, 1)}))
else:
    before = read(KEYS)
    ok = apply(S["settings"])
    after = read(KEYS)
    moved = max((float(np.linalg.norm(np.array(after[f][b]) - np.array(before[f][b]))) for f in KEYS for b in before[f]), default=0.0)
    if moved > 0.01:
        raise RuntimeError(f"key poses moved {moved:.3f} cm: not saved (undo with Ctrl+Z)")
    tab.save(str(tab.get_path_name()))
    if S.get("fbx"):
        loader = app.get_tools_manager().get_tool("FbxSceneLoader").get_fbx_loader(tab)
        st = csc.fbx.FbxSettings(); st.bake_animation = True; loader.set_settings(st)
        loader.export_joints(S["fbx"])
    if S.get("out"):
        json.dump({"state": state(), "key_pose_moved_cm": moved}, open(S["out"], "w"), indent=1)
    print(json.dumps({"ok": ok, "keys": KEYS, "key_pose_moved_cm": round(moved, 4), "saved": str(tab.get_path_name())}))
