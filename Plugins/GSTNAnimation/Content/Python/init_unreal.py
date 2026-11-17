# Runs once when the editor starts (PythonScriptPlugin loads init_unreal.py from every enabled plugin's Content/Python).
# Registers the GSTN Motion Tools MCP toolset when the editor's ToolsetRegistry is available. Never raises: the panel
# and `import gstn_motion_tools` work without it.
try:
    import unreal
    if hasattr(unreal, "ToolsetDefinition") and hasattr(unreal, "ToolsetRegistry"):
        from gstn_motion_tools import toolset as _gstn_mt_toolset
        unreal.ToolsetRegistry.register_toolset_class(_gstn_mt_toolset.GSTNMotionToolsToolset)
except Exception as _exc:  # noqa: BLE001
    try:
        unreal.log_warning("GSTN Motion Tools: MCP toolset not registered (%s); Python API still available." % _exc)
    except Exception:
        pass
