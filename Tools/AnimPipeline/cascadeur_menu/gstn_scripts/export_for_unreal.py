# Scripts > GSTN > Esporta per Unreal: save the tab, export the joints for `animpipe casc-import`, ask Claude to import.
import os
from gstn_scripts import _bridge as B


def name():
    return "GSTN.Esporta per Unreal"


def description():
    return "Salva la scena, esporta le ossa in <mossa>_casc.fbx e lascia a Claude la richiesta di import nella scheda"


def run(scene):
    cur = B.current(scene)
    if not cur:
        return
    app, tab, move, plan = cur
    fbx, keys = B.save_and_export(app, tab, move)
    owner = B.load_owner_keys(move)["frames"]
    card = plan.get("card", "")
    cmd = f"python -m animpipe casc-import {card} --fbx=\"{fbx}\" --keys={','.join(map(str, keys))}"
    req = B.request(move, "import", card=card, casc=str(tab.get_path_name()), fbx=fbx, keys=keys, owner_keys=owner, command=cmd)
    scene.success(f"GSTN: {move} salvata ed esportata ({len(keys)} chiavi, {len(owner)} segnate tue). "
                  f"Richiesta per Claude: {os.path.basename(req)}")
