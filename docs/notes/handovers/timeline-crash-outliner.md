# Timeline editor crash, montage end, hitbox outliner (cloud, 2026-11-13)
Branch `claude/project-thread-wif3ec` from `79454ebc`. NOT COMPILED (cloud): build on desktop `_ue59`, then headless tests.
**(a) Crash** (`PairId.IsValidId` in `GSTNInlineTimeline::OpenEditor`): a second "Open Timeline Editor" on a target whose editor was
already open built a toolkit, the lease refused it, and the toolkit was destroyed without `InitAssetEditor`: UE 5.9 asserts in that
destructor. Fix: `FGSTN_TimelineEditorToolkit::OpenFor` focuses the open editor (or refuses) BEFORE building a toolkit; all 3 openers use it.
**(b) Montage past the timeline**: the timeline does end at `TotalFrames` and stops the montage, but the fork stopped the mesh's instance
with its own blend-in alpha (~0.25 s) while the animation kept playing: the next punch showed under the blend. Also `Tail.BlendOutFrames`
was read nowhere at run time. Fix (presentation only, sim untouched): new timeline fields `EndBlendOutFrames` (-1 = as before, 0 = cut,
N = frames) and `bHoldPoseOnEnd` (freeze the last pose during the blend); a cancelled tail now blends over `Tail.BlendOutFrames`.
Fork: `UNpAbilitySystemComponent::StopSyncedMontageWithBlend` / `UNetMontageSimulator::StopMontageWithBlend` (pending stop applied in
`FinalizeMontageFrame`). `UGSTN_GA_BaseCombat::ResolveMontageStop` picks end / tail / default. Simulated proxies keep the default blend.
Behaviour change: moves WITH a tail now blend out in 6 frames (the field's default) on a tail cancel instead of ~15.
**(c) Hitbox outliner** (new "Hitboxes" tab, layout v2): tree of groups (Hitbox tracks) and their hitboxes with shape/space/frames.
Row click = select + gizmo + playhead into the group's frames; viewport click = row selected, expanded, scrolled into view.
Logic Slate-free in `GSTN_HitboxOutline.h`; widget `SGSTN_HitboxOutliner`. Old saved layout v1 is replaced by the default once.
**Tests added** `GSTNCombat.Editor.HitboxOutline.*`, `GSTNCombat.Editor.Registry.OpenForBuildsNoToolkitForAHeldTarget`; layout test now v2.
**Owner step to fix ESEMPIO strong**: in GA_Example_Strong set TotalFrames before the second punch, End Blend Out Frames ~4, Hold Pose On End on.
**Not verified**: compile, the tab in a real window, the blend in game. Designer guide artifact: add the 2 fields + the Hitboxes tab.
Cost: ~120k context, Opus, no subagents; estimated ~$10-14.
