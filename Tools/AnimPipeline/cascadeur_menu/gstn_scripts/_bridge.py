"""Shared helpers for the GSTN entries in Cascadeur's Scripts menu (owner request 2026-11-03: buttons for the bridge).

The commands never run animpipe: they save/export the current tab and write small files in CASC_DIR that Claude reads
(the import into the card, the revert, the interpolation all run on our side). Only scenes made by `animpipe casc-export`
qualify: <CASC_DIR>/<move>.casc with its <move>_export.json next to it. Lab copies (_spacelab, _test...) are refused,
so a button press can never feed a throwaway scene into a card.

Files written (all in CASC_DIR):
  <move>_casc.fbx              joints, baked: what `casc-import` reads by default
  The commands of the GSTN menu: Segna chiave, Invia (+ the status line of the bar). Undo, approve, variants and
  snapshots are in the GSTN Studio app (python -m animpipe.app), which needs no open scene.
  <move>_owner_keys.json       frames the owner marked as his keys {move, frames, marked{frame: stamp}, stamp}
  requests/<stamp>_<move>_<action>.json   one per button press; Claude handles it and moves it to requests/done/
"""
import importlib.util, json, os, time

import csc

# Paths come from Tools/AnimPipeline/animpipe.toml (+ animpipe.local.toml), the same file the commands read.
_TOOLS = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# animpipe/config.py is stdlib only; loaded by path under its own name (no numpy in Cascadeur's Python, no clash with
# another module called "config")
_spec = importlib.util.spec_from_file_location("gstn_animpipe_config", os.path.join(_TOOLS, "animpipe", "config.py"))
_cfg = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_cfg)
CASC_DIR = _cfg.CASC
REQ_DIR = os.path.join(CASC_DIR, "requests")


def stamp():
    return time.strftime("%Y%m%d_%H%M%S")


def current(scene):
    """(app, tab, move, plan) of the current tab, or None after telling the owner why not."""
    app = csc.app.get_application()
    tab = app.get_scene_manager().current_scene()
    path = str(tab.get_path_name() or "").replace("\\", "/")
    base = os.path.basename(path)
    if not path.lower().endswith(".casc") or os.path.normcase(os.path.dirname(os.path.abspath(path))) != os.path.normcase(os.path.abspath(CASC_DIR)):
        scene.error(f"GSTN: questa scena non viene da animpipe casc-export ({base or 'non salvata'}). "
                    f"Apri il file <mossa>.casc in {CASC_DIR}")
        return None
    move = base[:-5]
    plan_path = os.path.join(CASC_DIR, f"{move}_export.json")
    if not os.path.exists(plan_path):
        scene.error(f"GSTN: {base} non è collegata a una scheda (manca {move}_export.json): copia di prova?")
        return None
    with open(plan_path, encoding="utf-8") as f:
        plan = json.load(f)
    return app, tab, move, plan


def key_frames(tab):
    lv = tab.domain_scene().layers_viewer()
    return sorted({int(f) for lid in lv.all_layer_ids() for f in lv.layer(lid).key_frame_indices()})


def owner_keys_path(move):
    return os.path.join(CASC_DIR, f"{move}_owner_keys.json")


def load_owner_keys(move):
    p = owner_keys_path(move)
    if os.path.exists(p):
        with open(p, encoding="utf-8") as f:
            return json.load(f)
    return {"move": move, "frames": [], "marked": {}}


def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    os.replace(tmp, path)


def request(move, action, **extra):
    """Writes requests/<stamp>_<move>_<action>.json and returns its path."""
    s = stamp()
    data = {"action": action, "move": move, "stamp": s, **extra}
    p = os.path.join(REQ_DIR, f"{s}_{move}_{action}.json")
    write_json(p, data)
    return p


def save_and_export(app, tab, move):
    """Saves the tab on its own file and exports the joints, baked, to <move>_casc.fbx. Returns (fbx, keys)."""
    path = str(tab.get_path_name()).replace("\\", "/")
    tab.save(path)
    loader = app.get_tools_manager().get_tool("FbxSceneLoader").get_fbx_loader(tab)
    st = csc.fbx.FbxSettings(); st.bake_animation = True; loader.set_settings(st)
    fbx = os.path.join(CASC_DIR, f"{move}_casc.fbx").replace("\\", "/")
    loader.export_joints(fbx)
    return fbx, key_frames(tab)


def status_text():
    """One line for the bar, never an error popup: move, owner keys, waiting requests (refreshed every 2 s)."""
    try:
        tab = csc.app.get_application().get_scene_manager().current_scene()
        path = str(tab.get_path_name() or "").replace("\\", "/")
        base = os.path.basename(path)
        if not base.lower().endswith(".casc") or os.path.normcase(os.path.dirname(os.path.abspath(path))) != os.path.normcase(os.path.abspath(CASC_DIR)):
            return "scena non collegata"
        move = base[:-5]
        if not os.path.exists(os.path.join(CASC_DIR, f"{move}_export.json")):
            return f"{move}: non collegata a una scheda"
        pend = [n for n in os.listdir(REQ_DIR) if n.endswith(".json") and f"_{move}_" in n] if os.path.isdir(REQ_DIR) else []
        keys = load_owner_keys(move)["frames"]
        return f"{move} | chiavi tue {','.join(map(str, keys)) or '-'} | " + (f"{len(pend)} invio in attesa" if pend else "nessun invio in attesa")
    except Exception as e:          # the bar must never break Cascadeur
        return f"stato n/d ({type(e).__name__})"
