# Scripts > GSTN > Segna chiave: the current frame becomes (or stops being) one of the owner's keys. Claude's
# interpolation never moves these frames. Only a file is written: the scene is not touched.
from gstn_scripts import _bridge as B


def name():
    return "GSTN.Segna chiave (frame corrente)"


def description():
    return "Segna o toglie il frame corrente come chiave tua: Claude interpola tra queste e non le tocca"


def run(scene):
    cur = B.current(scene)
    if not cur:
        return
    app, tab, move, plan = cur
    f = int(tab.domain_scene().get_current_frame())
    ok = B.load_owner_keys(move)
    frames = set(ok.get("frames", [])); marked = ok.setdefault("marked", {})
    if f in frames:
        frames.discard(f); marked.pop(str(f), None); verb = "tolta"
    else:
        frames.add(f); marked[str(f)] = B.stamp(); verb = "segnata"
    ok["frames"] = sorted(frames); ok["stamp"] = B.stamp(); ok["move"] = move
    B.write_json(B.owner_keys_path(move), ok)
    msg = f"GSTN: chiave {f} {verb}. Chiavi tue su {move}: {ok['frames'] or 'nessuna'}"
    if verb == "segnata" and f not in B.key_frames(tab):
        scene.warning(msg + f" (attenzione: il frame {f} non ha una chiave nella scena)")
    else:
        scene.success(msg)
