"""Checks that the kit is attached correctly to this repo (step 10 of the README). Prints one line per check, at most 10.
  python harness/scripts/python/kit_check.py      exit 0 = all OK"""
import json, os, subprocess, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit_config  # noqa: E402

root = kit_config.root()
checks = []


def ok(name, cond, hint=""):
    checks.append(cond)
    print(("OK   " if cond else "MANCA") + f" {name}" + ("" if cond else f"  -> {hint}"))


cfg = kit_config.path()
ok("kit.config.json", bool(cfg), "copia harness/kit.config.example.json nella root del repo come kit.config.json")
c = kit_config.load()
ok("project_name compilato", bool(c.get("project_name")) and c.get("project_name") != "MyProject", "imposta project_name")
eng = c.get("engine", {})
if eng.get("type") == "unreal":
    up = eng.get("uproject")
    ok("engine.uproject esiste", bool(up) and os.path.isfile(os.path.join(root, up)), "percorso relativo del .uproject")
hp = subprocess.run(["git", "-C", root, "config", "core.hooksPath"], capture_output=True, text=True).stdout.strip()
ok("guardia pre-commit installata", hp.endswith("harness/scripts/git"), "python harness/scripts/python/install_hooks.py")
ok("CLAUDE.md", os.path.isfile(os.path.join(root, "CLAUDE.md")), "da harness/templates/CLAUDE.md.template")
s = os.path.join(root, ".claude", "settings.json")
deny = []
if os.path.isfile(s):
    try:
        deny = json.load(open(s, encoding="utf-8")).get("permissions", {}).get("deny", [])
    except ValueError:
        pass
ok("permessi .claude/settings.json con deny", len(deny) > 0, "da harness/claude/settings.template.json")
for key, label in (("paths.handovers", "cartella handover"), ("paths.divergences", "file divergenze")):
    v = kit_config.get(key)
    ok(label, bool(v) and os.path.exists(os.path.join(root, v)), f"crea {v or key}")
print("ESITO: " + ("OK" if all(checks) else f"{checks.count(False)} da sistemare"))
sys.exit(0 if all(checks) else 1)
