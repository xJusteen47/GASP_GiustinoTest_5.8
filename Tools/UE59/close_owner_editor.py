"""Guarded close of the open editor, called by Close-OwnerEditor.ps1 (see there). Prints one ESITO line.
Exit: 0 no editor, 2 PIE running, 3 dirty packages left after saving, 1 error, 10 quit requested (the .ps1 waits for the exit)."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "AnimPipeline"))
import ue_builtin  # noqa: E402

CHECK = r'''
import json, unreal
def _pie():
    try:
        return bool(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world())
    except Exception:
        return bool(unreal.EditorLevelLibrary.get_game_world())
res = {"pie": _pie()}
if not res["pie"]:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    left = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    left += [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    res["dirty"] = left
    if not left:
        unreal.SystemLibrary.quit_editor()
json.dumps(res)
'''

try:
    if not ue_builtin.available():
        print("ESITO: NESSUN EDITOR (MCP non raggiungibile, niente da chiudere)")
        sys.exit(0)
    r = ue_builtin.execute(CHECK, timeout=300)
except SystemExit:
    raise
except Exception as e:  # the quit can drop the connection before the reply: treat as requested
    if "quit" in str(e).lower() or "Connection" in type(e).__name__ or "Remote" in str(e):
        print("ESITO: CHIUSURA RICHIESTA (connessione chiusa dall'editor)")
        sys.exit(10)
    print("ESITO: ERRORE " + str(e)[:200])
    sys.exit(1)
if r.get("pie"):
    print("ESITO: RIFIUTATO PIE (l'owner sta giocando, niente toccato)")
    sys.exit(2)
if r.get("dirty"):
    print("ESITO: RIFIUTATO SALVATAGGIO, ancora sporchi: " + ", ".join(r["dirty"][:5]))
    sys.exit(3)
print("ESITO: SALVATO TUTTO, chiusura richiesta")
sys.exit(10)
