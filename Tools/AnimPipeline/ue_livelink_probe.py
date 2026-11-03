# Editor-side, read-only: what the CascLiveLink plugin and LiveLink expose to Python.
import unreal, json

def main():
    out = {}
    out["casc_types"] = sorted(n for n in dir(unreal) if "casc" in n.lower() and "cascade" not in n.lower() or n.lower().startswith("casclive"))
    out["livelink_types"] = sorted(n for n in dir(unreal) if n.startswith("LiveLink"))[:80]
    for t in out["casc_types"]:
        try:
            out["api_" + t] = [n for n in dir(getattr(unreal, t)) if not n.startswith("_")][:60]
        except Exception as e:
            out["api_" + t] = str(e)[:100]
    lbl = getattr(unreal, "LiveLinkBlueprintLibrary", None)
    out["lbl_api"] = [n for n in dir(lbl) if not n.startswith("_") and n[0].islower()][:80] if lbl else None
    try:
        out["subjects"] = [str(s) for s in unreal.LiveLinkBlueprintLibrary.get_live_link_subjects(True, True)]
    except Exception as e:
        out["subjects"] = "ERR " + str(e)[:150]
    return json.dumps(out)
