"""Ask the Unreal Editor AI assistant the open UE 5.9 adoption questions (open editor only, not headless).
Run via MCP execute_python or `py "<path>"` in the Output Log. Replies go to Saved/UE59/assistant_replies.txt
(written as each one completes, from a slate post-tick callback; takes a few minutes)."""
import os
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT = os.path.join(PROJ, "Saved", "UE59", "assistant_replies.txt")
os.makedirs(os.path.dirname(OUT), exist_ok=True)
CTX = ("Project: UE 5.9, Game Animation Sample based, locomotion in an AnimBlueprint with Motion Matching and Choosers, "
       "Mover + Network Prediction with fixed tick 62.5 Hz, UAF not enabled. ")
QUESTIONS = [
    "Does UUAFNotify_PlayNiagaraEffect fire for sequences played by an AnimBlueprint or montage, or only through UAF? What is the recommended way to spawn footstep and landing dust in 5.9 for an ABP-based character?",
    "In 5.9, which Control Rig nodes are deprecated among AimBone, TwoBoneIKSimple, GetRelativeTransform, SequenceExecution, DebugArc, and what replaces each? Will they stop working in a later version?",
    "Did 5.9 add a platform default frame-rate cap for Windows desktop builds or PIE? Which setting or cvar controls it and what is its default?",
    "With the ModelContextProtocol plugin in 5.9, what is the recommended way to restrict toolsets (ToolsetRegistrySettings BlockedNames vs AllowedNames) and which toolsets are risky to leave enabled?",
    "In 5.9 Pose Search, what does skipping databases by schema role do, and when is it useful for multi-character interactions such as grabs and throws?",
    "Can Python in 5.9 edit Blueprint graphs and StateTree assets (add nodes, states, transitions)? Which classes or toolsets should be used?",
]
_state = {"i": 0, "pending": None, "handle": None}


def _write(text):
    with open(OUT, "a", encoding="utf-8") as f:
        f.write(text + "\n")


def _tick(_dt):
    s = _state
    if s["pending"] is None:
        if s["i"] >= len(QUESTIONS):
            _write("== DONE ==")
            unreal.unregister_slate_post_tick_callback(s["handle"])
            return
        q = QUESTIONS[s["i"]]
        s["pending"] = unreal.AIAssistantInstance.get().send_text_message_and_get_reply("", CTX + q, "")
        _write(f"\n== Q{s['i'] + 1}: {q}")
        return
    r = s["pending"]
    if r.is_complete:
        _write(str(r.value))
        s["pending"] = None
        s["i"] += 1


open(OUT, "w", encoding="utf-8").close()
_state["handle"] = unreal.register_slate_post_tick_callback(_tick)
print(f"asking {len(QUESTIONS)} questions, replies -> {OUT}")
