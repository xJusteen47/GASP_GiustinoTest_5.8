"""Compact MCP replies: at most MAX_LINES lines of text, full result written to a file whose path is returned.

Why: every tool reply is re-read by the model on each following turn; indented JSON of gates/metrics costs thousands of
tokens for a decision that needs a verdict and a few numbers. The file keeps the full detail for the cases that need it.
"""
from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any

MAX_LINES = 5


def _flat(v: Any) -> str:
    if isinstance(v, float):
        return f"{v:g}"
    if isinstance(v, (dict, list)):
        return json.dumps(v, separators=(",", ":"), ensure_ascii=False)
    return str(v)


def _clip(s: str, n: int = 160) -> str:
    return s if len(s) <= n else s[: n - 3] + "..."


def summary_lines(tool: str, res: dict[str, Any]) -> list[str]:
    """Verdict first, then at most MAX_LINES-2 short facts picked per tool."""
    ok = bool(res.get("ok", True))
    head = f"{'OK' if ok else 'FAIL'} {tool}"
    if res.get("dry_run"):
        head += " (dry_run)"
    if not ok and res.get("error"):
        return [head, _clip(str(res["error"]), 300)]
    if res.get("summary"):
        return [head] + [str(x) for x in res["summary"] if not str(x).startswith("details")]
    facts: list[str] = []
    if "gates" in res:
        facts.append(f"gates={res['gates'].get('status')}")
    if "conservation" in res:
        c = res["conservation"]
        facts.append(f"conservation={c.get('status')}" + (f" fails={len(c['failures'])}" if c.get("failures") else ""))
    if "verdict" in res:
        facts.append(f"verdict={res['verdict']}")
    if "plan" in res and isinstance(res["plan"], dict) and "params" in res["plan"]:
        p = res["plan"]
        facts.append(f"frames={p.get('frames_touched')} params={len(p['params'])} bones={len(p.get('bones', []))}")
    for k in ("src", "dst", "asset", "returncode", "variant_id", "restored"):
        if k in res and res[k] not in (None, ""):
            facts.append(f"{k}={_flat(res[k])}")
    lines = [head]
    if facts:
        lines.append(_clip(" ".join(facts)))
    return lines


def compact_reply(tool: str, res: dict[str, Any], out_dir: Path | None) -> str:
    """Return the compact text; write the full result to out_dir (when given) and end with its path."""
    lines = summary_lines(tool, res)[: MAX_LINES - 1]
    path_txt = "details: (not saved)"
    if out_dir is not None:
        try:
            out_dir.mkdir(parents=True, exist_ok=True)
            p = out_dir / f"{time.strftime('%Y%m%dT%H%M%S')}_{tool}.json"
            n = 1
            while p.exists():
                p = out_dir / f"{time.strftime('%Y%m%dT%H%M%S')}_{tool}_{n}.json"
                n += 1
            p.write_text(json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
            path_txt = f"details: {p}"
        except OSError as exc:
            path_txt = f"details: write failed ({exc})"
    lines.append(path_txt)
    return "\n".join(lines)
