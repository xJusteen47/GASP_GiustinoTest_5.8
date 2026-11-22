# Landing, capsule and bounce fixes (cloud, 2026-11-17), branch claude/project-thread-z9mi0s
Base: 98dml5 7c16a2a + 6web8v merged. NOT COMPILED (cloud). Commits 5a2f9d1c (landing) and 46f414d1 (bounces).

## What changed
1. Capsule "1-frame order": the real cause is Mover's render smoothing. The capsule shift is interpolated, but the mesh base offset snaps. From docs/notes/reviews/dropkick-capsule-3/frames.jsonl f137 (`grep '"frame": 137'`): the actor goes 37.2 → 88.2 and mesh_rel_z is -102.7 instead of -88, a 14.7 cm dip. This also explains the high-whiff body missing at v140. Fix: `FGSTN_BodyCapsule::SmoothingCorrection`, applied in `FGSTN_BodyCapsuleModifier::ApplyPresentation` (local pawn only; sim proxies are not corrected, because the fields are not replicated).
2. Touchdown latency: the combat sim probes this frame's fall (a capsule sweep of −v·dt + ½g·dt² + margin) and treats a hit as grounded on the contact frame. Rules|Landing settings: `bAnticipateTouchdown` (default on; off = old behaviour), `TouchdownMarginCm` 3. GroundedRestore reads the same flag. Landing frames shift by −1, so the dair window data goes back to f4-12.
3. ABP land crouch replaying after a landing-lag montage: new anim node "GSTN Covered Landing". It holds the montage's last pose over the base-layer land, then blends out after 0.42 s or on movement. Settings are `CoveredLanding` on UGSTN_CharacterAnimInstance; cvar `a.GSTN.CoveredLanding 0` turns it off. The node is inserted by a script, with no manual wiring.
4. Bounce events: the sim stamps the last wall/ceiling contact (frame + face) in the hit state and ignores recontacts within `SurfaceBounceRules.RecontactFrames` (8; 0 = old behaviour). The deriver uses the stamped face (new tag Presentation.Event.WallBounce.Ceiling, which falls back to the WallBounce cue row) and drops velocity-read bounces within `BounceRefireFrames` (8) of a counted one.

## Desktop steps (next thread)
1. Build (headers changed: full build, not Live Coding).
2. Tests: GSTNCombat.Core.BodyCapsule, GSTNCombat.Core.Landing, GSTNCombat.NP.Hits.Impact, GSTNPresentation.Core.Derive, GSTNAnimation.Core.CoveredLanding, then the full suite (1739 before this branch).
3. Run Tools/GAS/setup_covered_landing_node.py in the editor (log in Saved/CoveredLanding, backup in Saved/Backups/GAS/covered_landing). It edits SandboxCharacter_Mover_ABP: tell the owner first, since it is a shared BP.
4. Set the dair V10 landing window back to f4-12, then re-capture dair_land, dropkick high whiff (v140) and dropkick low hit (v141). Re-capture the M2 bounce sequences too. Then Fable review.

## Open / not done
- Dropkick low hit v141 "body at air position": the smoothing fix should cover it. If it remains, the cause is a reaction montage starting one presented frame late (montage order), which needs a code thread.
- polish-m2-r4 (vcepyw): ground-bounce and ceiling-bump VFX still spawn at the body centre instead of the contact point; the OFF and ON captures start the host at different spots. Not done (budget).
- Cost so far about 15 $ of 20.
