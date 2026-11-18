# Hitstop: Sakurai's 8 techniques (2026-11-18, cloud, branch claude/project-thread-8oy91z from 98dml5 5b22e56e). NOT compiled.

Audit before (code reading) -> what this branch does. All presentation only: no sim state, no sync fragment, deterministic, online-safe.
1. Asymmetric shake: victim 3 cm, attacker 0 -> attacker default **1 cm** (`FGSTN_HitstopShake.AttackerAmplitudeCm`; validator warns if >= victim).
2. Hurtboxes never shake: ALREADY OK (mesh relative offset only, `UGSTN_ReactionAnimComponent::UpdateEntryDrop`).
3. Ground X only / air all axes: was screen-right always -> ground = screen right minus owner-up part; air (`AirTags`, default Combat.State.Falling + Tumble) = new axis per cycle by golden angle in the screen plane. Switch `bAllAxesInAir`.
4. Decay over the precomputed hitstop: was fixed 0.2 s -> `DecayMode` OverHitstop (default; length from the sim's HitstopFramesLeft) / FixedTime (old). A new hit during the freeze restarts the shake.
5. Multipliers: ALREADY OK in the sim: per hitbox `HitstopMultiplier` + `AttackerHitstopMultiplier` (Marth tipper), per hurtbox `ReactionScale.HitstopScale`, per kit via a kit mod `HitstopScale` (empty filter = every move). No per-character field added (owner: kits differ only by moves for now).
6. ~4-frame blend into the damage pose: `UGSTN_ReactionAnimSet.HitstopPoseBlend` (default ON): a reaction starting in the victim's hitstop blends 4 f, CircularOut (~2/3 on frame 1, keeps owner rule 11-14 "first frame reads as a change"), clip runs to `HoldAtFrame` 4 and holds until hitstop ends. Poses untouched ('Loosen arms' thread owns them; could not message it, a person's session).
7. Attacker micro-advance <1 frame: `HitPresentationComponent.HitstopAnimRate` existed but cannot move a synced montage (LandOnSimTime lands on the frozen sim time). New: fork `UNetMontageSimulator::SetPresentationLead` (drawn time + lead, under blend-out) driven by `UGSTN_ReactionAnimSet.HitstopAdvance`: Mode TaggedMoves (default; attacker carries `Combat.Feel.HitstopAdvance` from a move's Tags track) / AllMoves / Off, `AdvanceFrames` 0.6, ease-out; after the hitstop the pose waits for the sim (no jump back).
8. Camera-distance scale: ALREADY OK (`ReferenceCameraDistanceCm` 400, cap x3).
Hooks: `FGSTN_HitstopPresentationHooks` (GSTNCombatGAS) filled in GSTNCombatNP StartupModule. Debug getters: GetHitstopShakeOffset, GetHitstopAdvanceLead, IsHitstopPoseHeld.

Files: GSTN_HitstopFeel.h/.cpp (new), GSTN_ReactionAnimSet.h/.cpp, GSTN_ReactionAnimComponent.h/.cpp, GSTN_CombatTags.h/.cpp, GSTNCombatNPModule.cpp, fork NetMontageSimulator.h/.cpp.
Tests (new): GSTNCombat.GAS.ReactionAnim.HitstopShakeSakurai, .HitstopPoseAndAdvance, GSTNCombat.NP.MontageSync.PresentationLead; existing .HitstopShake still valid.

## Desktop steps (one thread, editor machine; full build: headers and reflection changed)
1. Merge branch, full build (Run-Timed 45 min), run `GSTNCombat.GAS.ReactionAnim` + `GSTNCombat.NP.MontageSync` (Run-Timed 30 min).
2. Likely compile spots: TestEqual(double) overloads in the GAS test, `FAlphaBlend::AlphaToBlendOption`, TFunction statics export.
3. PIE L_Test_Hub, KIT_Test_Playtest vs dummy, game camera, real speed + 4x slow capture (rule 10), heavy hit (strong/smash):
   a. victim shakes more than attacker, attacker visibly trembles a little; b. grounded victim: only side to side, feet never in floor;
   c. air hit (juggle/dair): shake also vertical; d. shake strong on frame 1, gone exactly when the hitstop ends (short jab vs heavy);
   e. victim: no snap and no full freeze, ~4 f blend into the hit pose, holds, then plays on; f. zoomed-out camera: shake still reads.
4. #7: set `HitstopAdvance.Mode = AllMoves` in a COPY of the reaction set (owner edits DA_ReactionAnims: don't touch) or tag one move's
   timeline with Combat.Feel.HitstopAdvance (Tags track); slow capture: weapon/limb creeps forward during the freeze, no jump back after.
5. Known limit: on the attacker's own screen a REMOTE victim is still frozen by HitPresentationComponent's predicted pause (bPauseMeshes),
   so #6 shows on the victim's screen and the host; decide later whether to delay that pause by the blend.
6. Each of 1/3/4/6/7 has an off switch for A/B. Fable review of the capture before it reaches the owner.
