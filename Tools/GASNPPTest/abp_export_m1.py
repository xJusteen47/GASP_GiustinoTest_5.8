import unreal, os
OUT = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "ci", "abp_export")
os.makedirs(OUT, exist_ok=True)
log=[]
try:
    for n in ("BPI_GetAnimInfo","CR_WallIkControlRig","ABP_WallSliding"):
        a = unreal.load_asset("/Game/GiustinoMoves/WallSlide/"+n)
        log.append(f"{n}: {a}")
    abp = unreal.load_asset("/Game/GiustinoMoves/WallSlide/ABP_WallSliding")
    at = unreal.AssetToolsHelpers.get_asset_tools()
    at.export_assets(["/Game/GiustinoMoves/WallSlide/ABP_WallSliding"], OUT)
    log.append("exported " + str(os.listdir(OUT)))
except Exception:
    import traceback; log.append(traceback.format_exc())
open(os.path.join(OUT,"log.txt"),"w").write("\n".join(log))
unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
