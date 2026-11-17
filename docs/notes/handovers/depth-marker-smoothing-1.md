# Depth marker + debug shapes follow the smoothed mesh (thread rwaob2, 2026-11-17)

Branch claude/project-thread-rwaob2, based on claude/playtest-fixes-1117c (23fb94a4). Cloud only: NOT compiled, NOT run.

## Finding (owner's hypothesis: right in substance)
- Mover sim moves the capsule root at 62.5 Hz; the mesh is smoothed between the last two ticks (UMoverComponent::FinalizeSmoothingFrame,
  mode VisualComponentOffset, NPP OnWorldPreActorTick). Root and mesh are up to one tick apart (~10 cm at 600 cm/s).
- Depth marker: SandboxCharacter_Mover is a Pawn, not ACharacter, so it used the generic Actor adapter: feet = capsule CENTRE + an offset
  from "the first mesh that is not the root" vs its archetype rest place. Neither is Mover's visual/base (the per-move capsule changes the base).
- Hitbox/hurtbox/body debug (GSTN_CombatDebugDraw): drawn purely on the simulated transforms, never on the mesh.

## Change
- New plugin Plugins/GSTNDepthMarkerMover (enabled in .uproject): UGSTN_DepthSubjectAdapter_Mover, priority 200. Presented root =
  Base^-1 * PrimaryVisual (what Mover wrote), feet = capsule bottom along Mover up, airborne/grounded from UCharacterMoverComponent,
  base from GetMovementBase(). bFollowPresentedMesh=false or smoothing None -> simulated root.
- GSTNCombatNP debug: new cvar gstn.Combat.Debug.FollowMesh (default 1 = shapes on the smoothed mesh; 0 = on the sim root, where hits
  really are). Offsets Root/BakedSocket hitboxes, hurtboxes, capsule, pushbox; WorldAtMoveStart/projectile hitboxes stay put.
- Tests: GSTNDepthMarker.Mover.PresentedRootMath / ResolvesForMoverPawn / FeetFollowSmoothedMesh.

## Behaviour change to check in playtest
- Feet are now the real feet (were capsule centre, +90 cm). ShowHeight 60 / HideHeight 40 now apply to the feet: on tiny hops the marker
  may not appear (before, it showed on any jump). Tunable per style (Fade.ShowHeight). Da confermare: keep 60 or lower it.

## Desktop next (build thread)
1. Merge this branch into the current playtest branch; build (script, 45 min cap).
2. Run tests: GSTNDepthMarker.Mover, GSTNDepthMarker.Runtime, GSTNCombat.NP (30 min cap).
3. PIE L_Test_Hub: run in a circle, marker must stay centred under the feet; gstn.Combat.Debug.Hitboxes 1 + FollowMesh 0/1 A/B.
4. If the marker still trails: next suspect is not the position read (probes are re-projected each frame); capture a clip first.
