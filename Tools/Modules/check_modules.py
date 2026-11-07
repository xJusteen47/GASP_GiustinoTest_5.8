"""Grep gate of the optional GSTN plugins (plain Python 3, no editor).

The module boundaries of the optional plugins are rules of the docs (combat-m6 spec 1b, combat-m7-piggyback.md 2b, plan G4). M6 checked
them by hand; this script makes them one command. It reads source and config text only (Binaries and Intermediate are skipped) and prints
one line per violation; the exit code is 1 when there is any.

Checks:
  1. Nothing outside Plugins/GSTNPiggyback names GSTNPiggyback (code, Build.cs, .uplugin, config, the .uproject): nothing depends on it.
     --allow-uproject skips the .uproject, for a local build that lists the plugin enabled (never commit that).
  2. Plugins/GSTNPiggyback/Source has no "/Game/" path and no ConstructorHelpers: the plugin is usable alone.
  3. Module edges (Build.cs): GSTNPiggybackCore lists no GAS, NP, Mover, Editor or sibling GAS/NP module; GSTNLink* never lists a
     GSTNDefence module and GSTNDefence* never a GSTNLink module; the two GSTNExtensionTest modules never list each other;
     GSTNCore (shared L2 helpers, M8 Q7) lists no GSTN module and no GAS, NP, Mover, Editor or project module;
     GSTNMinionCore lists no GAS, NP, Mover, Editor, project or sibling minion module.
  4. Nothing is built on GSTNMinion (M8 plan section 0): no module of GSTNCombat*, GSTNInput*, GSTNLink*, GSTNDefence*,
     GSTNParts* or GSTNPlatform* lists a GSTNMinion module or #includes a header whose path names Minion; and
     Plugins/GSTNMinion/Source has no "/Game/" path and no ConstructorHelpers.

Usage: python Tools/Modules/check_modules.py [--root <project dir>] [--allow-uproject]
"""

import argparse
import os
import re
import sys

TEXT_EXTENSIONS = {".h", ".hpp", ".cpp", ".inl", ".cs", ".uplugin", ".ini", ".uproject", ".json"}
SKIPPED_DIRS = {"Binaries", "Intermediate", "Saved", "DerivedDataCache", ".git", "__pycache__"}

PIGGYBACK_DIR = os.path.join("Plugins", "GSTNPiggyback")
MINION_DIR = os.path.join("Plugins", "GSTNMinion")

# Plugins that must not know GSTNMinion (M8 plan section 0, grep gate): module name prefix of their Build.cs files / folder names
MINION_BLIND_PREFIXES = ["GSTNCombat", "GSTNInput", "GSTNLink", "GSTNDefence", "GSTNParts", "GSTNPlatform"]
MINION_MODULE_PREFIX = "GSTNMinion"

# Build.cs file name -> module names it must not list
FORBIDDEN_EDGES = {
    # GSTNCore is the shared L2 helper (M8 Q7): engine modules only, so any GSTN plugin may depend on it without a cycle
    "GSTNCore.Build.cs": [
        "GameplayAbilities", "AbilitySystemSimulation", "NetworkPrediction", "Mover", "UnrealEd", "GASP_Giustino2",
    ],
    # GSTNMinionCore is the pure part of the minion plugin: engine modules, GSTNCombatCore and GSTNCore only
    "GSTNMinionCore.Build.cs": [
        "GameplayAbilities", "AbilitySystemSimulation", "NetworkPrediction", "Mover", "UnrealEd", "GASP_Giustino2",
        "GSTNCombatGAS", "GSTNCombatNP", "GSTNMinionRuntime", "GSTNMinionEditor", "GSTNMinionTests",
    ],
    "GSTNPiggybackCore.Build.cs": [
        "GameplayAbilities", "AbilitySystemSimulation", "NetworkPrediction", "Mover", "UnrealEd",
        "GSTNCombatGAS", "GSTNCombatNP", "GSTNLinkGAS", "GSTNLinkNP", "GSTNInputNP", "GSTNPiggybackRuntime",
        "GSTNPiggybackEditor", "GSTNPiggybackTests", "GASP_Giustino2",
    ],
    "GSTNPiggybackRuntime.Build.cs": ["GSTNPiggybackEditor", "GSTNPiggybackTests", "GASP_Giustino2", "GSTNDefenceCore", "GSTNDefenceGAS", "GSTNDefenceNP"],
    "GSTNExtTestLink.Build.cs": ["GSTNExtTestDefence", "GSTNDefenceCore", "GSTNDefenceGAS", "GSTNDefenceNP"],
    "GSTNExtTestDefence.Build.cs": ["GSTNExtTestLink", "GSTNLinkCore", "GSTNLinkGAS", "GSTNLinkNP"],
}
# Build.cs file name prefix -> module name prefix it must not list
FORBIDDEN_PREFIX_EDGES = {
    "GSTNLink": "GSTNDefence",
    "GSTNDefence": "GSTNLink",
    "GSTNCore": "GSTN",
}


def iter_text_files(top):
    for dirpath, dirnames, filenames in os.walk(top):
        dirnames[:] = [d for d in dirnames if d not in SKIPPED_DIRS]
        for name in filenames:
            if os.path.splitext(name)[1].lower() in TEXT_EXTENSIONS:
                yield os.path.join(dirpath, name)


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def listed_modules(build_cs_text):
    """Every quoted name in the Build.cs (module lists are string arrays); comments are dropped first"""
    without_comments = re.sub(r"//[^\n]*", "", build_cs_text)
    without_comments = re.sub(r"/\*.*?\*/", "", without_comments, flags=re.S)
    return set(re.findall(r'"([A-Za-z0-9_]+)"', without_comments))


def check(root, allow_uproject):
    problems = []
    piggyback_root = os.path.normcase(os.path.join(root, PIGGYBACK_DIR))
    tools_root = os.path.normcase(os.path.join(root, "Tools", "Modules"))

    # 1. Nothing outside the plugin names it
    scan_tops = [os.path.join(root, "Source"), os.path.join(root, "Plugins"), os.path.join(root, "Config")]
    for top in scan_tops:
        if not os.path.isdir(top):
            continue
        for path in iter_text_files(top):
            normalised = os.path.normcase(path)
            if normalised.startswith(piggyback_root + os.sep) or normalised.startswith(tools_root + os.sep):
                continue
            if "GSTNPiggyback" in read(path):
                problems.append("names GSTNPiggyback outside the plugin: " + os.path.relpath(path, root))
    if not allow_uproject:
        for name in os.listdir(root):
            if name.endswith(".uproject") and "GSTNPiggyback" in read(os.path.join(root, name)):
                problems.append("the .uproject lists GSTNPiggyback (keep it out of the committed file): " + name)

    # 2. The plugins are usable alone
    for plugin_dir in (PIGGYBACK_DIR, MINION_DIR):
        plugin_source = os.path.join(root, plugin_dir, "Source")
        if os.path.isdir(plugin_source):
            for path in iter_text_files(plugin_source):
                text = read(path)
                for needle in ("/Game/", "ConstructorHelpers"):
                    if needle in text:
                        problems.append("%s in %s" % (needle, os.path.relpath(path, root)))

    # 3. Module edges
    plugins = os.path.join(root, "Plugins")
    for path in iter_text_files(plugins):
        name = os.path.basename(path)
        if not name.endswith(".Build.cs"):
            continue
        modules = listed_modules(read(path))
        own = name[: -len(".Build.cs")]
        for forbidden in FORBIDDEN_EDGES.get(name, []):
            if forbidden in modules:
                problems.append("%s lists %s" % (os.path.relpath(path, root), forbidden))
        for own_prefix, forbidden_prefix in FORBIDDEN_PREFIX_EDGES.items():
            if own.startswith(own_prefix):
                for module in sorted(modules):
                    if module.startswith(forbidden_prefix):
                        problems.append("%s lists %s" % (os.path.relpath(path, root), module))
        # 4a. The older plugins never list a minion module
        if any(own.startswith(prefix) for prefix in MINION_BLIND_PREFIXES):
            for module in sorted(modules):
                if module.startswith(MINION_MODULE_PREFIX):
                    problems.append("%s lists %s (nothing may depend on GSTNMinion)" % (os.path.relpath(path, root), module))

    # 4b. ...and never include a minion header (comments may still name GSTNMinion)
    include_minion = re.compile(r'^\s*#\s*include\s*[<"][^>"]*Minion[^>"]*[>"]', re.M)
    if os.path.isdir(plugins):
        for entry in sorted(os.listdir(plugins)):
            if not any(entry.startswith(prefix) for prefix in MINION_BLIND_PREFIXES):
                continue
            for path in iter_text_files(os.path.join(plugins, entry)):
                if os.path.splitext(path)[1].lower() in (".h", ".hpp", ".cpp", ".inl") and include_minion.search(read(path)):
                    problems.append("includes a minion header: " + os.path.relpath(path, root))
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", default=os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")))
    parser.add_argument("--allow-uproject", action="store_true", help="skip the .uproject (a local build with the plugin enabled)")
    args = parser.parse_args()

    problems = check(os.path.abspath(args.root), args.allow_uproject)
    for problem in problems:
        print("FAIL " + problem)
    print("check_modules: %d problem(s)" % len(problems))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
