# Scripts > GSTN > Stato: what the bridge knows about the current scene (read only).
import os
from gstn_scripts import _bridge as B


def name():
    return "GSTN.Stato"


def description():
    return "Mostra la mossa, le chiavi della scena, quelle segnate tue e le richieste non ancora gestite"


def run(scene):
    cur = B.current(scene)
    if not cur:
        return
    app, tab, move, plan = cur
    pending = sorted(n for n in os.listdir(B.REQ_DIR) if n.endswith(".json") and f"_{move}_" in n) if os.path.isdir(B.REQ_DIR) else []
    scene.info(f"GSTN: {move} | chiavi scena {B.key_frames(tab)} | tue {B.load_owner_keys(move)['frames']} | "
               f"richieste in attesa {len(pending)}{': ' + ', '.join(pending[-3:]) if pending else ''}")
