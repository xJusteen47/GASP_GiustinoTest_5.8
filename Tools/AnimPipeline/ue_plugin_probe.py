# Editor-side, read-only: is CascLiveLink discovered / enabled / mounted, and are its modules loaded?
import unreal, json

def main():
    out = {}
    pbl = unreal.PluginBlueprintLibrary
    out["pbl_api"] = [n for n in dir(pbl) if not n.startswith("_")]
    names = pbl.get_enabled_plugin_names()
    out["enabled_casc"] = [n for n in names if "casc" in str(n).lower()]
    out["enabled_livelink"] = [n for n in names if "livelink" in str(n).lower()]
    for fn in ("is_plugin_mounted", "get_plugin_base_dir", "get_plugin_descriptor_file_path"):
        if hasattr(pbl, fn):
            try:
                out[fn] = str(getattr(pbl, fn)("CascLiveLink"))
            except Exception as e:
                out[fn] = "ERR " + str(e)[:200]
    try:
        ok = unreal.SystemLibrary.execute_console_command(None, "")
    except Exception:
        pass
    out["module_loaded"] = {}
    for m in ("CascLiveLink", "CascLiveLinkInterface", "CascLiveLinkTimeline"):
        try:
            out["module_loaded"][m] = unreal.ModuleManager.is_module_loaded(m) if hasattr(unreal, "ModuleManager") else "n/a"
        except Exception as e:
            out["module_loaded"][m] = "ERR " + str(e)[:100]
    return json.dumps(out)
