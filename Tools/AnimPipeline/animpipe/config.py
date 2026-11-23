"""Paths and apps of the pipeline, from animpipe.toml (no fixed per-user paths in the code).

Read order, later wins: Tools/AnimPipeline/animpipe.toml (in git, relative paths), animpipe.local.toml next to it (not in
git: this machine's overrides, e.g. a data folder in another checkout), then the env vars ANIMPIPE_DATA / ANIMPIPE_CARDS
(tests point them at a temp folder). Relative paths resolve against the folder of the toml that set them.

Stdlib only and no animpipe imports: Cascadeur's embedded Python imports this file too (cascadeur_menu/_bridge.py).
"""
import os

try:
    import tomllib
except ImportError:          # Python < 3.11 (an older embedded interpreter): flat `key = "value"` lines are enough here
    tomllib = None

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
DEFAULTS = {"data_dir": "../../Saved/AnimPipeline", "cards_dir": "cards", "rig": "/Game/Characters/UE5_Mannequins/Rigs/CR_Mannequin_Body",
            "skeleton": "/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin", "dcc": "cascadeur", "engine": "unreal",
            "owner_lang": "it", "python": ""}


def _parse(text):
    if tomllib:
        return tomllib.loads(text)
    out = {}
    for line in text.splitlines():
        line = line.split("#", 1)[0].strip()
        if "=" in line and not line.startswith("["):
            k, v = (s.strip() for s in line.split("=", 1))
            out[k] = v.strip('"\'')
    return out


def load():
    cfg, base = dict(DEFAULTS), {k: TOOLS for k in DEFAULTS}
    for name in ("animpipe.toml", "animpipe.local.toml"):
        p = os.path.join(TOOLS, name)
        if os.path.exists(p):
            with open(p, encoding="utf-8") as f:
                d = _parse(f.read())
            for k, v in d.items():
                cfg[k] = v; base[k] = TOOLS
    for k in ("data_dir", "cards_dir"):
        cfg[k] = os.path.normpath(os.path.join(base[k], os.path.expanduser(str(cfg[k]))))
    if os.environ.get("ANIMPIPE_DATA"):
        cfg["data_dir"] = os.environ["ANIMPIPE_DATA"]
    if os.environ.get("ANIMPIPE_CARDS"):
        cfg["cards_dir"] = os.environ["ANIMPIPE_CARDS"]
    return cfg


CFG = load()
DATA = CFG["data_dir"]                      # Saved/AnimPipeline: everything that is not in git
CARDS = CFG["cards_dir"]
OUT = os.path.join(DATA, "animpipe")        # legacy per-command result files
CASC = os.path.join(DATA, "casc")           # scenes, exports, owner keys, Cascadeur requests
RUNS = os.path.join(DATA, "runs")           # runs/<move>/<stamp>_<verb>.json, one per verb call
SNAPSHOTS = os.path.join(DATA, "snapshots")  # snapshots/<move>/<stamp>.zip
VISUAL = os.path.join(DATA, "visual")       # visual/<move>/r<N>/{grid.png, manifest.json, verdict.json, owner_ok.json}
VARIANTS = os.path.join(DATA, "variants")   # variants/<move>.json: the open variant of a move (one per round)
