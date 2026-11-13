# Hitbox / timeline editor: UX and UI improvement plan

Status: plan, 2026-11-13. Owner request: better UX and UI in general, icons, a way to resize the graph. Base: branch `claude/project-thread-wif3ec` (e1f7e8e9).
Scope: plugin `GSTNCombatEditor` only (Slate, editor only). No runtime change. Designer rules apply: no manual Blueprint wiring, no hardcoded inputs, no project settings for gameplay. Every size/colour knob goes in `UGSTN_CombatEditorSettings` (editor prefs) or is saved per user, never in the asset.

Current state (read from code): layout `GSTN_TimelineEditor_Layout_v2` (viewport 45% over timeline 55%, right column Hitboxes 30 / Details 40 / Frame Data 30); fixed `FGSTN_TimelineLayout::RowHeight` and `GSTNTimelineWidgets::NameColumnWidth`; ruler and phase bar fixed heights; one icon per track type from `FAppStyle` (`Icons.Next`, `Icons.Transform`, `Icons.Details`, `Icons.Event`, `Icons.Convert`, `Icons.Target`, `Icons.Lock`); hitbox rows are text only.

## Priorities (P1 = do first; cost is for one Opus code thread, medium)

| # | Item | Why (designer view) | Where | Size |
|---|---|---|---|---|
| P1.1 | Resizable rows and name column: drag the border under a row / the border of the name column; Ctrl+wheel over the name column = row height; reset button. Saved per user. | The graph and sub-lanes are too small to click keys apart. | `FGSTN_TimelineLayout::RowHeight` becomes a model value; `SGSTN_TimelineTrackRow`, `SGSTN_TimelineLane`, `SGSTN_TimelinePanel` | M |
| P1.2 | Vertical zoom of the lanes (Alt+wheel) and "Fit rows" button; horizontal zoom slider in the toolbar (today only Ctrl+wheel, Home, F). | Resizing the graph, owner ask. | panel toolbar, `FGSTN_TimelineViewTransform` | S |
| P1.3 | Tabs that the designer can drag-resize are already splitters; add a "Reset layout" and layout presets in the Window menu: *Edit hitboxes* (big viewport, small timeline), *Timing* (big timeline), *Review* (all equal). Bump to layout v3. | Panels get lost; today only the editor's generic reset exists. | `MakeDefaultLayout`, toolkit menu | S |
| P1.4 | Clearer selection: selected bar gets a 2 px bright outline plus a handle strip; selected key is larger and filled; the Details tab header says what is selected ("Hitbox 2 of Group 1, key at frame 14"); the outliner row, the lane and the viewport shape use the same highlight colour. | Today it is hard to tell which hitbox a gizmo is on. | `SGSTN_TimelineLane::OnPaint`, `SGSTN_TimelineDetails`, `SGSTN_HitboxOutliner`, `GSTNHitboxOverlay` | M |
| P1.5 | Hitbox colour per `HitGroup`, shown as a swatch in outliner rows, lane sub-lane tint and viewport shape (the overlay already colours by group). | One colour language everywhere. | outliner, lane | S |
| P2.1 | Icons: distinct icon per track type (current ones are generic engine brushes) plus icons in the toolbar (play, step, loop, auto-key, bake, validate, fit) and on the outliner (group, hitbox, shape sphere/capsule/box, space root/socket). Ship a small own style set (`FSlateStyleSet`, SVG 16/20 px) in the plugin so it does not depend on engine brush names that change between versions. | Faster reading, less text. | new `GSTN_EditorStyle.*`, `GetIconBrushName` of each track editor, toolbar | M |
| P2.2 | Row and bar tooltips with the frame range, duration in frames and ms, and what the row does in one Italian-friendly plain sentence; Italian-friendly names are loc strings, not hardcoded. | Designers do not read the Details tab to learn. | `SGSTN_TimelineTrackRow`, track editors | S |
| P2.3 | Outliner: rename a group (display name field on the track, editor-only), show/hide (eye) and solo a hitbox in the viewport, drag to reorder, right-click: duplicate, delete, mirror. Search box. | The outliner is read-only today. | `SGSTN_HitboxOutliner`, model, `GSTN_HitboxOutline` | M |
| P2.4 | Findings next to the thing: a red/yellow dot on the row, key or outliner row that a validation finding points at; click selects it. | Validation text in a list is not tied to the row. | `SGSTN_FindingsList`, lane, outliner | M |
| P2.5 | Move-end helper: in the phase bar show End Blend Out and Hold Pose as a draggable blend band after the last frame, with a tooltip; one-click "Cut at last frame" (EndBlendOutFrames 0 + Hold Pose). | The "move ends before the second punch" problem is the owner's most recent. | `SGSTN_PhaseBar`, model | S |
| P3.1 | Snapping and rulers: snap to frame already; add snap to other bars' edges (toggle), a frame number readout while dragging, ghost of the old position. | Precision while dragging. | `FGSTN_TimelineGesture` | S |
| P3.2 | Viewport: a toolbar with big icon buttons for move/rotate/scale, camera presets, show-all-hitboxes toggle; the 0 to N frame scrub slider under the viewport; onion skin of hitbox positions on neighbouring frames. | Edit and scrub without the timeline. | viewport client | M |
| P3.3 | Details tab: show only the fields that matter first (collapsed "Advanced"), hit data summary line on top, hide fields whose EditCondition is off. | Less noise. | `SGSTN_TimelineDetails`, customization | S |
| P3.4 | Theme and text size: use editor font scale, colour-blind-safe track palette option (`TrackColors` already per user). | Accessibility. | settings | S |

## Suggested order for a code thread
1. P1.1 + P1.2 + P1.3 (resize), one thread, Opus medium, estimate $8-12.
2. P1.4 + P1.5 + P2.2 (selection), $6-10.
3. P2.1 icons, $6-10 (needs art: use engine SVG-like drawn primitives or ask owner for icon art).
4. P2.3, P2.4, P2.5 as owner prioritises, $8-12 each.
P3 only if time. Each thread: Slate-free logic stays testable in headless `GSTNCombat.Editor.*` tests (as for the outliner); Slate files stay thin. Ends with desktop `_ue59` on latest and built.

## Decisions for the owner (only if he wants)
- Row height and panel sizes: save per user (recommended) or per asset. Recommended per user: assets stay clean.
- Icons: own small set (recommended, one thread) or keep engine icons.
