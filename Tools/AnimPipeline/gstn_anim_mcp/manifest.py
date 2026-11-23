"""Per-asset manifest (Saved/AnimPipeline/manifest/<asset>.json): the bookkeeping that makes cheap agents waste less.

Holds: intent hashes of dry runs, the last run_intent/gate report, attempts per objective (rule: max 2), the capture
cache index (base captures are reused), per-step minutes + provider usage, and produces the costi.csv line.
Pure stdlib, deterministic; every function takes the repo root so tests use a temp dir.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path
from typing import Any

MAX_ATTEMPTS = 2
COSTI_COLUMNS = "data,thread,obiettivo,macchina,provider,modello,minuti_desktop,token_review,usd_stimati,quota_pro_pct,esito"
_SRC_EXT = (".cpp", ".h", ".hpp", ".inl", ".cs")


def _name(asset: str) -> str:
    return asset.rsplit("/", 1)[-1]


def manifest_path(root: Path, asset: str) -> Path:
    return Path(root) / "Saved" / "AnimPipeline" / "manifest" / f"{_name(asset)}.json"


def intent_hash(src: str, dst: str, intent: dict[str, Any]) -> str:
    """Stable 16-hex hash of (src, dst, validated intent): key of the dry_run / report / capture bookkeeping."""
    blob = json.dumps({"src": _name(src), "dst": _name(dst), "intent": intent}, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(blob.encode("utf-8")).hexdigest()[:16]


def load(root: Path, asset: str) -> dict[str, Any]:
    p = manifest_path(root, asset)
    if p.exists():
        try:
            m = json.loads(p.read_text(encoding="utf-8"))
            if isinstance(m, dict):
                for k, v in (("dry_runs", []), ("attempts", {}), ("captures", {}), ("intents_captured", {}), ("steps", [])):
                    m.setdefault(k, v)
                m.setdefault("last_report", None)
                return m
        except (OSError, ValueError):
            pass
    return {"asset": _name(asset), "dry_runs": [], "last_report": None, "attempts": {}, "captures": {},
            "intents_captured": {}, "steps": []}


def save(root: Path, asset: str, m: dict[str, Any]) -> Path:
    p = manifest_path(root, asset)
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(m, indent=2, sort_keys=True), encoding="utf-8")
    return p


def record_dry_run(root: Path, asset: str, ihash: str) -> None:
    m = load(root, asset)
    if ihash not in m["dry_runs"]:
        m["dry_runs"].append(ihash)
    save(root, asset, m)


def has_dry_run(root: Path, asset: str, ihash: str) -> bool:
    return ihash in load(root, asset).get("dry_runs", [])


def record_report(root: Path, asset: str, ihash: str, res: dict[str, Any]) -> dict[str, Any]:
    """Store the verdict of a real anim_run_intent: follow-through ok, conservation ok, gates ok (None = not applicable)."""
    cons = res.get("conservation")
    gates = res.get("gates")
    rep = {
        "intent_hash": ihash,
        "verdict": res.get("verdict") or ("PASS" if res.get("ok") else "FAIL"),
        "ok": bool(res.get("ok")),
        "follow_through_ok": bool(res.get("ok")) if res.get("links") is not None else None,
        "conservation_ok": (cons.get("status") in ("OK", "PASS", None) if isinstance(cons, dict) else None),
        "gates_ok": (gates.get("status") in ("OK", "PASS", "SKIPPED", None) if isinstance(gates, dict) else None),
        "t": time.strftime("%Y-%m-%dT%H:%M:%S"),
    }
    m = load(root, asset)
    m["last_report"] = rep
    save(root, asset, m)
    return rep


def report_is_pass(rep: dict[str, Any] | None, ihash: str | None = None) -> tuple[bool, str]:
    if not rep:
        return False, "no run_intent report for this variant: run anim_run_intent first"
    if ihash and rep.get("intent_hash") != ihash:
        return False, f"last report is for intent {rep.get('intent_hash')}, not {ihash}: rerun anim_run_intent"
    bad = [k for k in ("follow_through_ok", "conservation_ok", "gates_ok") if rep.get(k) is False]
    if rep.get("verdict") != "PASS" or not rep.get("ok") or bad:
        return False, f"last report is {rep.get('verdict')} (failed: {', '.join(bad) or 'verdict'}): rollback or fix"
    return True, "ok"


def attempts(root: Path, asset: str, objective: str) -> int:
    return int(load(root, asset).get("attempts", {}).get(objective, 0))


def bump_attempt(root: Path, asset: str, objective: str) -> int:
    m = load(root, asset)
    m["attempts"][objective] = int(m["attempts"].get(objective, 0)) + 1
    save(root, asset, m)
    return m["attempts"][objective]


def record_step(root: Path, asset: str, step: str, minutes: float = 0.0, usage: dict[str, Any] | None = None,
                cache_log_path: Path | None = None, tag: str = "") -> dict[str, Any]:
    """Append a step (desktop minutes, provider usage) to the manifest; usage also goes to cache_log's jsonl."""
    entry: dict[str, Any] = {"step": step, "minutes": round(float(minutes), 2), "t": time.strftime("%Y-%m-%dT%H:%M:%S")}
    if usage:
        entry["usage"] = {k: usage.get(k) for k in ("prompt_tokens", "completion_tokens", "prompt_cache_hit_tokens",
                                                   "prompt_cache_miss_tokens") if usage.get(k) is not None}
        try:
            import cache_log
            cache_log.record_usage(usage, tag or f"{_name(asset)}:{step}",
                                   cache_log_path or cache_log.default_log_path(Path(root)))
        except Exception:  # noqa: BLE001 - logging must never break a step
            pass
    m = load(root, asset)
    m["steps"].append(entry)
    save(root, asset, m)
    return entry


def costi_line(root: Path, asset: str, thread: str, objective: str, model: str, esito: str, machine: str = "desktop",
               provider: str = "claude_max", usd: float | str = "", quota_pct: float | str = "",
               date: str | None = None) -> str:
    """One ready costi.csv row (columns: COSTI_COLUMNS). Not written anywhere: the coordinator appends it."""
    steps = load(root, asset).get("steps", [])
    minutes = round(sum(float(s.get("minutes", 0)) for s in steps), 1)
    tokens = sum(int((s.get("usage") or {}).get("prompt_tokens") or 0) + int((s.get("usage") or {}).get("completion_tokens") or 0)
                 for s in steps)

    def q(v: Any) -> str:
        s = str(v)
        return '"' + s.replace('"', '""') + '"' if any(c in s for c in ',"\n') else s

    row = [date or time.strftime("%Y-%m-%d"), thread, objective, machine, provider, model, minutes, tokens, usd, quota_pct, esito]
    return ",".join(q(v) for v in row)


def _source_files(root: Path) -> list[Path]:
    root = Path(root)
    dirs = [root / "Source"]
    if (root / "Plugins").exists():
        dirs += sorted((root / "Plugins").glob("*/Source"))
    out: list[Path] = []
    for d in dirs:
        if d.exists():
            out += sorted(p for p in d.rglob("*") if p.is_file() and p.suffix in _SRC_EXT)
    return out


def build_id(root: Path) -> str:
    """Hash of the (path, size, mtime) list of Source/ and Plugins/*/Source files: changes when C++ changes (capture cache key)."""
    h = hashlib.sha256()
    for f in _source_files(root):
        st = f.stat()
        h.update(f"{f.relative_to(root).as_posix()}|{st.st_size}|{st.st_mtime_ns}\n".encode())
    return h.hexdigest()[:16]


def cpp_source_hash(root: Path) -> str:
    """Content hash of the C++ sources (stable across checkouts, unlike mtimes): used by needs_build."""
    h = hashlib.sha256()
    for f in _source_files(root):
        h.update(f.relative_to(root).as_posix().encode())
        h.update(hashlib.sha256(f.read_bytes()).digest())
    return h.hexdigest()


def needs_build(root: Path, stamp: Path | None = None) -> dict[str, Any]:
    """needs_build True when the C++ sources differ from the last build stamp (Saved/AnimPipeline/build_stamp.txt)."""
    stamp = stamp or Path(root) / "Saved" / "AnimPipeline" / "build_stamp.txt"
    cur = cpp_source_hash(root)
    last = stamp.read_text(encoding="utf-8").strip() if stamp.exists() else ""
    return {"needs_build": cur != last, "hash": cur, "stamp": str(stamp)}


def write_build_stamp(root: Path, stamp: Path | None = None) -> str:
    """Call after a successful build."""
    stamp = stamp or Path(root) / "Saved" / "AnimPipeline" / "build_stamp.txt"
    stamp.parent.mkdir(parents=True, exist_ok=True)
    h = cpp_source_hash(root)
    stamp.write_text(h, encoding="utf-8")
    return h


def capture_key(clip_shas: list[str], sequences: list[str], cvars: str, views: str, build: str, extra: str = "") -> str:
    blob = json.dumps({"clips": sorted(clip_shas), "seq": list(sequences), "cvars": cvars, "views": views,
                       "build": build, "extra": extra}, sort_keys=True)
    return hashlib.sha256(blob.encode("utf-8")).hexdigest()
