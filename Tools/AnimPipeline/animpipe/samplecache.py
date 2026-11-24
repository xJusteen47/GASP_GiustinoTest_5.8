"""Bone-sample cache and call timing for the UE bridge (import-veloce-1, 2026-11-24).

Why: one animpipe round samples the SAME built clip three times through the bridge (build: 12 bones vs the source, legibility: its
own bone set, clearance: the collision bones), 8 frames per call, and samples the source take again even when the author poses did
not change. Each call is one game-thread round trip in the editor. The data is a pure function of the clip, so it is kept on disk
and served again, byte for byte, instead of asking the editor twice.

Quality rule (owner 11-24: same quality as the OG, savings only from faster processes): a cached answer is the exact dict the
editor returned for that clip, so every check reads the same numbers. Validity:
  - key = asset path + sha1 of its .uasset/.uexp on disk + space; a rebuild saves the clip (ue_rig.build, ue_write_take), so the
    file and the key change;
  - a package with unsaved edits in the editor is never served from (or written to) the cache: one dirty check per process and
    clip, through animpipe/samplecache_ue.py;
  - assets outside /Game (plugin content) or not found on disk are never cached;
  - ANIMPIPE_SAMPLE_CACHE_VERIFY=1 asks the editor anyway, compares with the cache and logs the largest difference (cm / deg):
    run it on the first real rounds; any value > 0 means the cache must stay off (then report it, never loosen a check).
Prefetch: on a miss the request carries the union of the bones ever asked for that clip path (bones_seen.json), so the build's
first pass already holds what legibility and clearance ask next. Bone transforms do not depend on which other bones are requested.

Env: ANIMPIPE_SAMPLE_CACHE = cache folder (unset = off; the round runner sets it), ANIMPIPE_TIMING = jsonl file, one line per
bridge call {t, pid, method, sec, cache, asset} (unset = off). Stdlib only: ue.py imports this from every legacy script.
"""
import hashlib, json, os, time

CACHED = ("sample_pose", "read_anim_sequence")
_dirty_seen = {}          # (asset, fp) -> bool, per process


def project_dir():
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.abspath(os.path.join(here, "..", "..", ".."))


def asset_files(asset, root=None):
    """/Game/A/B/AS_X(.AS_X) -> [Content/A/B/AS_X.uasset, .uexp if present]; [] when not a /Game asset or not on disk."""
    if not isinstance(asset, str) or not asset.startswith("/Game/"):
        return []
    rel = asset[len("/Game/"):].split(".", 1)[0]
    base = os.path.join(root or project_dir(), "Content", *rel.split("/"))
    files = [base + ext for ext in (".uasset", ".uexp") if os.path.exists(base + ext)]
    return files if files and files[0].endswith(".uasset") else []


def fingerprint(asset, root=None):
    files = asset_files(asset, root)
    if not files:
        return None
    h = hashlib.sha1()
    for p in files:
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
    return h.hexdigest()[:20]


def _log(rec):
    p = os.environ.get("ANIMPIPE_TIMING")
    if not p:
        return
    rec = dict(rec, t=round(time.time(), 3), pid=os.getpid())
    try:
        os.makedirs(os.path.dirname(os.path.abspath(p)), exist_ok=True)
        with open(p, "a", encoding="utf-8") as f:
            f.write(json.dumps(rec) + "\n")
    except OSError:
        pass


class Store:
    """One json file per (asset fingerprint, space): {"meta": {...}, "frames": {frame: {bone: bone_dict}}}."""

    def __init__(self, folder):
        self.dir = folder
        os.makedirs(folder, exist_ok=True)

    def _path(self, fp, space):
        return os.path.join(self.dir, f"{fp}_{space}.json")

    def load(self, fp, space):
        try:
            with open(self._path(fp, space), encoding="utf-8") as f:
                return json.load(f)
        except (OSError, ValueError):
            return {"meta": {}, "frames": {}}

    def save(self, fp, space, data):
        p = self._path(fp, space)
        tmp = f"{p}.{os.getpid()}.tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(data, f)
        os.replace(tmp, p)

    def seen(self, asset):
        try:
            with open(os.path.join(self.dir, "bones_seen.json"), encoding="utf-8") as f:
                return json.load(f).get(asset, [])
        except (OSError, ValueError):
            return []

    def add_seen(self, asset, bones):
        p = os.path.join(self.dir, "bones_seen.json")
        try:
            with open(p, encoding="utf-8") as f:
                d = json.load(f)
        except (OSError, ValueError):
            d = {}
        merged = sorted(set(d.get(asset, [])) | set(bones))
        if merged != d.get(asset):
            d[asset] = merged
            tmp = f"{p}.{os.getpid()}.tmp"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(d, f, indent=0)
            os.replace(tmp, p)


def _max_diff(a, b):
    """Largest absolute difference between two bone dicts (location cm, rotation deg, any numeric leaf)."""
    worst = 0.0
    for k in set(a) | set(b):
        x, y = a.get(k), b.get(k)
        if isinstance(x, dict) and isinstance(y, dict):
            worst = max(worst, _max_diff(x, y))
        elif isinstance(x, (int, float)) and isinstance(y, (int, float)):
            worst = max(worst, abs(float(x) - float(y)))
        elif x != y:
            return float("inf")
    return worst


def cached_call(raw, method, params, timeout, root=None, folder=None, dirty=None):
    """raw(method, params, timeout) is the real bridge call. dirty(asset) -> bool asks the editor (None = trust the disk)."""
    folder = folder or os.environ.get("ANIMPIPE_SAMPLE_CACHE")
    params = params or {}
    asset = params.get("assetPath")
    t0 = time.time()
    if not folder or method not in CACHED:
        r = raw(method, params, timeout)
        _log({"method": method, "sec": round(time.time() - t0, 3), "cache": "off", "asset": asset})
        return r
    fp = fingerprint(asset, root)
    if fp and dirty is not None:
        key = (asset, fp)
        if key not in _dirty_seen:
            try:
                _dirty_seen[key] = bool(dirty(asset))
            except Exception:
                _dirty_seen[key] = True             # cannot tell: do not trust the disk
        if _dirty_seen[key]:
            fp = None
    if not fp:
        r = raw(method, params, timeout)
        _log({"method": method, "sec": round(time.time() - t0, 3), "cache": "bypass", "asset": asset})
        return r
    store = Store(folder)
    verify = os.environ.get("ANIMPIPE_SAMPLE_CACHE_VERIFY") == "1"

    if method == "read_anim_sequence":
        d = store.load(fp, "info")
        if d["meta"].get("info") is not None and not verify:
            _log({"method": method, "sec": round(time.time() - t0, 3), "cache": "hit", "asset": asset})
            return d["meta"]["info"]
        r = raw(method, params, timeout)
        if verify and d["meta"].get("info") is not None:
            _log({"method": method, "cache": "verify", "asset": asset, "same": r == d["meta"]["info"]})
        if isinstance(r, dict) and r:
            store.save(fp, "info", {"meta": {"info": r}, "frames": {}})
        _log({"method": method, "sec": round(time.time() - t0, 3), "cache": "miss", "asset": asset})
        return r

    space = str(params.get("space", "component"))
    bones, frames = list(params.get("boneNames") or []), [int(f) for f in (params.get("frames") or [])]
    if not bones or not frames:
        return raw(method, params, timeout)
    d = store.load(fp, space)
    have = d["frames"]
    hit = all(str(f) in have and all(b in have[str(f)] for b in bones) for f in frames)
    if hit and not verify:
        out = dict(d["meta"])
        out["samples"] = [{"frame": f, "bones": [have[str(f)][b] for b in bones]} for f in frames]
        _log({"method": method, "sec": round(time.time() - t0, 3), "cache": "hit", "asset": asset, "n": len(frames)})
        return out
    ask = sorted(set(bones) | set(store.seen(asset))) if not verify else bones
    r = raw(method, dict(params, boneNames=ask), timeout)
    if not isinstance(r, dict) or "samples" not in r or r.get("dialogBlocking"):
        return r                                    # never cache a refused or blocked answer; the caller retries as before
    if verify and hit:
        worst = max(_max_diff(have[str(s["frame"])][b["name"]], b) for s in r["samples"] for b in s["bones"])
        _log({"method": method, "cache": "verify", "asset": asset, "max_diff": worst})
    for s in r["samples"]:
        slot = have.setdefault(str(s["frame"]), {})
        for b in s["bones"]:
            slot[b["name"]] = b
    d["meta"] = {k: v for k, v in r.items() if k != "samples"}
    store.save(fp, space, d)
    store.add_seen(asset, bones)
    out = dict(d["meta"])
    out["samples"] = [{"frame": s["frame"], "bones": [b for b in s["bones"] if b["name"] in set(bones)]} for s in r["samples"]]
    for s in out["samples"]:                         # requested order, as the editor would have returned it
        idx = {b["name"]: b for b in s["bones"]}
        s["bones"] = [idx[b] for b in bones if b in idx]
    _log({"method": method, "sec": round(time.time() - t0, 3), "cache": "miss", "asset": asset, "n": len(frames), "asked": len(ask)})
    return out
