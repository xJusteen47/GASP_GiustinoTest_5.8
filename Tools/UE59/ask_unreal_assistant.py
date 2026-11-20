"""Ask the Unreal Editor AI assistant (Unreal Assistant, free) from any thread, without screen control.

Needs an open UE 5.9 editor with the built-in MCP server (http://127.0.0.1:8000/mcp). Uses
Tools/AnimPipeline/ue_builtin.py to run Python in the editor: that Python calls
unreal.AIAssistantInstance.send_text_message_and_get_reply and writes the reply to a file from a
slate post-tick callback (the call is async; the editor must keep ticking, so not during a modal dialog).

Usage (prints at most 5 lines; full reply in the --out file):
  python Tools/UE59/ask_unreal_assistant.py "question" [--conv <id>] [--context "hidden context"] [--out file] [--timeout 600]
  python Tools/UE59/ask_unreal_assistant.py --file questions.txt   (one question per blank-line-separated block; each starts a new conversation unless --conv)
Exit 0 = replies written, 2 = editor/MCP not reachable, 3 = timeout.
"""
import argparse, json, os, sys, time, uuid

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "AnimPipeline"))
import ue_builtin  # noqa: E402

PROJECT_CONTEXT = ("Project GASP_Giustino2: UE 5.9, Game Animation Sample based; locomotion in an AnimBlueprint with "
                   "Motion Matching and Choosers; Mover + Network Prediction fixed tick 62.5 Hz; GAS fork; UAF not enabled. "
                   "Answer for this setup, name exact classes, settings and cvars.")

# Runs inside the editor. %(...)r values are filled in by the caller.
_EDITOR = r'''
import json, unreal
_out = %(out)r
_qs = json.loads(%(qs)r)
_ctx = %(ctx)r
_st = {"i": 0, "pending": None, "conv": %(conv)r, "res": [], "h": None}
def _save(done=False):
    with open(_out, "w", encoding="utf-8") as f:
        json.dump({"done": done, "conversation_id": _st["conv"], "replies": _st["res"]}, f, indent=1)
def _tick(_dt):
    s = _st
    try:
        if s["pending"] is None:
            if s["i"] >= len(_qs):
                _save(True); unreal.unregister_slate_post_tick_callback(s["h"]); return
            s["pending"] = unreal.AIAssistantInstance.get().send_text_message_and_get_reply(s["conv"], _qs[s["i"]], _ctx)
            return
        r = s["pending"]
        if r.is_complete:
            s["res"].append({"q": _qs[s["i"]], "a": str(r.value)})
            s["pending"] = None; s["i"] += 1; _save(False)
    except Exception as e:
        s["res"].append({"q": _qs[min(s["i"], len(_qs) - 1)], "error": repr(e)}); _save(True)
        unreal.unregister_slate_post_tick_callback(s["h"])
_save(False)
_st["h"] = unreal.register_slate_post_tick_callback(_tick)
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("question", nargs="?")
    ap.add_argument("--file")
    ap.add_argument("--conv", default="", help="conversation id to continue; empty = new conversation")
    ap.add_argument("--context", default=PROJECT_CONTEXT, help="hidden context appended to each message")
    ap.add_argument("--out", default=os.path.join(os.getcwd(), "Saved", "UE59", f"assistant_{uuid.uuid4().hex[:8]}.json"))
    ap.add_argument("--timeout", type=float, default=600)
    a = ap.parse_args()
    qs = [a.question] if a.question else []
    if a.file:
        qs += [b.strip() for b in open(a.file, encoding="utf-8").read().split("\n\n") if b.strip()]
    if not qs:
        ap.error("give a question or --file")
    if not ue_builtin.available():
        print("ESITO: editor MCP not reachable at " + ue_builtin.URL); sys.exit(2)
    out = os.path.abspath(a.out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    if os.path.exists(out):
        os.remove(out)
    ue_builtin.execute(_EDITOR % {"out": out, "qs": json.dumps(qs), "ctx": a.context, "conv": a.conv})
    t0 = time.time()
    while time.time() - t0 < a.timeout:
        time.sleep(3)
        try:
            d = json.load(open(out, encoding="utf-8"))
        except Exception:
            continue
        if d.get("done"):
            errs = [r for r in d["replies"] if "error" in r]
            print(f"ESITO: {len(d['replies']) - len(errs)}/{len(qs)} replies, conversation {d['conversation_id'] or '(new)'} -> {out}")
            if errs:
                print("first error: " + errs[0]["error"][:200])
            sys.exit(0 if not errs else 1)
    print(f"ESITO: TIMEOUT after {a.timeout:.0f}s, partial replies in {out}"); sys.exit(3)


if __name__ == "__main__":
    main()
