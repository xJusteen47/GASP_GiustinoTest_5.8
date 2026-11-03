"""Adds (or with --uninstall removes) the GSTN Scripts menu and button bar in Cascadeur's user settings.

    python -I Tools/AnimPipeline/cascadeur_menu/install.py [--uninstall] [--settings=<settings.json>]

Edits %LOCALAPPDATA%/Nekki Limited/Cascadeur/settings.json: this folder on Python.Path, `gstn_scripts` in Python.Scripts,
`gstn_events` in Python.Events (the built-in `scripts`/`events` stay listed). Idempotent; a backup
settings.json.bak_gstn is written once; the file's read-only flag (set on this machine) is put back afterwards.
Cascadeur reads the file at start-up: restart it after installing.
"""
import json, os, shutil, stat, sys

HERE = os.path.dirname(os.path.abspath(__file__)).replace("\\", "/")
ADD = {"Scripts": ["scripts", "gstn_scripts"], "Events": ["events", "gstn_events"]}


def patch(s, uninstall=False):
    py = s.setdefault("Python", {})
    path = [p for p in py.get("Path", []) if p.replace("\\", "/").rstrip("/") != HERE]
    if not uninstall:
        path.append(HERE)
    py["Path"] = path
    py.setdefault("Commands", ["commands"])
    for key, items in ADD.items():
        cur = [x for x in py.get(key, items[:1]) if x != items[1]]
        for x in items[:1]:
            if x not in cur:
                cur.insert(0, x)
        py[key] = cur + ([] if uninstall else [items[1]])
    return s


def main(argv):
    opt = dict(a[2:].split("=", 1) if "=" in a else (a[2:], True) for a in argv if a.startswith("--"))
    p = opt.get("settings") or os.path.join(os.environ["LOCALAPPDATA"], "Nekki Limited", "Cascadeur", "settings.json")
    with open(p, encoding="utf-8") as f:
        s = json.load(f)
    if not os.path.exists(p + ".bak_gstn"):
        shutil.copy2(p, p + ".bak_gstn")
    ro = not os.stat(p).st_mode & stat.S_IWRITE
    if ro:
        os.chmod(p, stat.S_IREAD | stat.S_IWRITE)
    try:
        with open(p, "w", encoding="utf-8") as f:
            json.dump(patch(s, "uninstall" in opt), f, indent=4)
    finally:
        if ro:
            os.chmod(p, stat.S_IREAD)
    print(f"{'removed from' if 'uninstall' in opt else 'installed in'} {p}; restart Cascadeur")


if __name__ == "__main__":
    main(sys.argv[1:])
