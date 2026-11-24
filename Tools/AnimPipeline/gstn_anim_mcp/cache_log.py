"""Log provider prompt-cache usage (DeepSeek: usage.prompt_cache_hit_tokens / prompt_cache_miss_tokens).

The MCP server never calls a provider; the bridge/router that does passes each response's `usage` object here
(`record_usage`) or pipes it to `python cache_log.py record [tag] < usage.json`. `python cache_log.py summary` prints the
hit ratio. Providers that return no cache fields are logged with hit=miss=None so a missing field is visible, not silent.
"""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path
from typing import Any

HIT_KEY = "prompt_cache_hit_tokens"
MISS_KEY = "prompt_cache_miss_tokens"


def default_log_path(root: Path | None = None) -> Path:
    base = root or Path(__file__).resolve().parents[3]
    return base / "Saved" / "AnimPipeline" / "cache_usage.jsonl"


def record_usage(usage: dict[str, Any] | None, tag: str = "", log_path: Path | None = None) -> dict[str, Any]:
    usage = usage or {}
    hit, miss = usage.get(HIT_KEY), usage.get(MISS_KEY)
    entry = {
        "t": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "tag": tag,
        "hit": hit,
        "miss": miss,
        "prompt_tokens": usage.get("prompt_tokens"),
        "completion_tokens": usage.get("completion_tokens"),
    }
    if isinstance(hit, int) and isinstance(miss, int) and hit + miss > 0:
        entry["hit_ratio"] = round(hit / (hit + miss), 4)
    p = log_path or default_log_path()
    p.parent.mkdir(parents=True, exist_ok=True)
    with p.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(entry, ensure_ascii=False) + "\n")
    return entry


def summary(log_path: Path | None = None) -> dict[str, Any]:
    p = log_path or default_log_path()
    hit = miss = calls = no_fields = 0
    if p.exists():
        for line in p.read_text(encoding="utf-8").splitlines():
            try:
                e = json.loads(line)
            except ValueError:
                continue
            calls += 1
            if isinstance(e.get("hit"), int) and isinstance(e.get("miss"), int):
                hit += e["hit"]
                miss += e["miss"]
            else:
                no_fields += 1
    tot = hit + miss
    return {"calls": calls, "no_cache_fields": no_fields, "hit": hit, "miss": miss,
            "hit_ratio": round(hit / tot, 4) if tot else None}


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "record":
        print(json.dumps(record_usage(json.load(sys.stdin), sys.argv[2] if len(sys.argv) > 2 else "")))
    elif len(sys.argv) >= 2 and sys.argv[1] == "summary":
        print(json.dumps(summary()))
    else:
        print("usage: cache_log.py record [tag] < usage.json | summary")
        sys.exit(2)
