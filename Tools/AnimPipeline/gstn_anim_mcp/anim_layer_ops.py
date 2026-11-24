"""`anim_layer` tool: a stack of pose layers per variant clip, with undo for every step (math in anim_layer.py).

The stack file is the SOURCE of the variant: dst = src + layers, in order. It lives in
Tools/AnimPipeline/cards/layers/<dst>.json (versioned with the thread's commit, like a card) and is never edited by hand.
Actions:
  add    validate one layer, measure it offline on the src dump (if there is one), append it (dry_run: measure only)
  undo   remove the last layer (or every layer after `to`); the next build rebuilds dst from src without it
  list   the stack, plus whether dst was built from the current stack
  dump   (UE, once per src clip) write the src local transforms, so add/undo measure joints in cm offline
  build  (UE) snapshot dst, rebuild it from src + the whole stack, conservation on the data -> PASS/FAIL
Never: dst == src, dst that is an approved clip (docs/notes/anim-approvate.md), keys added to a card.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import anim_layer as L
import anim_ops

UE59 = r"C:\Users\jacop\Documents\Unreal Projects\GASP_Giustino2_ue59"


def _name(asset: str) -> str:
    return asset.rsplit("/", 1)[-1].split(".")[0]


def stack_path(repo: Path, dst: str) -> Path:
    return repo / "Tools" / "AnimPipeline" / "cards" / "layers" / f"{_name(dst)}.json"


def dump_path(repo: Path, src: str) -> Path:
    return repo / "Saved" / "AnimPipeline" / "layers" / f"locals_{_name(src)}.json"


def _load_stack(repo: Path, dst: str) -> dict[str, Any]:
    p = stack_path(repo, dst)
    if p.exists():
        return json.loads(p.read_text(encoding="utf-8"))
    return {"dst": _name(dst), "src": None, "layers": [], "built_hash": None, "history": []}


def _save_stack(repo: Path, st: dict[str, Any]) -> Path:
    p = stack_path(repo, st["dst"])
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(st, indent=1) + "\n", encoding="utf-8")
    return p


def stack_hash(st: dict[str, Any]) -> str:
    return anim_ops._manifest().intent_hash(st["src"] or "", st["dst"], {"layers": [e["layer"] for e in st["layers"]]})


def _approved_names(repo: Path) -> set[str]:
    p = repo / "docs" / "notes" / "anim-approvate.md"
    if not p.exists():
        return set()
    return set(re.findall(r"\b(A[SM]_[A-Za-z0-9_]+)", p.read_text(encoding="utf-8")))


def _guard(repo: Path, src: str, dst: str) -> None:
    if _name(src) == _name(dst):
        raise ValueError("dst must be a NEW variant clip: layers never overwrite their src")
    if _name(dst) in _approved_names(repo):
        raise ValueError(f"{_name(dst)} is an approved clip (docs/notes/anim-approvate.md): it is locked, layer a new variant")


def _load_dump(repo: Path, src: str) -> tuple[int, dict] | None:
    for p in (dump_path(repo, src), dump_path(Path(UE59), src)):
        try:
            if p.exists():
                return L.clip_from_dump(json.loads(p.read_text(encoding="utf-8")))
        except (OSError, ValueError):
            continue
    return None


def _measure(repo: Path, src: str, layers_raw: list[dict], n_hint: int | None) -> dict[str, Any]:
    """Offline report of the whole stack: plan only without a dump, measured joints in cm with one."""
    d = _load_dump(repo, src)
    if d is None:
        norm = [L.validate_layer(x, n_hint) for x in layers_raw]
        return {"measured": False, **L.plan(norm, n_hint)}
    n, local = d
    norm = [L.validate_layer(x, n) for x in layers_raw]
    new, rep = L.apply_stack(n, local, norm)
    rep["conservation"] = L.conservation(n, local, new, norm)
    rep["measured"] = True
    rep["n_frames"] = n
    return rep


def _event(st: dict[str, Any], what: str, **kw: Any) -> None:
    st["history"].append({"at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"), "do": what, **kw})
    st["history"] = st["history"][-200:]


def anim_layer(action: str = "add", dst: str = "", src: str = "", layer: dict | None = None, dry_run: bool = False,
               to: int | None = None, root: str | Path | None = None, worktree_dir: str | None = None,
               ue59_dir: str = UE59) -> dict[str, Any]:
    repo = anim_ops._repo_root(root)
    if not dst:
        raise ValueError("dst (the variant clip the stack builds) is required")
    st = _load_stack(repo, dst)
    if src:
        if st["src"] and _name(src) != st["src"]:
            raise ValueError(f"stack of {st['dst']} is built on {st['src']}, not {_name(src)}: start a new dst for another base")
    src_n = st["src"] or (_name(src) if src else "")

    if action == "list":
        return {"ok": True, "dst": st["dst"], "src": st["src"], "layers": st["layers"],
                "built": bool(st["layers"]) and st.get("built_hash") == stack_hash(st), "stack_file": str(stack_path(repo, dst))}

    if action == "undo":
        if not st["layers"]:
            return {"ok": False, "error": "stack is empty: nothing to undo"}
        keep = [e for e in st["layers"] if to is not None and e["id"] <= int(to)] if to is not None else st["layers"][:-1]
        removed = [e["id"] for e in st["layers"] if e not in keep]
        if dry_run:
            return {"ok": True, "dry_run": True, "would_remove": removed}
        st["layers"] = keep
        _event(st, "undo", removed=removed)
        _save_stack(repo, st)
        return {"ok": True, "removed": removed, "layers_left": [e["id"] for e in keep],
                "next": "build to rebuild the clip without them" if st.get("built_hash") else "add the next layer"}

    if not src_n:
        raise ValueError("src (the dense base clip, e.g. AS_GSTN_RunStart_Generic_w3) is required for the first add/dump/build")
    _guard(repo, src_n, dst)

    if action == "add":
        if layer is None:
            raise ValueError(f"layer is required. Example: {L.EXAMPLE}")
        n_hint = anim_ops._clip_frame_count(repo, src_n, ue59_dir)
        d = _load_dump(repo, src_n)
        L.validate_layer(layer, d[0] if d else n_hint)               # clear error before anything else
        raws = [e["layer"] for e in st["layers"]] + [layer]
        rep = _measure(repo, src_n, raws, n_hint)
        this = rep["layers"][-1]
        res = {"ok": True, "dry_run": dry_run, "dst": st["dst"], "src": src_n, "measured": rep["measured"],
               "layer": this, "conservation": rep.get("conservation"), "stack_size": len(raws) - (1 if dry_run else 0)}
        if rep["measured"] and rep["conservation"] and not rep["conservation"]["ok"]:
            res["ok"] = False
            res["error"] = "conservation failed on the data (see conservation): layer not added"
            return res
        if dry_run:
            return res
        nid = max([e["id"] for e in st["layers"]], default=0) + 1
        st["src"] = src_n
        st["layers"].append({"id": nid, "layer": layer, "hash": L.layer_hash(L.validate_layer(layer))})
        _event(st, "add", id=nid)
        _save_stack(repo, st)
        try:
            anim_ops._manifest().record_dry_run(repo, st["dst"], stack_hash(st))
        except OSError:
            pass
        res.update({"id": nid, "stack_size": len(st["layers"]), "stack_file": str(stack_path(repo, dst)),
                    "next": "add the next layer, undo, or build (UE) once the step's numbers are right"})
        return res

    if action in ("dump", "build"):
        return _run_ue(action, repo, st, src_n, dst, worktree_dir, ue59_dir)
    raise ValueError("action must be add, undo, list, dump or build")


def _run_ue(action: str, repo: Path, st: dict[str, Any], src_n: str, dst: str, worktree_dir: str | None, ue59_dir: str) -> dict[str, Any]:
    if os.name != "nt":
        return {"ok": False, "error": f"{action} needs the editor (Windows desktop): add/undo/list work offline"}
    wt = Path(worktree_dir).resolve() if worktree_dir else repo
    env = os.environ.copy()
    env["GSTN_MCP_DIR"] = str(wt / "Tools" / "AnimPipeline" / "gstn_anim_mcp")
    out_dir = repo / "Saved" / "AnimPipeline" / "layers"
    out_dir.mkdir(parents=True, exist_ok=True)
    if action == "dump":
        env["GSTN_LAYER_DUMP"] = str(dump_path(repo, src_n))
        env["GSTN_LAYER_SRC"] = src_n
        env["GSTN_SAVE"] = "0"
        assets = src_n
        report = None
    else:
        if not st["layers"]:
            return {"ok": False, "error": "stack is empty: add a layer first"}
        snap = anim_ops._snapshot_asset(repo, st["dst"])
        payload = {"src": src_n, "dst": st["dst"], "layers": [e["layer"] for e in st["layers"]]}
        ip = out_dir / f"stack_{st['dst']}.json"
        ip.write_text(json.dumps(payload, indent=1), encoding="utf-8")
        report = out_dir / f"report_{st['dst']}.json"
        report.unlink(missing_ok=True)
        env.update({"GSTN_INTENT_JSON": str(ip), "GSTN_SAVE": "1", "GSTN_LAYER_REPORT": str(report)})
        assets = st["dst"]
    cmd = ["pwsh", "-NoProfile", "-File", str(wt / "Tools" / "CI" / "Run-SparseAnimStep.ps1"), "-WorktreeDir", str(wt),
           "-Script", r"Tools\AnimPipeline\gstn_anim_mcp\anim_layer_ue.py", "-Ue59Dir", ue59_dir, "-Assets", assets,
           "-Tag", f"layer_{action}_{assets[:20]}"]
    proc = subprocess.run(cmd, env=env, capture_output=True, text=True)
    err = [ln for ln in (proc.stdout or "").splitlines() if "ANIM_LAYER ERROR" in ln]
    if err or proc.returncode not in (0, 1):
        return {"ok": False, "error": err[0].strip() if err else f"editor step failed (rc={proc.returncode})",
                "stdout": (proc.stdout or "")[-1200:]}
    if action == "dump":
        return {"ok": True, "dump": str(dump_path(repo, src_n)), "next": "add layers: they are now measured in cm offline"}
    rep = json.loads(report.read_text(encoding="utf-8")) if report and report.exists() else {}
    ok = bool(rep.get("ok"))
    if ok:
        st["built_hash"] = stack_hash(st)
        _event(st, "build", hash=st["built_hash"])
        _save_stack(repo, st)
    res = {"ok": ok, "verdict": "PASS" if ok else "FAIL", "dst": st["dst"], "report": str(report),
           "conservation": rep.get("conservation"), "edited_bones": rep.get("edited_bones"),
           "next": "capture once per cycle (anim_capture_move / capture_ab)" if ok else f"anim_rollback asset={st['dst']}"}
    try:
        anim_ops._manifest().record_report(repo, st["dst"], stack_hash(st), res)
    except OSError:
        pass
    return res
