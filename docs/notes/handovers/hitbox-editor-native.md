# Hitbox editor: native feel, Details per hitbox, rename, parenting (cloud, 2026-11-16)

Branch `claude/project-thread-jytbuj` from `claude/project-thread-ibkcds` b9deccc5 (H1, which contains integ-cp3 069287f7). Cloud, Opus. **NOT COMPILED** (no UE in cloud).
Owner asks (thread 2026-11-16): native icons, better buttons, horizontal zoom, hitbox keys buried in Details arrays, parenting group/hitbox, rename.

## What changed
- **Toolbar**: engine slim toolbar (`FSlimHorizontalToolBarBuilder`, style AssetEditorToolbar) in sections transport / frame / keys / view, FAppStyle icons
  (`Animation.*` transport). Each icon has fallbacks (`SGSTN_TimelinePanel::ResolveToolbarIcon`); the test warns on any missing brush. New **View** menu: zoom, frame all/selection, ruler mode, fit length.
- **Ruler**: frames + seconds by default (`EGSTN_RulerDisplay`, modes Frames+Seconds / Frames+ms / Frames / Seconds, saved per user in EditorPerProjectUserSettings, not project settings).
- **Zoom/pan**: wheel over the ruler zooms, Ctrl+wheel zooms anywhere, Shift+wheel scrolls; middle/right drag pans; horizontal scrollbar under the lanes; Ctrl+= / Ctrl+- zoom.
- **Shortcuts (Sequencer-like)**: Left/Right step, `,` `.` prev/next key (were Shift+, .), Up start, Ctrl+Up end, Space play, F frame selection, Home frame all, K key.
- **Details**: a selected hitbox key shows three stacked views: Track/Group (without the Hitboxes array), **Hitbox** (that hitbox only, Keys hidden), **Key** (offset/rotation/scale of the selected key, header gives timeline frame). One undo step per edit; keys re-sorted if a frame is typed.
  Reading confirmed: "dal viewport non posso modificare i numeri, solo la posizione" = the gizmo only drags; the Key/Hitbox views give the numbers.
- **Rename**: `FGSTN_HitboxSpec::Name`, `FGSTN_TimelineTrack_Hitbox::GroupName` (labels only, nothing looks them up; hits use indices). Hitboxes tab: F2, double-click, click on selected name, right-click Rename. Shown in tab, lane label, Details header.
- **Hitboxes tab** right-click: Add Hitbox (group), Duplicate (Ctrl+D), Delete (Del, last one stays). Icons per row.
- **Parenting** (data, runtime and editor): group gets `bUseGroupTransform` (default off = old behaviour), `GroupSpace` (Root/BakedSocket/WorldAtMoveStart), `GroupSocket`, `GroupOffset`; hitbox gets `bAttachToGroup` (default on). Child placement = Keys * GroupOffset * group space. Used in `GSTN_CombatTick_Attacker` BuildCarrier, minion BuildCarrier, gizmo math/target, bake (group socket baked), both validators, outline. Selecting a group row/bar with a transform puts the gizmo on the group: moving it moves all children. Group offset is static (not keyed): animated group path = future.
- Old assets load unchanged (new UPROPERTYs only, defaults keep old placement). Setup scripts untouched (H1 guard still applies).

## Desktop build and test (Haiku low, ~3 $)
1. Close the editor. Build `GASP_Giustino2Editor` Win64 Development with the timed script (45 min). Compile errors: fix max 2 tries, else stop and write them here.
2. Headless: `GSTNCombat.Editor.HitboxNative` (4 tests) + `GSTNCombat.Editor.Timeline` + `GSTNCombat.Editor.HitboxOutline` + `GSTNCombatNP` attacker tests. Report missing-icon warnings.
3. Open TL_Overlord_Jab1, screenshot toolbar, Hitboxes tab, Details with a key selected. Don't save maps; revert IC_/KIT_ dirtying.

## Open
- Hurtboxes are root-relative from the reference pose (`GSTN_HurtboxTypes.h` FGSTN_Hurtbox comment): they do not follow the animation; no per-move hurtbox extension exists (HurtboxModifier.ShapeOverrides is data only). Owner decision pending.
- H2 (victim dummy, trail, anim-frame ruler) and H3 still queued.
