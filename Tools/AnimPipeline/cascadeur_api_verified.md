# Cascadeur 2026.2.3 Python API: verified calls

Tested on 2026-10-30/31 against `jab_phys_test.casc` (a throwaway copy of `jab_v5.casc`) through the script server.
Status per entry: **VERIFIED** (ran and the data changed as expected), **PARTIAL**, **PENDING** (not tested yet), **FAILS**.

## 0. Running code in Cascadeur

- The script server must be started by hand once per Cascadeur session: `Commands > MCP > Start script server`
  (there is no auto-start; `scripts/mcp/start_server.py`). It listens on `127.0.0.1:8765`.
- `POST /run` with JSON `{"code": "<python>"}`; `scene`, `csc`, `app` are in scope; `print()` output comes back in `messages`.
  Runner: `python -I Saved/AnimPipeline/csc_run.py <script.py> NAME=value ...` (prepends assignments, exits 1 on errors).
- Scripts run on the next `scene_idle`; a failing `modify*` callback can still return `ok: true` over HTTP: always check the
  bool returned by `modify*`, and read `%LOCALAPPDATA%\Nekki Limited\Cascadeur\logs\cascadeur_log.log` on `False`.

## 1. Tabs and files (VERIFIED)

```python
app = csc.app.get_application(); sm = app.get_scene_manager()
app.get_data_source_manager().load_scene(path)        # opens a new tab
tab = [s for s in sm.scenes() if s.name() == "x.casc"][0]
sm.set_current_scene(tab)                               # actions and the FBX exporter follow the CURRENT tab
tab.save(path)                                          # save-as RENAMES the tab to the new file
ds = tab.domain_scene()                                 # model_viewer / data_viewer / behaviour_viewer / layers_viewer
app.get_data_source_manager().close_scene(tab)          # close a tab (not yet exercised)
```
Pitfall: `src.save(new_path)` on an open tab renames that tab; the original file on disk keeps its last saved state.
Copy the file on disk and `load_scene` the copy when you need the source tab to stay as it is.

## 2. Selection, current frame, timeline interval (VERIFIED)

```python
ds.set_current_frame(f)
ds.selector().select({ids[n] for n in names})          # object selection (set of ObjectId)
lv = ds.layers_viewer(); layer_ids = list(lv.all_layer_ids())
ds.get_layers_selector().set_full_selection_by_parts(layer_ids, first, last)   # timeline cells first..last
```
`set_full_selection_by_parts` opens its own undo session: call it OUTSIDE `modify*`, otherwise
`RuntimeError: SessionControl::getSession; session exists`.
Read back: `sorted({c.frame_index for c in ds.get_layers_selector().selection().cell_indices()})`.

## 3. Actions: `app.get_action_manager().call_action(key)`

Keys exist in the binaries for all of these. Effects are applied synchronously unless noted.

| Key | Needs | Status | Effect |
|---|---|---|---|
| `Timeline.Bezier viscous.Bezier viscous on selected interval` | timeline interval selected | **VERIFIED** | sections in the interval become `LOW_AMPLITUDE_BEZIER`; neighbouring in-betweens move too (tangents) |
| `AutoPhysicsTool.Switch Auto Physics` | current tab | **VERIFIED** (toggle) | no data change by itself; enables the tools below. Python off switch: `tm.get_tool("AutoPhysicsTool").editor(tab).turn_off()` |
| `AutoPhysicsTool.Set priority frame` | AutoPhysics ON, current frame | **VERIFIED** | sets `AutoPhysics.priority_frame` (on object `Center of mass`) at the current frame; does nothing while AutoPhysics is off |
| `AutoPhysicsTool.Snap to Auto Physics` | AutoPhysics ON | **VERIFIED with PySide (6b)** | the call alone does nothing visible: it opens the "Apply AutoPhysics" dialog window (All Frames / Only keys) and waits; pressing its button from PySide applies the snap |
| `AutoPhysicsTool.Switch Frozen Auto Physics` | AutoPhysics ON | runs, no data change | freezes the physics ghost; effect only visible in the viewport |
| `AutoPhysicsTool.Show all fucrum points` (sic) | - | not tested | display only |
| `TweenMachine.Attract to inertial position.Selected frames` | objects selected + timeline interval | **VERIFIED** | moves the KEYS inside the interval (no new keys) toward the inertial position; with the lead hand selected on 5-12: hand up to 4.4 cm, elbow 3.6 cm, in-betweens 4-16 follow |
| `View.FixFoot_KeepKeyFrames` | foot/ball points selected + interval | **VERIFIED** | re-pins fulcrum feet keeping keys; on already planted feet it moved them <= 0.02 cm (safe final cleanup) |
| `View.Live Link (Start UE connection)` | current tab | runs | toolbar toggle of LiveLinkTool; the actual link (root/skeleton/AnimSequence) is chosen in the Unreal Engine Link window (UI) |

Other existing keys: `Timeline.Bezier clamped.Bezier clamped on {current frame|selected interval}`, `Timeline.Bezier.Bezier on ...`,
`TweenMachine.Attract to {previous|next|average|interpolation|local interpolation|inertial|inverse inertial} position.{Current frame|Selected frames}[ (hard)]`.

Direct alternative for interpolation (no selection needed), as in the pass scripts:
```python
I = csc.layers.layer.Interpolation   # BEZIER=0, LOW_AMPLITUDE_BEZIER=1 (viscous), LINEAR=2, STEP=3, FIXED=4, NONE=5, CLAMPED_BEZIER=6, AI=7
def interp(model, update, su):
    le = model.layers_editor()
    for lid in layer_ids:
        le.change_section(f, lid, lambda s: setattr(s.interval, "interpolation", I.LOW_AMPLITUDE_BEZIER))
ds.modify_update("interp", interp)
# read: lv.layer(lid).interval(f).interpolation
```

## 4. AutoPhysics data on the UEFN mannequin rig (read and write VERIFIED)

The rig has a `Center of mass` object (behaviours `CenterOfMass`, `AutoPhysics`) and `FulcrumGroup`s on both feet
(foot + ball points), both balls and both hands. Every point controller (43) carries:

| Behaviour.property | Rig value now | Meaning (docs) |
|---|---|---|
| `FulcrumPoint.fulcrum_state` | 0 on every frame | enum `FulcrumState`: Fulcrum (default, auto-detected) / Enforce / NotFulcrum, set per keyframe |
| `FulcrumPoint.collision_radius`, `max_speed` | 5 (pelvis 13), 1 | detection thresholds |
| `CompensationMotion.muscle_stiffness` | hands/feet 5, pelvis 100 | 100 = follow the animation; 5-50 = takes part in compensation |
| `SecondaryMotion.local_blending` / `global_blending` / `damping` / `air_friction` | 10 / 0 / 10 / 0 (balls 100/0/0/0) | docs default local_blending 100 (= follow); low = loose. Reset to 100 except wrists/hands before using Secondary |
| `SeparationOfMotion.muscle_stiffness` | 100 | |
| `AutoPhysicsApply.keep_global_rotation` / `keep_global_translation` | 0 / 0 | 0-100 |
| `AutoPhysics.priority_frame` (on `Center of mass`) | False | per frame |

Read:
```python
bv = ds.behaviour_viewer(); dv = ds.data_viewer()
b = bv.get_behaviour_by_name(ids["hand_MainPoint_r"], "CompensationMotion")
d = bv.get_behaviour_data(b, "muscle_stiffness"); dv.get_data_value(d, frame)
```
`FulcrumPoint.fulcrum_state` values: **0 = Fulcrum** (auto-detected, default), **1 = Enforce**, **2 = NotFulcrum**
(read from the description `Enum FulcrumState`: `dv.get_description_value(dv.get_standard_description_id_by_name("Enum FulcrumState"))`).

Write (VERIFIED 2026-10-31: muscle_stiffness 5->15, local_blending 10->30, damping 10->5, fulcrum_state 0->1 on all frames,
priority_frame True on frames {1, 3, 17} only; all data on this rig is `DataMode.Animation`, i.e. per frame):
```python
def mod(model, update, su):
    de = model.data_editor()
    if dv.get_data(d).mode == csc.model.DataMode.Static:
        de.set_data_value(d, value)
    else:
        de.set_data_value(d, set(range(dv.get_animation_size())), value)   # or (d, frame, value)
ds.modify("set stiffness", mod)
```

## 5. AutoPhysics panel toggles (read-only from Python)

`tab.get_setting_handler().get_bool_value("AutoPhysics", "Secondary motion")` (also `"Compensation motion"`,
`"Separation of motion"`, `"Smooth rotation"`, `"Smooth trajectory"`; floats `"Secondary alpha"` etc.) reads the values
of THAT scene: they survive a Cascadeur restart and differ from the app ini, so they are stored per scene (jab_v5: all True,
alpha 50). There is no setter. `app.get_setting_manager().get_*_value(key)` returns 0/False for every key (useless).
`%LOCALAPPDATA%\Nekki Limited\Cascadeur\Cascadeur_tools.ini` `[AutoPhysics]` holds app defaults, rewritten on exit.

## 6. Live Link to Unreal (VERIFIED 2026-10-31, plugin CascLiveLink 1.0.2 on UE 5.8.4)

- The UE side needs the Fab plugin enabled in the project AND listed in the editor target receipt the editor picks
  (`Binaries/Win64/*.target`, first editor receipt in folder order). A stale `GameAnimationSampleEditor.target` hid it
  (renamed `.target.stale`); rebuild `GASP_Giustino2Editor Win64 Development` after enabling a plugin as Optional.
- Data is exact: the streamed AnimSequence matched the FBX import of the same scene within 0.01 cm / 0.05 deg on every bone
  and frame (60 fps, 38 frames, root identical).
- Unreal rebuilds the AnimSequence 0.14-0.26 s after a Cascadeur modify; edits on non-key frames are no-ops (key first).
- The AnimSequence is created dirty in memory and must be saved in Unreal by hand (or duplicated and saved elsewhere).
- Reading it from scripts works as for any AnimSequence (`sample_pose`, `visual_check.py`).

## 6b. Snap to AutoPhysics through PySide (VERIFIED 2026-10-31, on the copy jab_snap_test.casc)

Cascadeur 2026.2 ships PySide6 6.5.3; its UI is pure QML (a `QGuiApplication`, no QWidgets), so dialogs are
`QQuickWindow`s. `AutoPhysicsTool.Snap to Auto Physics` opens a non-blocking window titled **"Apply AutoPhysics"**
("Which frames should AutoPhysics results be applied to?") with two `DialogButton`s: **"All Frames"** and **"Only keys"**.
Scripts keep running while it is open (that is why earlier scripted snaps "did nothing": the dialog was waiting).
Recipe (`snap_dialog_click.py`): with AutoPhysics ON and the timeline interval / objects selected, call the action, then
in the same script or later:
```python
from PySide6 import QtCore, QtGui, QtQuick
import shiboken6
for w in QtGui.QGuiApplication.topLevelWindows():
    qw = shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow)   # PySide returns plain QWindow
    if w.isVisible() and qw.title() == "Apply AutoPhysics":
        stack = [qw.contentItem()]
        while stack:
            it = stack.pop(); stack.extend(it.childItems())
            if it.property("text") == "All Frames" and "DialogButton" in it.metaObject().className():
                QtCore.QMetaObject.invokeMethod(it, "clicked")   # "click" is not invokable; emitting clicked works
```
Result on the copy (AutoPhysics on, all panel toggles on in that scene): the snap applied to all frames, pelvis/chest moved
up to ~19 cm, hands ~22-25 cm, feet unchanged, dialog closed. Each un-answered snap call leaves one dialog window open:
answer it (or close it) before calling again.

## 6c. AutoPhysics panel toggles (Secondary / Compensation / Separation / Smooth rotation / Smooth trajectory)

- They are NOT scene data or scene settings: no object in the scene has a setting with those names
  (`data_viewer.get_all_settings_id` over every object), so `data_editor.set_setting_value` has nothing to write.
- They ARE per scene: `tab.get_setting_handler().get_bool_value("AutoPhysics", ...)` reads True in jab_v5/v7 while the app
  ini says false, and the value survives a restart. The ini (`Cascadeur_tools.ini`) holds app defaults, rewritten on exit;
  editing it (with Cascadeur closed) can only change the defaults, not an existing .casc (not tested, by design).
- UI route (partial): the toggles are custom QML rows (Text + QQuickImage + QQuickMouseArea) in the "Physics settings" side
  tab. A synthetic click on a row with the label "Secondary motion" hit the per-point "Secondary motion" section header in
  Object properties instead (same text), and switching the side tab programmatically failed (the tab bar is a
  QQuickListView; restored afterwards). Not solved: set these toggles by hand once per scene (they persist in the .casc),
  or extend the PySide approach to target the Physics settings panel by its parent item.

## 7. Not scriptable without the UI

- Starting the script server (`Commands > MCP > Start script server`).
- (Snap to AutoPhysics: now scriptable through PySide, see 6b.) The Unreal Engine Link window choices (root / skeleton / AnimSequence), adding the
  `Casc Live Link` source in Unreal (no Python API for it).
- `DataSourceManager.close_scene(tab)` on a modified tab opens a QML window "Closing scene" (Cancel / Don't Save /
  Save as... / Save) and waits. Pressing **Don't Save** from PySide (emitting `clicked`) made Cascadeur EXIT on
  2026-10-31: do not answer that dialog from a script.
- AI Inbetweening motion generation: action `View.Inbetweening_RunMotionGeneration`; style = scene setting
  `RootMotionStyles` (0 Any, 1 Walk, 2 Run, 3 Ground, 4 Jump, 5 Fall, 6 Acrobatic, 7 Combat), no Python setter.
  See ai_tools_local_check.md.
