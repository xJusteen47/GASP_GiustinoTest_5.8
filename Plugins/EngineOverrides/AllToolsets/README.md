# AllToolsets (engine override)

Copy of Epic's **All Toolsets** aggregator plugin (1.0, "Aggregator plugin that depends on all Toolsets plugins"). Editor-only, experimental, `NoRedist`, not enabled by default. It is an **unmodified copy as far as git shows** (only commit `53c8716`). It is enabled in `GASP_Giustino2.uproject`, which is how the toolset plugins (and so the `unreal-mcp` tool surface) get loaded.

## Purpose and structure

No code and no modules: the folder only contains `AllToolsets.uplugin`, whose `Plugins` list enables 21 toolset plugins:
AIModuleToolset, AnimationAssistantToolset, AutomationTestToolset, ConfigSettingsToolset, ConversationToolset, DataRegistryToolset, DataflowAgent, EditorToolset, GameFeaturesToolset, GameplayTagsToolset, GASToolsets, MCPClientToolset, NiagaraToolsets, PCGToolset, PhysicsToolsets, PluginToolset, SemanticSearchToolset, SlateInspectorToolset, StateTreeToolset, UMGToolSet, WorldConditionsToolset.
Of these, only `GASToolsets` is in this folder (see [../GASToolsets/README.md](../GASToolsets/README.md)); the rest come from the engine.

## Dependencies, tags, extension

- Depends on every plugin above; nothing depends on it except the project .uproject.
- Declares no gameplay tags and no modules; there is nothing to extend. To add a toolset, create a new plugin and add it to this list (or enable it directly in the .uproject).

## Using it standalone

Copy it into any project to enable all toolsets at once; every listed plugin must exist in the engine or project, otherwise the editor reports a missing plugin. The listed plugins are engine plugins that require an engine build shipping them (5.8).

## Where to read more

[../README.md](../README.md).
