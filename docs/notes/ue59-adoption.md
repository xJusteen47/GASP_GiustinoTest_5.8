# UE 5.9 release notes: checks and adoption (2026-11-19, desktop, no editor, branch claude/ue59-adoption-1)

Installed engine: `UE_5.9` 5.9.0, CL 60738873 (`Engine/Build/Build.version`). Project checked: `GASP_Giustino2_ue59` (uproject EngineAssociation 5.9).
Offline scans: `_scratch/ue59_scan.py` and `_scratch/ue59_scan2.py` (outside the repo, results in `_scratch/ue59_scan*.txt`).
Editor-only checks: `Tools/UE59/ue59_editor_checks.py` (read-only, not run yet, see "For the editor thread").

## 1. Checks (what could break us)

| Item | Result | Action |
|---|---|---|
| Python Foundation Packages removed | The installed 5.9 still ships `Plugins/Experimental/PythonFoundationPackages` (empty site-packages; only OpenCV, AnimGen, PythonMLPackages, UnrealCanvasBridge depend on it, none enabled by us). No in-editor script of ours imports numpy/PIL/cv2/etc.: scan2 section 1 is empty (files with `import unreal` + a third-party import). numpy/PIL/matplotlib/PySide6 are used only by out-of-editor tools (AnimPipeline, Cascadeur, ffmpeg helpers) with their own Python. | None. If a future in-editor tool needs numpy, pip-install it into the project, do not rely on that plugin. |
| `init_unreal.py` only for plugins with loaded modules | We have no `init_unreal.py` anywhere (Content, Plugins). Our startup hook is the project setting `StartupScripts` in `Config/DefaultEngine.ini:220` (`import hub_startup`), which does not depend on plugin modules. | None. Rule for new plugins: no content-only plugin with `init_unreal.py`; use StartupScripts or a plugin with a module. |
| MCP Tool Library removed | Not in the 5.9 engine and no reference in our code/config (scan1 section 3). Our clients use `http://127.0.0.1:8000/mcp` (`.mcp.json`, `Tools/AnimPipeline/ue_builtin.py`) and our own `UE_MCP_Bridge` + `GSTNAnimToolsets`. | None. |
| New MCP settings | `UModelContextProtocolSettings` is per-user (EditorPerProjectUserSettings): `ServerUrlPath`, `ServerPortNumber`, `bAutoStartServer` (default false), `AdditionalAllowedOrigins`, `MaximumProtocolVersion`, `bEnableToolSearch`, `bDumpMediaToDisk`, `Instructions`. A second, project-level class (config=Game) holds only project `Instructions`. Nothing set in our Config or Saved ini: defaults (port 8000) match our clients. Curl sends no Origin header, so the origin list does not affect us. | Optional: put a short project Instructions text (our toolsets, "write python output to a file", "never save in PIE") in Project Settings > Model Context Protocol. Ready text in §3. |
| AllToolsets on by default, restrict via Toolset Registry | `UToolsetRegistrySettings` (config=Game) has `BlockedNames`/`AllowedNames` (+ AgentSkill and SetObjectProperties block lists). We enable AllToolsets explicitly in the uproject. Nothing restricted now. | Recommended: block only risky toolsets, not an allow-list (agents use many). Proposed block list in §3, to confirm after the editor dump. Editor tooling only, not gameplay. |
| Control Rig deprecated nodes | 201 deprecated RigUnit/RigVM structs in 5.9 headers. Byte scan of our 94 rig assets: 16 contain candidate names (substring match, may over-report). Our own ones: `CR_WallIkControlRig` (both copies: AimBone), `CR_Biped_FootPlacement` (AimBone, GetRelativeTransform, TwoBoneIKSimple, DebugArc), `CR_Echo_Twist`, and the GASP mannequin rigs (`CR_Mannequin_Body/BasicFootIK/Procedural`). The rest is MetaHuman/sample content. Deprecated nodes still load and run. | Exact list from the editor script. Then upgrade nodes only on rigs we own (WallIk, Biped_FootPlacement) in a code thread; leave Epic sample rigs alone. |
| Platform default frame-rate cap | No frame-rate key changed between 5.8 and 5.9 shipped ini (Engine/Config + Windows): diff empty. `bSmoothFrameRate=false` as before. Ours: `GameUserSettings FrameRateLimit=0` (no cap), no `t.MaxFPS`. NP fixed tick stays 62.5 Hz. | Runtime values (`t.MaxFPS`, vsync, GameUserSettings) come from the editor script. No settings change. |

## 2. Adoption

| Item | Finding | Recommendation |
|---|---|---|
| UAF "Play Niagara Effect" notify | `UUAFNotify_PlayNiagaraEffect` (plugin UAFDefaultNotifies, depends on UAF) derives from `UUAFAnimNotify`: it fires only when the sequence is played by UAF. Our locomotion is ABP + Motion Matching, UAF not enabled. Our dust (run stop, landing) is spawned from gameplay events (`RunStopPlant`), which is rollback-friendly. | Do not adopt now. If an anim-driven effect is wanted, the classic `AnimNotify_PlayNiagaraEffect` (NiagaraAnimNotifies) already works with ABP. Note sent to the VFX thread via the coordinator. |
| Chooser editing from Python | `UChooserTableEditorLibrary` has 66 `ScriptMethod` functions (types, signature, columns, rows). Our `GSTN_ChooserTableToolset` already wraps it. | Adopt in Python tools directly (`unreal.ChooserTableEditorLibrary`) instead of new C++ wrappers. Keep the context-class fix from memory (copied GASP choosers). |
| Pose Search schema roles | Roles belong to multi-character Pose Search Interactions. We have 5 interaction/multi-anim assets (e.g. GASP `PSD_Interaction_Shove`), 15 assets referencing schemas. | Useful later for grabs/throws (GSTNLink) and sync attacks; nothing to do now. |
| Python Blueprint / StateTree editing | `BlueprintDslToolset` (Read/Write graph DSL) and StateTree editing (`UStateTreeEditingSubsystem`: compile, validate, mark modified) exist; editor script lists what Python sees. We do not use StateTree in gameplay. | Blueprint: use the DSL toolset for plain BPs (still refuses AnimBP, use GSTN_AnimBlueprintToolset). StateTree: skip until a feature needs it. |
| UBA Cloud | Remote build helpers via Horde/cloud agents. One developer, two machines, builds are RAM-bound (65/67 GB) rather than CPU-bound. | Do not adopt (assessed only, no account). If anything, local UBA between desktop and laptop is the cheaper first step. |

## 3. Ready snippets (not applied)

Toolset Registry block list (Project Settings > Toolset Registry, writes DefaultGame.ini), confirm names against the dump:
```
[/Script/ToolsetRegistry.ToolsetRegistrySettings]
+BlockedNames=CheatsToolset
+BlockedNames=MetaHumanCalibrationProcessingToolset
+BlockedNames=MetaHumanDepthGeneratorToolset
+BlockedNames=CaptureManagerToolset
```
MCP project Instructions: "GASP_Giustino2. Prefer GSTNAnimToolsets for AnimBP and choosers. Python print() is not returned: write to a file under Saved/. Never save assets or set component properties during PIE."

## 4. For the editor thread
Run `Tools/UE59/ue59_editor_checks.py` once in the open editor (MCP execute_python with the file content, or `py "<path>"` in the Output Log). Read-only, about a minute (loads ~20 rigs). Output: `Saved/UE59/ue59_editor_checks.txt`. Copy it to `docs/notes/ue59-editor-checks.txt` on this branch.

Then run `Tools/UE59/ue59_ask_assistant.py` in the same open editor (owner: the Editor AI assistant is free, use it to confirm fit). It asks the 6 open questions (UAF notify, Control Rig replacements, frame-rate cap, toolset restriction, Pose Search roles, Python BP/StateTree editing) and writes `Saved/UE59/assistant_replies.txt` (ends with `== DONE ==`). Copy it to `docs/notes/ue59-assistant-replies.txt`; where a reply contradicts §1-2, the reply wins after a check.
