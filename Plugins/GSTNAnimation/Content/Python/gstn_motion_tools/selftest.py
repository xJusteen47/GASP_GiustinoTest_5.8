"""In-editor smoke test of GSTN Motion Tools on a COPY of a real sequence (the source is never opened for writing).

    import gstn_motion_tools.selftest as st; st.run("/Game/AnimPipeline/Pilot/<...>/LS_<move>")

Copies the sequence to /Game/AnimPipeline/Pilot/_MotionToolsTest/<name>_mtselftest (replaced each run is NOT done:
an existing copy makes the test stop, pick another suffix), then checks:
 1. the base section keys are byte-identical after overlap + easing (base never written);
 2. on every protected key frame the visible pose equals the pose with the layers off;
 3. between keys the Overlap layer changes the pose (it does something);
 4. easing keeps the interval's end values;
 5. switching the layer off gives back the base exactly; removing it removes the section.
Writes Saved/GSTNMotionTools/selftest.json and prints at most 5 lines (rule 4).
"""
import json
import os

import unreal

from . import api, sequencer_io

TEST_FOLDER = "/Game/AnimPipeline/Pilot/_MotionToolsTest"


def _base_keys(backend):
    out = {}
    for ch in backend.base_section().get_all_channels():
        try:
            out[str(ch.channel_name)] = [(sequencer_io._key_frame(k), round(k.get_value(), 5)) for k in ch.get_keys()]
        except Exception:
            out[str(ch.channel_name)] = [sequencer_io._key_frame(k) for k in ch.get_keys()]
    return out


def _max_diff(a, b):
    return max(abs(x - y) for pa, pb in zip(a, b) for x, y in zip(pa[0] + pa[1], pb[0] + pb[1]))


def run(sequence_path, suffix="mtselftest", chain=None):
    name = sequence_path.rsplit("/", 1)[1].split(".")[0]
    dst = "%s/%s_%s" % (TEST_FOLDER, name, suffix)
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        raise RuntimeError("Copy already exists: %s (pass another suffix)" % dst)
    unreal.EditorAssetLibrary.duplicate_asset(sequence_path, dst)
    b = sequencer_io.UnrealBackend(dst)
    checks = {}

    keys_before = _base_keys(b)
    chains = api.describe(b)["chains"]
    chain = chain or ("braccio_r" if "braccio_r" in chains else next(iter(chains)))
    r = api.apply_overlap(b, chain=chain, preset="pesante")
    ctrls = r["controls"]
    s, e = b.frame_range()
    frames = list(range(s, e))

    worst_key, moved = 0.0, 0.0
    for c in ctrls:
        key_frames = b.key_frames(c) | {s, e - 1}       # each control is protected on its own keys
        on = b.sample(c, frames, [])
        off = b.sample(c, frames, ["Overlap", "Easing"])
        for i, f in enumerate(frames):
            d = _max_diff([on[i]], [off[i]])
            if f in key_frames:
                worst_key = max(worst_key, d)
            else:
                moved = max(moved, d)
    checks["keys_exact_deg"] = round(worst_key, 4)
    checks["overlap_moves_deg"] = round(moved, 3)

    ks = sorted(b.key_frames(ctrls[0]))
    span = next(((a, c) for a, c in zip(ks, ks[1:]) if c - a >= 3), None)
    if span:
        before = b.sample(ctrls[0], list(span), ["Overlap", "Easing"])
        api.apply_easing(b, [ctrls[0]], span[0], span[1], "scatto")
        after = b.sample(ctrls[0], list(span), ["Overlap"])
        checks["easing_ends_deg"] = round(_max_diff(before, after), 4)

    checks["base_untouched"] = _base_keys(b) == keys_before

    api.set_layer(b, "Overlap", active=False)
    api.set_layer(b, "Easing", active=False) if b.has_layer("Easing") else None
    off_all = b.sample(ctrls[-1], frames, [])
    ref = b.sample(ctrls[-1], frames, ["Overlap", "Easing"])
    checks["layers_off_equals_base_deg"] = round(_max_diff(off_all, ref), 4)
    api.remove(b, "Overlap")
    api.remove(b, "Easing")
    checks["layers_removed"] = not b.has_layer("Overlap") and not b.has_layer("Easing")

    ok = (checks["base_untouched"] and checks["keys_exact_deg"] < 0.01 and checks["overlap_moves_deg"] > 0.5
          and checks.get("easing_ends_deg", 0.0) < 0.01 and checks["layers_off_equals_base_deg"] < 0.01
          and checks["layers_removed"])
    result = {"ok": ok, "copy": dst, "chain": chain, "controls": ctrls, "checks": checks}
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), "GSTNMotionTools")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "selftest.json")
    with open(path, "w") as fh:
        json.dump(result, fh, indent=2, sort_keys=True)
    print("ESITO: %s  chain=%s  copy=%s" % ("PASS" if ok else "FAIL", chain, dst))
    print("checks: %s" % json.dumps(checks, sort_keys=True))
    print("detail: %s" % path)
    return result
