"""Reads kit.config.json (repo root, found by walking up from this file or the cwd; env KIT_CONFIG overrides).

    import kit_config
    kit_config.root()                      # folder that holds kit.config.json
    kit_config.get("engine.uproject")      # dotted key, default None
"""
import json, os

_cache = None


def path():
    env = os.environ.get("KIT_CONFIG")
    if env and os.path.isfile(env):
        return os.path.abspath(env)
    for start in (os.path.dirname(os.path.abspath(__file__)), os.getcwd()):
        d = start
        while True:
            f = os.path.join(d, "kit.config.json")
            if os.path.isfile(f):
                return f
            parent = os.path.dirname(d)
            if parent == d:
                break
            d = parent
    return None


def load():
    global _cache
    if _cache is None:
        p = path()
        _cache = json.load(open(p, encoding="utf-8")) if p else {}
    return _cache


def root():
    p = path()
    return os.path.dirname(p) if p else os.getcwd()


def get(key, default=None):
    v = load()
    for part in key.split("."):
        if not isinstance(v, dict) or part not in v:
            return default
        v = v[part]
    return v
