# UE_MCP_Bridge

Third-party editor plugin (UE-MCP, version 1.4.3 in `UE_MCP_Bridge.uplugin`). It is a C++ WebSocket server inside the Unreal
Editor that exposes editor actions ("handlers": spawn actors, edit Blueprints, materials, animation, GAS assets, PCG, ...)
to an external UE-MCP MCP server (Node/TypeScript, not part of this repo). The `.uplugin` advertises 1914+ actions.

This is editor-only tooling. It contains no game code, no gameplay tags, and nothing in a packaged build depends on it.
It is not the same thing as the `unreal-mcp` server in `.mcp.json` (an HTTP endpoint on `127.0.0.1:8000`); the two are
separate integrations.

## Modules

Both modules are `Editor` type, so they are never built into a game target.

| Module | Loading phase | Contents |
|---|---|---|
| `UE_MCP_BridgeStatus` | `PostConfigInit` | Engine-status snapshot published before the bridge exists, so the startup window (asset scan, shader compile, map load, prompts) is visible to the client. Public headers `MCPEngineStatus.h`, `MCPBridgeStateDir.h`. Depends only on `Core`, `Json`. |
| `UE_MCP_Bridge` | `PostEngineInit` | The server and all handlers. Entry point `Source/UE_MCP_Bridge/Private/UE_MCP_Bridge.cpp` (`FUE_MCP_BridgeModule`). |

Layout of `Source/UE_MCP_Bridge/Private/` in `UE_MCP_Bridge`:

- `BridgeServer.*`, `BridgeWebSocket.*`, `BridgePortConfig.*`, `BridgeStateFiles.*`: socket, WebSocket framing, port selection, state files.
- `HandlerRegistry.*`, `HandlerCatalog.*`: method-name to function map, and the single place where every built-in category is registered.
- `GameThreadExecutor.*`: runs handlers on the game thread with a timeout. `LockRegistry.*`: edit locks.
- `Handlers/<Category>/`: the actions themselves (Animation, Asset, Blueprint, Gas, Level, Material, Niagara, PCG, Sequencer, StateTree, ...).
- `Tests/`: automation tests. `Public/`: helpers (`HandlerUtils.h`, `HandlerParams.h`, `HandlerResult.h`, ...) and the external API `MCPHandlerRegistration.h`.

## Dependencies

- Plugins (all `Enabled`, from the `.uplugin`): PythonScriptPlugin, DataValidation, EditorScriptingUtilities, EnhancedInput,
  GameplayAbilities, GameplayTagsEditor, Niagara, PCG, IKRig, ControlRig, PoseSearch, Chooser, PropertyBindingUtils, Metasound,
  Synthesis, StateTree, and Fab (the only `Optional` one).
- Modules: a very large private list in `UE_MCP_Bridge.Build.cs` (editor modules for animation, Blueprint, Niagara, PCG, UMG, ...,
  plus `GameplayTags`, `GameplayAbilities`, `GameplayTagsEditor`). Fab is linked only if the engine has its header (`WITH_FAB_PLUGIN`).
- Used by: `GASP_Giustino2.uproject` lists `UE_MCP_Bridge`. No project module links to it.

## Configuration and start-up

- The bridge starts by itself when the editor loads the plugin (`StartupModule`); there is nothing to enable in-game.
- `ue-mcp.yml` at the repo root is read by the Node client, not by C++: `contentRoots: [/Game/]`, `disable: [feedback]`, empty `tasks`/`flows`.
  A `ue-mcp.local.yml` is git-ignored for per-machine overrides.
- Port resolution order (`BridgePortConfig.cpp`): `-MCPPort=N` on the command line, `UE_MCP_PORT` environment variable, the client's
  `Saved/UE_MCP_Bridge/requested.json`, `ue-mcp.bridge.port` from the layered config, then a port derived from the project path.
  If the port is taken the bridge probes up to 50 higher ports. The chosen port is written to state files under `Saved/UE_MCP_Bridge/`
  so the client can find it. The log line to grep for is `[UE-MCP] Bridge listening on ws://127.0.0.1:<port>`.
- The MCP server side is installed and run outside this repo. No MCP server entry for it exists in `.mcp.json`.

## Security

The server binds to loopback only (`INADDR_LOOPBACK`, `127.0.0.1`) and has no authentication on the WebSocket upgrade;
`BridgeWebSocket.cpp` rejects any upgrade that carries an `Origin` header (browsers). Anything running on the same machine as the
editor can still call every handler, including ones that write and delete assets. Do not change the bind
address, do not forward the port, and do not run this plugin on a shared machine.

## How to add a handler

1. Pick or create a category folder under `Source/UE_MCP_Bridge/Private/Handlers/`.
2. Write a static function `TSharedPtr<FJsonValue> Fn(const TSharedPtr<FJsonObject>& Params)` in an `F<Name>Handlers` class
   (see `Handlers/Demo/DemoHandlers.h` for the shape). Use the helpers in `Public/` for parameter reading and results.
3. In its `RegisterHandlers(FMCPHandlerRegistry&)`, call `Registry.RegisterHandler(TEXT("method_name"), &Fn, { /* FMCPParamSpec list */ })`.
   `RegisterHandlerWithTimeout` is available for long operations. Handler specs are checked by `Tests/HandlerSpecContractTests.cpp`.
4. For a new category, include its header and call its `RegisterHandlers` from `HandlerCatalog.cpp`.
5. Add an automation test under `Private/Tests/`. After adding a new source file, bump the rescan counter comment in
   `UE_MCP_Bridge.Build.cs` so UBT rescans.

Handlers from another plugin do not need to edit this one: add `UE_MCP_Bridge` to that plugin's private dependencies and call
`UEMCP::RegisterExternalHandler(...)` from `StartupModule` (and `UnregisterExternalHandler` on shutdown); see `Public/MCPHandlerRegistration.h`.
Prefer this route for project-specific actions. The API versions are `UEMCP_BRIDGE_API_VERSION 2` and `UEMCP_BRIDGE_PROTOCOL_VERSION 2`.

## Third-party code: keep changes minimal

This is vendored upstream code (single baseline commit in git, no project-specific edits identifiable). Avoid editing it; upgrading
by replacing the folder should stay possible. If a change is unavoidable, keep it small, mark it with a comment such as
`// Project: <reason>`, and list it in this README under a "Local changes" heading.

## Standalone use

Copy the `UE_MCP_Bridge` folder into another project's `Plugins/` and make sure the plugins listed under Dependencies are available
(they all ship with UE 5.x). Nothing GAS-NPP-specific is required. Run the UE-MCP Node server separately to talk to it.
