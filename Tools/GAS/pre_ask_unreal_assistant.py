"""Ask the Unreal Assistant from inside a capture/boot editor (no MCP needed): a `-Pre` module for vfx_v3_capture.ps1.

Same in-editor code as Tools/UE59/ask_unreal_assistant.py (slate post-tick callback, the assistant call is async), but started from the
boot script, so the replies arrive while the capture runs and cost no extra editor session. Questions: env ASK_FILE (blocks separated
by a blank line), replies: env ASK_OUT (json, "done": true when all are in). Nothing happens if ASK_FILE is unset.
"""
import json
import os

import unreal

CONTEXT = ("Project GASP_Giustino2: UE 5.9, Game Animation Sample based; Niagara CPU sprites; GASP hub lighting (directional + skylight, "
           "auto exposure). Answer for this setup, name exact classes, settings and cvars.")


def run():
    qfile, out = os.environ.get("ASK_FILE", ""), os.environ.get("ASK_OUT", "")
    if not qfile or not out or not os.path.exists(qfile):
        return "no ASK_FILE"
    qs = [b.strip() for b in open(qfile, encoding="utf-8").read().split("\n\n") if b.strip()]
    st = {"i": 0, "pending": None, "res": [], "h": None}

    def save(done=False):
        with open(out, "w", encoding="utf-8") as f:
            json.dump({"done": done, "replies": st["res"]}, f, indent=1)

    def tick(_dt):
        try:
            if st["pending"] is None:
                if st["i"] >= len(qs):
                    save(True)
                    unreal.unregister_slate_post_tick_callback(st["h"])
                    return
                st["pending"] = unreal.AIAssistantInstance.get().send_text_message_and_get_reply("", qs[st["i"]], CONTEXT)
                return
            if st["pending"].is_complete:
                st["res"].append({"q": qs[st["i"]], "a": str(st["pending"].value)})
                st["pending"] = None
                st["i"] += 1
                save(False)
        except Exception as e:
            st["res"].append({"q": qs[min(st["i"], len(qs) - 1)], "error": repr(e)})
            save(True)
            unreal.unregister_slate_post_tick_callback(st["h"])

    save(False)
    st["h"] = unreal.register_slate_post_tick_callback(tick)
    return f"{len(qs)} questions queued -> {out}"
