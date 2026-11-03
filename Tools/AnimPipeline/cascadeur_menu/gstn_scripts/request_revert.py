# Scripts > GSTN > Annulla ultimo import: asks Claude to run `animpipe casc-revert` (the card goes back to the
# snapshot taken before the last import). Nothing changes in Cascadeur.
import os
from gstn_scripts import _bridge as B


def name():
    return "GSTN.Annulla ultimo import (chiedi a Claude)"


def description():
    return "Chiede a Claude di rimettere la scheda com'era prima dell'ultimo import"


def run(scene):
    cur = B.current(scene)
    if not cur:
        return
    app, tab, move, plan = cur
    card = plan.get("card", "")
    req = B.request(move, "revert", card=card, command=f"python -m animpipe casc-revert {card}")
    scene.success(f"GSTN: richiesta di annullare l'ultimo import di {move} lasciata a Claude ({os.path.basename(req)})")
