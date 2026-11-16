# Traversal leftovers: T2 close-out (laptop, 2026-11-06)

Branch claude/project-thread-wlggpb from be6ff01 (R3-1). No code change.

T2 was listed as "paused" in resto-m3-m4b-m5-t.md, but that audit read the commit 4041401 ("T2, paused"). The follow-up 447fc2d
(already on this branch) ran every check the pause had left open: surface scenario at 2 % loss, `wall_ab.py` moving lanes, the v2 wall
scenarios at 0 % and 2 %; design and numbers in wall-slide-v2.md 6.15 and traversal-doc-deltas.md "T2". The plan's class
`UGSTN_SurfaceWallEntryTransition` and tests `GSTNMovement.Wall.Surface.*` were replaced by `UGSTN_WallReentryUtils::Judge` and its tests (documented).

Re-check today on the current code (build up to date):
- `Automation RunTests GSTN.Wall+GSTNMovement.Wall`: 90/90 Success, 0 Fail (Saved/T2/wall-tests.log, untracked).
- `python Tools/GASNPPTest/surface_scenarios.py` (60/15 ms, 0 % loss, listen host + 1 client, both subjects, 2 reps, memory kept):
  4/4 pass; client `launch_to_grab` 1 unexplained per rep (the harness's own teleport, as in 6.15), host 0 (Saved/T2/surface.txt).

T2 is closed. Next traversal gates in plan order: T4 (needs M8 G1/G5 merged: GSTNCombatCore), T6/T7 (GSTNPlatform; not while M8 G10 runs), T9.
Cost: ~$1 of tokens (reading + two headless runs).

## T6 parts bridge (laptop, 2026-11-06, branch claude/project-thread-wlggpb)

New plugin `Plugins/GSTNPlatformParts` (README there), enabled in the `.uproject` (plan §6 lists this edit; done without asking, D-rule 6).
`UGSTN_PlatformPartsComponent` feeds `SetShapeMask` from the confirmed broken mask at the pose point; part names resolve to part + ancestor
bits (`FGSTN_PlatformPartGates`, GSTNPlatformCore, shared with T8). GSTNPlatform got three small seams: `OnPrePose` (forward and
presentation poses only), `SetShapeMaskAtFrame`, and the settings treat part names as resolved when GSTNPlatformParts is loaded.
Deviation from plan T6 (also in traversal-doc-deltas.md "T6"): clients do not read their own copy of the fragment. Run 1 did that and
the client reconciled 4-7 times per break (it stamped the change a round trip later than the host). Now the host replicates
{mask, frame} on the bridge and clients stamp the host's frame retroactively; inside the Breaking grace nothing reconciles.
Checks: build green (3 builds). `RunTests GSTNPlatform+GSTNParts+GSTNSurface` 176/176, `GSTNPlatformParts.*` 5/5 (Saved/T6/headless-3.log).
`surface_scenarios.py --phases part` 4/4 pass, client corrections [0,0] (Saved/T6/summary-part-run2.md); wall+stand+part at 2 % loss 2/2
pass (Saved/T6/summary-all-loss2.md). Limit: the scenario breaks the part with `gstn.PlatformParts.SetBrokenMask`, not with a hit (the harness
fighter has no combat state); hit-driven break and the healer restore (P7b) need a combat fighter with a flank: T9 content.
Next: T7 (M, plan $6-10, realistic $10-15: needs a combat fighter carrying a player and may touch the lag comp history in GSTNCombatNP,
so after M8 G1/G5 merge; not while M8 G10 edits GSTNPlatform). Not started here. Cost of this thread: ~$7-10 (Opus, ~120k context, 3 builds, 3 PIE runs).

## SLIDE (brief, 2026-11-08, waiting for the owner's OK before any code)

**Annullato 2026-11-08**: il movimento verrà rifatto come movement mode separata (come la corsa); lo slide non è previsto. Il brief resta come storia.

Source: `docs/notes/handovers/movimento.md` (MOV-1, last item), m3-doc-deltas.md "Slide + jump" (C14/C15: no desync found with GASP's slide).
- **Today**: the slide is GASP's Mover transition `BP_MovementTransition_ToSlide` on the Walking mode of the component inherited from
  SandboxCharacter_Mover: crouch while sprinting starts it, the character stands up at the end. Not an ability: kits, tags, the combat
  command table and cancel rules cannot see it. MOV-1 could not remove it from `BP_GASNPP_TestCharacter` by script (inherited component override lost).
- **Goal**: `UGSTN_GA_Slide` (derives `UGSTN_GameplayAbility`, NP-simulated) that moves the character through a Mover layered move
  (rules 1-2 of CLAUDE.md: deterministic, no direct state writes), owns `State.Movement.Sliding` (native tag in `GSTN_AbilityTags.h`), ends
  crouched (not standing), and is cancelable by jump (slide-jump keeps the slide's horizontal speed) and by the combat cancel rules.
- **Input (proposal)**: RB / C (`IA_Crouch`, already bound) while the ground speed is above a threshold (default 450 cm/s) starts the slide;
  the same button while slow is a plain crouch. Alternative to decide: a dedicated button. Held = longer slide up to a cap; release = early end.
- **Tuning** (`USTRUCT FGSTN_SlideParams`, `Category = "Slide|..."`): entry speed boost, friction/decel curve, max duration frames, min exit
  speed, slope gain/loss along gravity, jump carry factor, cooldown frames.
- **Interactions**: jump during the slide = GA jump with the slide's velocity carried; wall-slide entry from a slide allowed (the wall entry
  transition reads tags only); a hit cancels it (hit reaction wins); minions and links: none.
- **The GASP transition**: the owner removes `ToSlide` from `BP_GASNPP_TestCharacter` by hand (30 s, MOV-1 steps), otherwise both slides
  fire. Code cannot do it without a Blueprint edit; the owner's OK covers that step only if they do it.
- **Acceptance**: `GSTNCombat.NP.Slide.*` unit tests (entry threshold, duration cap, jump carry, determinism of the layered move);
  harness `scenario_slide` (sprint, crouch, slide distance within 5 % on server and both clients, no unexplained reconciles) and
  `scenario_slide_jump_ability` (jump mid-slide keeps speed), dedicated + 2 at 0 % and 2 %; SUITE_DEFAULT on DefaultLevel unchanged.
- **Machine / model / cost**: laptop; Opus for the layered move and the prediction path, Sonnet for tests and harness; ~$5-8.
- **Owner decisions**: (1) RB/C while running, or a dedicated button? (2) the slide ends crouched, or stands up like GASP's?
  (3) OK to remove GASP's ToSlide from the test character by hand?
