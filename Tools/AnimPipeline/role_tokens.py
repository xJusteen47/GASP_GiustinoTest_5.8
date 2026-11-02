"""Tokens per role (owner, 31/10: light models run the structured loop, Opus keeps design and review; measure the saving
against the Opus-only baseline in token_costs.md). Sums `usage` once per assistant message id, per model, over a session's
transcript and its subagents' transcripts, optionally inside a time window, and appends one row per model to
sheets/<take>_roles.tsv. API-equivalent dollars with the per-model prices below (same weighting as token_costs.md).

Usage: python role_tokens.py <session-id> <take> <label> [from-ISO] [to-ISO]
  the transcripts are ~/.claude/projects/<project-folder-slug>/<session-id>.jsonl and
  <session-id>/subagents/*.jsonl"""
import sys, os, json, glob, collections

# default: the Claude project folder whose slug ends with the project name (the slug embeds the user's home path)
PROJ = os.environ.get("CLAUDE_PROJECT_TRANSCRIPTS") or next(iter(sorted(glob.glob(os.path.expanduser("~/.claude/projects/*-GASP-Giustino2")))), "")
# $/MTok: input, cache write (1 h), cache read, output
PRICE = {"opus": (4, 8, 0.20, 20), "sonnet": (3, 6, 0.30, 15), "haiku": (1, 2, 0.10, 5), "fable": (4, 8, 0.20, 20)}

def family(model):
    m = (model or "").lower()
    return next((k for k in PRICE if k in m), "opus")

def sums(paths, t0=None, t1=None):
    U = collections.defaultdict(lambda: collections.Counter()); seen = {}
    for p in paths:
        for line in open(p, encoding="utf-8"):
            try:
                d = json.loads(line)
            except Exception:
                continue
            if d.get("type") != "assistant":
                continue
            t = d.get("timestamp", "")
            if (t0 and t < t0) or (t1 and t > t1):
                continue
            m = d["message"]; mid = m.get("id"); u = m.get("usage") or {}
            key = (p, mid)
            cur = {"in": u.get("input_tokens", 0), "cw": u.get("cache_creation_input_tokens", 0), "cr": u.get("cache_read_input_tokens", 0),
                   "out": u.get("output_tokens", 0)}
            old = seen.get(key)
            fam = ("sub:" if "subagents" in p else "") + family(m.get("model"))
            if old:                                   # streamed parts of one message: keep the max per field
                for k in cur:
                    U[fam][k] += max(cur[k] - old[k], 0)
                seen[key] = {k: max(cur[k], old[k]) for k in cur}
            else:
                for k in cur:
                    U[fam][k] += cur[k]
                U[fam]["calls"] += 1; seen[key] = cur
    return U

if __name__ == "__main__":
    sid, take, label = sys.argv[1:4]
    t0 = sys.argv[4] if len(sys.argv) > 4 else None; t1 = sys.argv[5] if len(sys.argv) > 5 else None
    paths = [os.path.join(PROJ, f"{sid}.jsonl")] + glob.glob(os.path.join(PROJ, sid, "subagents", "*.jsonl"))
    U = sums([p for p in paths if os.path.exists(p)], t0, t1)
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sheets", f"{take}_roles.tsv")
    new = not os.path.exists(out)
    with open(out, "a", encoding="utf-8") as f:
        if new:
            f.write("label\tmodel\tcalls\tinput\tcache_write\tcache_read\toutput\tusd_equiv\twindow\n")
        for fam, c in sorted(U.items()):
            p = PRICE[fam.split(":")[-1]]
            usd = (c["in"] * p[0] + c["cw"] * p[1] + c["cr"] * p[2] + c["out"] * p[3]) / 1e6
            row = f"{label}\t{fam}\t{c['calls']}\t{c['in']}\t{c['cw']}\t{c['cr']}\t{c['out']}\t{usd:.2f}\t{t0 or ''}..{t1 or ''}"
            f.write(row + "\n"); print(row)
