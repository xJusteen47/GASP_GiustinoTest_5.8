# Scripts > GSTN > Invia: save the tab, export the joints and leave an import request for `animpipe import <move>`.
import os
from gstn_scripts import _bridge as B


def name():
    return "GSTN.Invia"


def description():
    return "Salva la scena, esporta le ossa e lascia la richiesta di import nella scheda (animpipe import)"


def run(scene):
    cur = B.current(scene)
    if not cur:
        return
    app, tab, move, plan = cur
    fbx, keys = B.save_and_export(app, tab, move)
    owner = B.load_owner_keys(move)["frames"]
    card = plan.get("card", "")
    cmd = f"python -m animpipe import {move}"
    req = B.request(move, "import", card=card, casc=str(tab.get_path_name()), fbx=fbx, keys=keys, owner_keys=owner, command=cmd)
    scene.success(f"GSTN: {move} salvata ed esportata ({len(keys)} chiavi, {len(owner)} segnate tue). "
                  f"Richiesta di import: {os.path.basename(req)}")
