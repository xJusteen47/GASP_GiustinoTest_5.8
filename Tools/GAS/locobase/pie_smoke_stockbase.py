"""loco-base-1 PIE smoke test: Play with gstn.Loco.Base=LOCO_BASE (default 1) for ~10 s, then write which anim class each
skeletal mesh runs and quit. Run through Tools/CI/Run-Timed.ps1 -Preset Capture -Script Tools/GAS/locobase/pie_smoke_stockbase.py
(windowed editor: take the _ue59 lock). Report: Saved/LocoBase/pie_smoke.txt ; ESITO line in it.
"""
import os
import runpy
import time
import unreal

OUT = unreal.Paths.project_saved_dir() + "LocoBase/pie_smoke.txt"
BASE = os.environ.get("LOCO_BASE", "1")
HERE = os.path.dirname(os.path.abspath(__file__))
try:
    runpy.run_path(os.path.join(HERE, "..", "..", "GASNPPTest", "vd2_mover.py"), run_name="vd2_mover")
except Exception:
    pass

st = {"t0": None, "phase": "wait_editor", "h": None, "log": []}

def finish():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("\n".join(st["log"]) + "\n")
    unreal.unregister_slate_post_tick_callback(st["h"])
    unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")

def tick(_dt):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    now = time.time()
    if st["phase"] == "wait_editor":
        st["t0"] = st["t0"] or now
        if now - st["t0"] > 20:
            unreal.SystemLibrary.execute_console_command(None, "gstn.Loco.Base " + BASE)
            les.editor_request_begin_play()
            st["phase"], st["t0"] = "playing", now
    elif st["phase"] == "playing" and now - st["t0"] > 15:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        # 0 = whatever the pawn/skin sets (in L_Test_Hub the kit skin sets ABP_GSTN_Locomotion), never the stock copy
        stock = "ABP_GSTN_StockMover_C"
        found = []
        if world:
            for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Pawn):
                for m in a.get_components_by_class(unreal.SkeletalMeshComponent):
                    inst = m.get_anim_instance()
                    found.append("%s %s %s" % (a.get_name(), m.get_name(), inst.get_class().get_name() if inst else None))
        st["log"].append("gstn.Loco.Base=%s world=%s" % (BASE, world.get_name() if world else None))
        st["log"].extend(found)
        ok = bool(found) and (all(stock in f for f in found) if BASE == "1" else not any(stock in f for f in found))
        st["log"].append("ESITO: " + ("OK" if ok else "FAIL (base %s, see classes above)" % BASE))
        les.editor_request_end_play()
        st["phase"], st["t0"] = "ending", now
    elif st["phase"] == "ending" and now - st["t0"] > 3:
        finish()

st["h"] = unreal.register_slate_post_tick_callback(tick)
