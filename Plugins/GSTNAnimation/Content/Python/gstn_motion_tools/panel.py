"""Bridge between the C++ panel (GSTNMotionToolsEditor) and the Python operations.

The panel calls `run('<base64 json request>')` as a Python statement and decodes the base64 JSON it returns, so no
quoting or escaping crosses the C++/Python boundary. Errors come back as {"ok": false, "error": "..."} and are shown
in the panel's status line; nothing raises into the editor.

Request: {"op": "...", ...}. Ops: state, overlap, easing, easing_around_playhead, remove, layer, presets.
"""
import base64
import json
import traceback

import gstn_motion_tools as mt
from . import core, presets


def _encode(obj):
    return base64.b64encode(json.dumps(obj, sort_keys=True).encode("utf-8")).decode("ascii")


def _num(req, key):
    v = req.get(key)
    return None if v is None or (isinstance(v, (int, float)) and v < 0) else float(v)


def _presets():
    return {
        "overlap": [{"id": k, "label": p["label"], "hint": p["hint"], "delay": p["delay"], "softness": p["softness"],
                     "bounce": p["bounce"], "mode": p["mode"]} for k, p in presets.OVERLAP_PRESETS.items()],
        "easing": [{"id": k, "label": presets.EASING_LABELS.get(k, k)} for k in core.EASING_PRESETS],
        "chains": [{"id": k, "label": c["label"]} for k, c in presets.CHAINS.items()],
    }


def _state(req):
    import unreal
    from . import sequencer_io
    out = {"ok": True, "op": "state", "presets": _presets()}
    seq = unreal.LevelSequenceEditorBlueprintLibrary.get_current_level_sequence()
    if not seq:
        out.update(sequence=None, message="Apri una Level Sequence con Control Rig nel Sequencer.")
        return out
    b = sequencer_io.UnrealBackend(None, req.get("rig_name"))
    d = mt.api.describe(b)
    out.update(sequence=seq.get_path_name().split(".")[0], chains=d["chains"], layers=d["layers"],
               frames=d["frames"], selection=b.selected_controls(), settings=d["settings"],
               playhead=unreal.LevelSequenceEditorBlueprintLibrary.get_current_time())
    return out


def _key_frames(req):
    """'0, 6, 12' typed in the panel -> [0, 6, 12]; empty -> None (use the base keys)."""
    text = str(req.get("key_frames") or "").replace(";", ",").strip()
    if not text:
        return None
    return [int(float(x)) for x in text.split(",") if x.strip()]


def _chain_args(req):
    if req.get("use_selection"):
        from . import sequencer_io
        sel = sequencer_io.UnrealBackend(None, req.get("rig_name")).selected_controls()
        if not sel:
            raise ValueError("Nessun controllo selezionato nel viewport")
        return {"controls": sel}
    return {"chain": req.get("chain")}


def _dispatch(req):
    op = req.get("op")
    rig = req.get("rig_name")
    if op == "presets":
        return {"ok": True, "op": op, "presets": _presets()}
    if op == "state":
        return _state(req)
    if op == "overlap":
        floor = {"z": float(req["floor_z"])} if req.get("floor") else None
        return mt.overlap(preset=req.get("preset", "leggero"), delay=_num(req, "delay"),
                          softness=_num(req, "softness"), bounce=_num(req, "bounce"), mode=req.get("mode"),
                          weight=float(req.get("weight", 1.0)), protect=req.get("protect", "keys"),
                          floor=floor, key_frames=_key_frames(req), rig_name=rig, **_chain_args(req))
    if op in ("easing", "easing_around_playhead"):
        import unreal
        from . import sequencer_io
        b = sequencer_io.UnrealBackend(None, rig)
        controls = req.get("controls") or b.selected_controls()
        if not controls:
            raise ValueError("Seleziona i controlli da addolcire nel viewport")
        if op == "easing_around_playhead":
            span = b.keys_around(controls, unreal.LevelSequenceEditorBlueprintLibrary.get_current_time())
            if not span:
                raise ValueError("Il cursore non sta tra due chiavi dei controlli selezionati")
            start, end = span
            if end - start < 2:
                raise ValueError("Base con una chiave per frame (clip cotta): scrivi Da/A e usa Applica")
        else:
            start, end = int(req["start"]), int(req["end"])
        return mt.api.apply_easing(b, controls, start, end, req.get("preset", "morbido"),
                                   float(req.get("intensity", 1.0)))
    if op == "remove":
        args = _chain_args(req) if (req.get("chain") or req.get("use_selection")) else {}
        return mt.remove(req.get("layer", "Overlap"), rig_name=rig, **args)
    if op == "layer":
        return mt.layer(req.get("layer", "Overlap"), active=req.get("active"), weight=req.get("weight"),
                        rig_name=rig)
    raise ValueError("Unknown op '%s'" % op)


def run(request_b64):
    try:
        req = json.loads(base64.b64decode(request_b64).decode("utf-8"))
        result = _dispatch(req)
    except Exception as exc:  # the panel shows the message; the trace goes to the Output Log
        try:
            import unreal
            unreal.log_warning("GSTN Motion Tools: %s" % traceback.format_exc())
        except Exception:  # logging must never turn an error reply into a crash
            pass
        result = {"ok": False, "error": str(exc)}
    return _encode(result)
