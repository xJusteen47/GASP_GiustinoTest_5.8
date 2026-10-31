"""Token usage per work block from Claude Code transcripts (usage per unique assistant message id, max per field)."""
import json, sys, collections
p = sys.argv[1]
RULES = [  # (block, markers) in priority order; only applied after 17:50Z
    ("animationsight test", ["animationsight"]),
    ("side tests: tangents, unbaking, AutoPhysics", ["autophys", "Auto Physics", "unbake", "tangent_test", "ue_tangent", "ue_subframe", "ue_perf"]),
    ("documentation (handoff, README, memory, skill delta)", ["README", "handoff", "memory\\\\", "memory/", "jab_log", "ganon_ftilt_log", "patch_handoff", "SKILL", "docstring"]),
    ("Ganondorf v2: new checks, idle, model, first pass", ["ganon_v2", "idle_points", "balance_check", "pose_rules", "make_ganon_v2", "ue_find_idle", "ue_export_idle"]),
    ("Ganondorf v1 (incl. sparse keys)", ["ganon_ftilt_motion", "sparse_apply", "GanondorfFTilt", "ganon_v1", "ganon_page", "ganon_iter", "fingers_fist"]),
    ("jab v11", ["v11"]),
]
def classify(t, s, prev):
    if t < "2026-10-31T13:20": return "jab v1-v7 + setup and research (night 30-31/10)"
    if t < "2026-10-31T17:50": return "jab v7-v10 + checks, refs, MCP Plus (day)"
    if t >= "2026-10-31T20:55": return "Q&A estimates (now)"
    for name, mk in RULES:
        if any(m in s for m in mk): return name
    return prev
U = {}; order = []; cls_of = {}; imgs = collections.Counter(); owner = collections.Counter(); prev = None
for line in open(p, encoding="utf-8"):
    try: d = json.loads(line)
    except: continue
    t = d.get("timestamp", "")
    if d.get("type") == "assistant":
        m = d["message"]; mid = m.get("id"); u = m.get("usage") or {}
        s = json.dumps([c.get("input", "") for c in m.get("content", []) if c.get("type") == "tool_use"])
        c = classify(t, s, prev) if mid not in cls_of else cls_of[mid]
        if prev and c != prev and s == "[]": c = prev
        cls_of[mid] = c; prev = c
        r = U.setdefault(mid, [0, 0, 0, 0, c])
        r[4] = c
        for i, f in enumerate(("input_tokens", "cache_creation_input_tokens", "cache_read_input_tokens", "output_tokens")):
            r[i] = max(r[i], u.get(f, 0) or 0)
    elif d.get("type") == "user":
        c = d.get("message", {}).get("content")
        if isinstance(c, list):
            for x in c:
                if isinstance(x, dict) and x.get("type") == "tool_result" and isinstance(x.get("content"), list):
                    imgs[prev] += sum(1 for y in x["content"] if isinstance(y, dict) and y.get("type") == "image")
                elif isinstance(x, dict) and x.get("type") == "image":
                    imgs[prev] += 1
            txt = " ".join(x.get("text", "") for x in c if isinstance(x, dict) and x.get("type") == "text")
        else:
            txt = c or ""
        if 'from="human"' in txt or "trust=\"principal\"" in txt:
            owner[prev if t < "2026-10-31T20:55" else "Q&A estimates (now)"] += 1
B = collections.OrderedDict()
for mid, r in U.items():
    b = B.setdefault(r[4], [0, 0, 0, 0, 0])
    for i in range(4): b[i] += r[i]
    b[4] += 1
print("block | API calls | owner msgs | images | input | cache write | cache read | output | avg context per call")
T = [0] * 7
for k, b in B.items():
    ctx = (b[0] + b[1] + b[2]) / max(b[4], 1)
    print(f"{k} | {b[4]} | {owner[k]} | {imgs[k]} | {b[0]} | {b[1]} | {b[2]} | {b[3]} | {ctx/1000:.0f}k")
    T = [a + x for a, x in zip(T, [b[4], owner[k], imgs[k], b[0], b[1], b[2], b[3]])]
print("TOTAL |", " | ".join(str(x) for x in T))
