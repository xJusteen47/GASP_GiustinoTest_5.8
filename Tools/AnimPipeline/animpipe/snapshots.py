"""Compressed snapshots of everything a move's writes can touch, taken automatically before every writing verb.

    <data>/snapshots/<move>/<YYYYMMDD_HHMMSS>.zip
      meta.json                 {move, stamp, reason, verb, card_sha, files}
      cards/<kit>/<move>.json   the card, plus approved/ manual/ refs/ files of the move (cards_dir relative)
      data/casc/<move>_*.json   owner keys, export plan, Cascadeur readback (data_dir relative)
      data/casc/<move>.casc     the owner's scene (kept, never written back by restore: see below)

restore writes the cards/ and data/ json files back and removes the move's files that did not exist at the snapshot time,
after taking a snapshot of the current state ("before restore"), so a restore can itself be undone. The owner's .casc
is never overwritten (owner rule: his scene is his): restore puts the copy in data/casc/restored/ for him to open.
Snapshots are never deleted by the pipeline.
"""
import glob, hashlib, json, os, re, time, zipfile

from animpipe import config as K

CASC_MAX_MB = 30          # a scene larger than this is not copied (the json side is what the pipeline needs)


def _is_variant_of(name, move):
    return re.match(re.escape(move) + r"_var[A-Z]\b", name) is not None


def move_files(move, card):
    """[(arcname, abspath)] of the move's files that exist now."""
    kit_dir = os.path.dirname(card)
    out = []
    pats = [os.path.join(kit_dir, f"{move}.json")] + [os.path.join(kit_dir, d, f"{move}[._]*") for d in ("approved", "manual", "refs")]
    for p in pats:
        for f in sorted(glob.glob(p)):
            if os.path.isfile(f) and not _is_variant_of(os.path.basename(f), move):
                out.append(("cards/" + os.path.relpath(f, K.CARDS).replace(os.sep, "/"), f))
    for f in sorted(glob.glob(os.path.join(K.CASC, f"{move}_*.json"))):
        name = os.path.basename(f)
        if not _is_variant_of(name, move) and not re.match(re.escape(move) + r"_\d{8}_", name):
            out.append(("data/" + os.path.relpath(f, K.DATA).replace(os.sep, "/"), f))
    scene = os.path.join(K.CASC, f"{move}.casc")
    if os.path.isfile(scene) and os.path.getsize(scene) <= CASC_MAX_MB * 2 ** 20:
        out.append(("data/casc/" + f"{move}.casc", scene))
    return out


def sha(path):
    with open(path, "rb") as f:
        return hashlib.sha1(f.read()).hexdigest()[:12]


def folder(move):
    return os.path.join(K.SNAPSHOTS, move)


def take(move, card, reason, verb=""):
    """Writes the zip, returns its stamp."""
    os.makedirs(folder(move), exist_ok=True)
    stamp = time.strftime("%Y%m%d_%H%M%S"); s, i = stamp, 1
    while os.path.exists(os.path.join(folder(move), f"{s}.zip")):      # two writes in the same second
        s = f"{stamp}_{i}"; i += 1
    files = move_files(move, card)
    meta = {"move": move, "stamp": s, "reason": reason, "verb": verb, "card_sha": sha(card) if os.path.exists(card) else None,
            "files": [a for a, _ in files]}
    with zipfile.ZipFile(os.path.join(folder(move), f"{s}.zip"), "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("meta.json", json.dumps(meta, indent=1))
        for arc, f in files:
            z.write(f, arc)
    return s


def listing(move):
    """[{stamp, reason, verb, card_sha, files}] oldest first."""
    out = []
    for p in sorted(glob.glob(os.path.join(folder(move), "*.zip"))):
        try:
            with zipfile.ZipFile(p) as z:
                out.append(json.loads(z.read("meta.json")))
        except (zipfile.BadZipFile, KeyError, ValueError):
            out.append({"stamp": os.path.basename(p)[:-4], "reason": "unreadable", "files": []})
    return out


def resolve(move, date):
    """The one stamp matching `date` (a stamp prefix, e.g. 20261103 or 20261103_0930, or "last"), else ValueError."""
    stamps = [m["stamp"] for m in listing(move)]
    if not stamps:
        raise LookupError(f"no snapshot for {move}")
    if date in ("last", "latest", ""):
        return stamps[-1]
    hit = [s for s in stamps if s == date] or [s for s in stamps if s.startswith(date.replace("-", "").replace(":", ""))]
    if len(hit) != 1:
        raise LookupError(f"{len(hit)} snapshots match {date!r}: {', '.join(hit[-5:]) or 'none'} (give a longer stamp)")
    return hit[0]


def _target(arc):
    root, rel = arc.split("/", 1)
    return os.path.join(K.CARDS if root == "cards" else K.DATA, *rel.split("/"))


def restore(move, card, stamp):
    """Puts the snapshot back. Returns {restored, removed, scene_copy, before}."""
    before = take(move, card, f"before restore {stamp}", "restore")
    with zipfile.ZipFile(os.path.join(folder(move), f"{stamp}.zip")) as z:
        meta = json.loads(z.read("meta.json"))
        keep, scene_copy = set(), None
        for arc in meta["files"]:
            if arc.endswith(".casc"):
                d = os.path.join(K.CASC, "restored"); os.makedirs(d, exist_ok=True)
                scene_copy = os.path.join(d, f"{move}_{stamp}.casc")
                with open(scene_copy, "wb") as f:
                    f.write(z.read(arc))
                keep.add(os.path.normcase(os.path.abspath(_target(arc))))
                continue
            t = _target(arc); os.makedirs(os.path.dirname(t), exist_ok=True)
            with open(t, "wb") as f:
                f.write(z.read(arc))
            keep.add(os.path.normcase(os.path.abspath(t)))
    removed = []
    for arc, f in move_files(move, card):
        if os.path.normcase(os.path.abspath(f)) not in keep and not f.endswith(".casc"):
            os.remove(f); removed.append(arc)       # it is in the "before" snapshot
    return {"restored": [a for a in meta["files"] if not a.endswith(".casc")], "removed": removed, "scene_copy": scene_copy,
            "before": before}
