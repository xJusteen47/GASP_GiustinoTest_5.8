"""UE 5.9 release-note checks that need the editor (read-only: loads assets, saves nothing).

Run in an open editor (MCP execute_python or Output Log `py`), or headless:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -nullrhi
Writes <Project>/Saved/UE59/ue59_editor_checks.txt and prints one line.
"""
import json, os, sys, traceback
import unreal

PROJ = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
OUT_DIR = os.path.join(PROJ, "Saved", "UE59")
os.makedirs(OUT_DIR, exist_ok=True)
OUT = os.path.join(OUT_DIR, "ue59_editor_checks.txt")
HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else os.path.join(PROJ, "Tools", "UE59")
L = []


def section(name, fn):
    L.append(f"\n== {name} ==")
    try:
        fn()
    except Exception:
        L.append("ERROR " + traceback.format_exc().strip().splitlines()[-1])


def mcp_settings():
    for cls in ("ModelContextProtocolSettings", "ModelContextProtocolProjectSettings", "ToolsetRegistrySettings"):
        c = getattr(unreal, cls, None)
        if c is None:
            L.append(f"{cls}: not exposed to Python"); continue
        cdo = unreal.get_default_object(c)
        for p in ("server_url_path", "server_port_number", "auto_start_server", "additional_allowed_origins",
                  "maximum_protocol_version", "enable_tool_search", "instructions",
                  "blocked_names", "allowed_names", "agent_skill_blocked_names", "agent_skill_allowed_names"):
            try:
                L.append(f"{cls}.{p} = {cdo.get_editor_property(p)}")
            except Exception:
                pass
    # registered toolsets (names only), to choose a block list
    names = sorted(n for n in dir(unreal) if n.endswith(("Toolset", "Toolsets", "Tools")) and not n.startswith("_"))
    L.append(f"python classes ending Toolset/Tools: {len(names)}")
    L.append(", ".join(names))


def frame_rate():
    for cv in ("t.MaxFPS", "r.VSync", "r.VSyncEditor", "t.UnfocusedFPSLimit", "Editor.UnfocusedFPSLimit", "r.DynamicRes.FrameTimeBudget"):
        try:
            L.append(f"{cv} = {unreal.SystemLibrary.get_console_variable_float_value(cv)}")
        except Exception:
            L.append(f"{cv} = n/a")
    try:
        gus = unreal.GameUserSettings.get_game_user_settings()
        L.append(f"GameUserSettings.frame_rate_limit = {gus.get_frame_rate_limit()}  vsync = {gus.is_v_sync_enabled()}")
    except Exception:
        L.append("GameUserSettings: n/a")


def python_packages():
    L.append(f"python {sys.version.split()[0]} exe={sys.executable}")
    for m in ("numpy", "PIL", "scipy", "cv2", "torch", "matplotlib"):
        try:
            mod = __import__(m)
            L.append(f"import {m}: OK {getattr(mod, '__version__', '')} from {os.path.dirname(getattr(mod, '__file__', '') or '')}")
        except Exception as e:
            L.append(f"import {m}: FAIL {type(e).__name__}")
    pm = unreal.PluginBlueprintLibrary if hasattr(unreal, "PluginBlueprintLibrary") else None
    if pm:
        en = set(pm.get_enabled_plugin_names())
        for p in ("PythonFoundationPackages", "PythonMLPackages", "ModelContextProtocol", "AllToolsets", "UAF", "UAFDefaultNotifies", "AIAssistant"):
            L.append(f"plugin {p} enabled = {p in en}")


def control_rig():
    dep = set(json.load(open(os.path.join(HERE, "cr_deprecated_59.json"))))
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/ControlRigDeveloper", "ControlRigBlueprint")],
                          package_paths=["/Game"], recursive_paths=True, recursive_classes=True)
    assets = [a for a in ar.get_assets(flt) if not str(a.package_name).startswith("/Game/MetaHumans")]
    L.append(f"control rig blueprints (excluding MetaHumans): {len(assets)}")
    for a in assets:
        bp = a.get_asset()
        found = {}
        try:
            models = bp.get_all_models()
        except Exception:
            models = [bp.get_model()]
        for g in models:
            for n in g.get_nodes():
                try:
                    st = n.get_script_struct()
                except Exception:
                    continue
                if st and st.get_name() in dep:
                    found.setdefault(st.get_name(), []).append(str(n.get_node_path()))
        L.append(f"{a.package_name}: " + ("clean" if not found else "; ".join(f"{k} x{len(v)}" for k, v in sorted(found.items()))))


def apis():
    for cls, probe in (("ChooserTableEditorLibrary", None), ("BlueprintDslToolset", None), ("StateTreeEditingSubsystem", None),
                       ("UAFNotify_PlayNiagaraEffect", None), ("AnimNotify_PlayNiagaraEffect", None),
                       ("AIAssistantInstance", None)):
        c = getattr(unreal, cls, None)
        L.append(f"{cls}: {'present, ' + str(len([m for m in dir(c) if not m.startswith('_')])) + ' members' if c else 'absent'}")
    st = [n for n in dir(unreal) if "StateTree" in n and ("Tool" in n or "Library" in n or "Editing" in n)]
    L.append(f"StateTree editing classes: {st}")


section("MCP + Toolset Registry settings", mcp_settings)
section("Frame-rate cap", frame_rate)
section("Python packages / plugins", python_packages)
section("Control Rig deprecated nodes (exact struct match)", control_rig)
section("Editing APIs available to Python", apis)

open(OUT, "w", encoding="utf-8").write("\n".join(L))
print(f"UE59 checks written: {OUT} ({len(L)} lines)")
