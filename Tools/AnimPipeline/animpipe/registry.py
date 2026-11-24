"""The clip registry (owner decision 3A, 2026-11-24): ONE file says the state of every clip version,
Tools/AnimPipeline/registro-clip.csv, and only the two verbs below write it. docs/notes/anim-approvate.md is a VIEW generated
from it (the git guard, anim_layer_ops and capture_move keep reading the asset names there); only its "Esenzioni dei
controlli" section stays hand-written, because an exemption is the owner's own words (INV-10).

Why: before 3A the state lived in four places that diverged (anim-approvate.md rows, approved/* tags, claude/approved-*
branches, handovers), and jab v12 / ftilt v3.1 existed only as a local copy on the desktop.

    publish <move> <ver> --from=<a.uasset,b.uasset> --review=<docs/notes/reviews/<variant>>
        refused unless the review folder proves BOTH: assert.txt with only "ASSERT: OK" lines (rule 10: the capture shows the
        expected clip) and a level-2 PASS (final/review_pack.json complete + final/verdict.json {"level": 2, "verdict":
        "PASS", "blind_critic": "PASS", "clips": [...]} naming every published asset). Copies the files byte for byte to
        Content/AnimPipeline/Published/<move>/<ver>/ (LFS, lockable: .gitattributes) with publish.json (sha256 per file),
        row -> PASS. A row already `approvata` (owner approval older than this command: jab v12, ftilt v3.1, dair v9) only
        gets its files stored: the owner's words are the authority there, the state does not change.
    approve <move> <ver> --owner="<owner's words>"
        row must be PASS (or approvata: re-run is idempotent); its Published files must be committed at HEAD. LFS-locks them
        (git lfs lock; ANIMPIPE_NO_LFS_LOCK=1 only in tests), creates the annotated tag approved/<move>-<ver> on HEAD (never
        moves an existing tag), row -> approvata with the owner's words and the commit, regenerates the view.
    registro check | view    validate the csv (schema, states, unique rows, approvata => words + commit) | regenerate the view

The published copy is an archive of the exact bytes the owner approved: in-game references (montage -> sequence, kits) keep
pointing at the working path until an editor session redirects them (desktop, outside this module).
Paths resolve against ANIMPIPE_REPO when set (tests), else the repository that holds this file."""
import csv, hashlib, io, json, os, re, shutil, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
COLUMNS = ["mossa", "versione", "stato", "responsabile", "asset_ue", "carta", "commit", "video_review", "data", "parole_owner", "note"]
STATES = ("WIP", "review", "PASS", "approvata", "bloccata", "scartata")
LOCKED = ("approvata", "bloccata")              # rows shown in the view: nobody saves over these assets
EXEMPT_HEADING = "## Esenzioni dei controlli"   # = gstn_anim_mcp/capture_move.EXEMPT_HEADING
EXEMPT_DEFAULT = (EXEMPT_HEADING + "\n"
                  "Solo l'owner (INV-10, `Tools/AnimPipeline/contract/invarianti-review.md`): un blocco di `animpipe check` che fallisce anche\n"
                  "sulla base approvata blocca la variante, salvo una riga qui con le parole dell'owner. Una riga vale per quella base, quel\n"
                  "blocco e quella riga esatta (frame e numeri): se la variante li cambia, blocca di nuovo. `stop_and_go` e `hold_reason` mai.\n"
                  "| base | blocco | riga esatta di animpipe check | owner (data e parole tra virgolette) |\n|---|---|---|---|\n")
SLUG = re.compile(r"^[a-z0-9][a-z0-9.\-]*$")


class RegistryError(ValueError):
    """A refusal: nothing was written."""


def repo():
    return os.environ.get("ANIMPIPE_REPO") or os.path.abspath(os.path.join(HERE, "..", "..", ".."))


def reg_path():
    return os.path.join(repo(), "Tools", "AnimPipeline", "registro-clip.csv")


def view_path():
    return os.path.join(repo(), "docs", "notes", "anim-approvate.md")


def published_dir(move, ver):
    return os.path.join(repo(), "Content", "AnimPipeline", "Published", move, ver)


def tag_name(move, ver):
    return f"approved/{move}-{ver}"


# ------------------------------------------------------------------ csv
def read():
    p = reg_path()
    if not os.path.exists(p):
        return []
    with open(p, encoding="utf-8", newline="") as f:
        r = csv.DictReader(f)
        if r.fieldnames != COLUMNS:
            raise RegistryError(f"{p}: header {r.fieldnames} != {COLUMNS}")
        return [dict(row) for row in r]


def write(rows):
    problems = validate(rows)
    if problems:
        raise RegistryError("registry not written: " + "; ".join(problems[:5]))
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=COLUMNS, lineterminator="\n")
    w.writeheader()
    for row in rows:
        w.writerow({k: row.get(k, "") for k in COLUMNS})
    tmp = reg_path() + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="") as f:
        f.write(buf.getvalue())
    os.replace(tmp, reg_path())


def validate(rows):
    out, seen = [], set()
    for i, r in enumerate(rows, 2):
        key = (r.get("mossa", ""), r.get("versione", ""))
        if not all(SLUG.match(x or "") for x in key):
            out.append(f"line {i}: mossa/versione {key} must be lowercase slugs (they name the tag approved/<mossa>-<versione>)")
        if key in seen:
            out.append(f"line {i}: {key} twice")
        seen.add(key)
        if r.get("stato") not in STATES:
            out.append(f"line {i}: stato {r.get('stato')!r} not in {STATES}")
        if r.get("stato") == "approvata" and not (r.get("parole_owner", "").strip() and r.get("commit", "").strip()):
            out.append(f"line {i}: approvata without the owner's words or the commit")
        if not r.get("asset_ue", "").strip():
            out.append(f"line {i}: no asset_ue")
    return out


def find(rows, move, ver):
    return next((r for r in rows if r["mossa"] == move and r["versione"] == ver), None)


# ------------------------------------------------------------------ view
def render_view(rows, old_text=""):
    exempt = EXEMPT_HEADING + old_text.split(EXEMPT_HEADING, 1)[1] if EXEMPT_HEADING in old_text else EXEMPT_DEFAULT
    lines = ["# Clip approvate e bloccate (vista generata)", "",
             "<!-- GENERATA da `python -m animpipe registro view` da Tools/AnimPipeline/registro-clip.csv: non modificarla a mano",
             "(owner 3A, 2026-11-24). Solo la sezione Esenzioni in fondo si scrive a mano, con le parole dell'owner. -->",
             "approvata = parole dell'owner + tag `approved/<mossa>-<versione>`; bloccata = nessuno la sovrascrive, ma non e' approvata",
             "(niente tag). Si lavora sempre su una variante nuova. Stato di tutte le altre versioni: il registro.", "",
             "| mossa | versione | stato | asset UE (carta; commit) | owner | tag |", "|---|---|---|---|---|---|"]
    for r in rows:
        if r["stato"] not in LOCKED:
            continue
        cell = lambda s: s.replace("|", "/").replace("\n", " ").strip()
        where = "; ".join(x for x in (cell(r["carta"]), f"commit {r['commit']}" if r["commit"] else "") if x)
        owner = cell(r["parole_owner"]) + (f" ({cell(r['note'])})" if r["note"] else "")
        tag = tag_name(r["mossa"], r["versione"]) if r["stato"] == "approvata" else "nessun tag"
        lines.append(f"| {r['mossa']} | {r['versione']} | {r['stato']} | {cell(r['asset_ue'])} ({where}) | {owner} | {tag} |")
    return "\n".join(lines) + "\n\n" + exempt.rstrip("\n") + "\n"


def write_view(rows):
    p = view_path()
    old = open(p, encoding="utf-8").read() if os.path.exists(p) else ""
    new = render_view(rows, old)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(new)
    return new != old


def view_in_sync():
    p = view_path()
    old = open(p, encoding="utf-8").read() if os.path.exists(p) else ""
    return render_view(read(), old) == old


# ------------------------------------------------------------------ git
def _git(*args, check=True):
    p = subprocess.run(["git", "-C", repo(), *args], capture_output=True, text=True, encoding="utf-8")
    if check and p.returncode:
        raise RegistryError(f"git {' '.join(args[:3])}: {(p.stderr or p.stdout).strip()[:300]}")
    return p.stdout.strip()


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


# ------------------------------------------------------------------ review evidence (rule 10, level 2)
def review_evidence(review_dir, assets):
    """-> list of reasons the review folder does NOT prove ASSERT: OK + level-2 PASS for these assets ([] = proven)."""
    d = review_dir if os.path.isabs(review_dir) else os.path.join(repo(), review_dir)
    why = []
    ap = os.path.join(d, "assert.txt")
    if not os.path.exists(ap):
        why.append(f"no {ap} (the capture must print ASSERT: OK before any review, rule 10)")
    else:
        rows = [l.strip() for l in open(ap, encoding="utf-8") if l.strip().startswith("ASSERT:")]
        if not rows or any(not l.startswith("ASSERT: OK") for l in rows):
            why.append(f"assert.txt is not all ASSERT: OK ({len(rows)} ASSERT lines)")
    pk = os.path.join(d, "final", "review_pack.json")
    try:
        if not json.load(open(pk, encoding="utf-8")).get("complete"):
            why.append("final/review_pack.json is not complete (level 2 needs every frame covered)")
    except (OSError, ValueError):
        why.append("no readable final/review_pack.json (level-2 material from anim_review_pack)")
    vp = os.path.join(d, "final", "verdict.json")
    try:
        v = json.load(open(vp, encoding="utf-8"))
    except (OSError, ValueError):
        v = None
        why.append('no readable final/verdict.json ({"level": 2, "verdict": "PASS", "blind_critic": "PASS", "clips": [...]})')
    if v is not None:
        if v.get("level") != 2 or v.get("verdict") != "PASS" or v.get("blind_critic") != "PASS":
            why.append(f"level-2 verdict is not PASS with a blind critic PASS: level {v.get('level')}, verdict {v.get('verdict')}, "
                       f"blind_critic {v.get('blind_critic')}")
        names = {os.path.splitext(os.path.basename(a))[0] for a in assets}
        miss = sorted(names - set(v.get("clips", [])))
        if miss:
            why.append(f"the PASS verdict does not name {miss}: it judged other clips")
    return why


# ------------------------------------------------------------------ verbs
def publish(move, ver, files, review=None, by="thread", date=None):
    import datetime
    rows = read()
    row = find(rows, move, ver)
    if row is None:
        raise RegistryError(f"{move} {ver} is not in the registry: add its WIP/review row first (asset_ue, carta)")
    if row["stato"] in ("bloccata", "scartata"):
        raise RegistryError(f"{move} {ver} is {row['stato']}: never published")
    if not files:
        raise RegistryError("--from=<file.uasset,...>: the asset files to store")
    src = [f if os.path.isabs(f) else os.path.join(repo(), f) for f in files]
    bad = [f for f in src if not (os.path.isfile(f) and f.endswith((".uasset", ".umap")))]
    if bad:
        raise RegistryError(f"not an existing .uasset/.umap: {bad}")
    legacy = row["stato"] == "approvata"
    if not legacy:
        if not review:
            raise RegistryError("--review=<docs/notes/reviews/<variant>>: publish needs ASSERT: OK and the level-2 PASS")
        why = review_evidence(review, src)
        if why:
            raise RegistryError("publish refused: " + " | ".join(why))
    dst = published_dir(move, ver)
    if os.path.isdir(dst) and os.listdir(dst):
        old = json.load(open(os.path.join(dst, "publish.json"), encoding="utf-8")) if os.path.exists(os.path.join(dst, "publish.json")) else {}
        same = {os.path.basename(f): _sha256(f) for f in src} == old.get("files")
        if not same:
            raise RegistryError(f"{dst} already holds other bytes: a published version is never overwritten, publish a new version")
    os.makedirs(dst, exist_ok=True)
    for f in src:
        shutil.copy2(f, os.path.join(dst, os.path.basename(f)))
    man = {"move": move, "version": ver, "files": {os.path.basename(f): _sha256(f) for f in src},
           "review": review or "", "legacy_owner_approval": legacy, "by": by,
           "date": date or datetime.date.today().isoformat()}
    with open(os.path.join(dst, "publish.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(man, f, indent=1); f.write("\n")
    if not legacy:
        row.update(stato="PASS", responsabile=by, data=man["date"], video_review=os.path.relpath(
            review if os.path.isabs(review) else os.path.join(repo(), review), repo()).replace(os.sep, "/"))
    write(rows)
    return {"published": os.path.relpath(dst, repo()).replace(os.sep, "/"), "files": man["files"], "stato": row["stato"],
            "legacy_owner_approval": legacy}


def approve(move, ver, owner_words, date=None, lock=None):
    import datetime
    words = (owner_words or "").strip()
    if len(words.strip('"\' ')) < 2:
        raise RegistryError('--owner="<the owner\'s own words>": only the owner approves, quote him')
    rows = read()
    row = find(rows, move, ver)
    if row is None:
        raise RegistryError(f"{move} {ver} is not in the registry")
    if row["stato"] not in ("PASS", "approvata"):
        raise RegistryError(f"{move} {ver} is {row['stato']}: only a published PASS goes to the owner (publish first)")
    dst = published_dir(move, ver)
    files = sorted(f for f in (os.listdir(dst) if os.path.isdir(dst) else []) if f.endswith((".uasset", ".umap")))
    if not files:
        raise RegistryError(f"nothing in {os.path.relpath(dst, repo())}: publish first")
    rel = [os.path.relpath(os.path.join(dst, f), repo()).replace(os.sep, "/") for f in files + ["publish.json"]]
    tracked = set(_git("ls-tree", "-r", "--name-only", "HEAD", "--", os.path.relpath(dst, repo()).replace(os.sep, "/")).splitlines())
    dirty = _git("status", "--porcelain", "--", *rel)
    if set(rel) - tracked or dirty:
        raise RegistryError(f"commit the published files first ({sorted(set(rel) - tracked) or dirty.splitlines()[:3]}): the tag points at HEAD")
    head = _git("rev-parse", "HEAD")
    tag = tag_name(move, ver)
    have = _git("rev-parse", "-q", "--verify", f"refs/tags/{tag}^{{commit}}", check=False)
    if have and have != head and row["stato"] != "approvata":
        raise RegistryError(f"tag {tag} already exists on {have[:8]}: an approved tag is never moved")
    if lock is None:
        lock = os.environ.get("ANIMPIPE_NO_LFS_LOCK") != "1"
    locked = []
    if lock:
        for r in rel[:-1]:
            p = subprocess.run(["git", "-C", repo(), "lfs", "lock", r], capture_output=True, text=True)
            if p.returncode and "already locked" not in (p.stderr + p.stdout):
                raise RegistryError(f"git lfs lock {r}: {(p.stderr or p.stdout).strip()[:200]} (push the published files, then approve)")
            locked.append(r)
    if not have:
        _git("tag", "-a", tag, head, "-m", f"owner: {words}")
    if row["stato"] != "approvata":
        row.update(stato="approvata", responsabile="owner", commit=(have or head)[:8], parole_owner=words,
                   data=date or datetime.date.today().isoformat())
    write(rows)
    write_view(rows)
    return {"tag": tag, "commit": row["commit"], "locked": locked, "stato": "approvata", "view": os.path.relpath(view_path(), repo())}


def registro(sub):
    rows = read()
    if sub == "check":
        probs = validate(rows)
        if not view_in_sync():
            probs.append("docs/notes/anim-approvate.md is not the view of the registry: run `animpipe registro view`")
        tags = set(_git("tag", "-l", "approved/*", check=False).splitlines())
        missing = [tag_name(r["mossa"], r["versione"]) for r in rows if r["stato"] == "approvata" and tag_name(r["mossa"], r["versione"]) not in tags]
        return probs, {"rows": len(rows), "by_state": {s: sum(r["stato"] == s for r in rows) for s in STATES if any(r["stato"] == s for r in rows)},
                       "tags_missing_here": missing}
    if sub == "view":
        changed = write_view(rows)
        return [], {"rows": len(rows), "view_changed": changed}
    raise RegistryError("registro check|view")
