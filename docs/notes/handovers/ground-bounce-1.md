# Ground bounce: hard bounce by speed + tunable tech bounce (cloud, 2026-11-16). NOT COMPILED
Branch `claude/project-thread-ipt5wf` from `claude/kit-playtest-1116` 9658b271. Opus high, no editor, no asset touched.
## Owner design (22:17Z, 22:23Z, 22:24Z)
Tumble hitting the floor: fast (spike) = classic hard bounce, still launched; slower = "tech bounce" (small hop, then lying), height tunable,
scaled by impact force down to a minimum or none; per-move override; a limit so a victim cannot bounce on forever. Dropkick = reference tech bounce.
## What changed (all data, no Project Settings)
- Hard bounce, core: `UGSTN_CombatRules::GroundImpact` (`FGSTN_GroundImpactRules`, GSTN_ImpactTypes.h): bBounceBySpeed (on), BounceMinSpeed 1200 cm/s
  (speed into the floor x hit SpeedScale), bOnlyDuringHitstun (on), Restitution 0.6, MaxBounces 2, BounceWindowFrames 90 (0 = no limit).
  `FGSTN_CombatVictim::StepImpact` decides floors itself, also without a surface table/row; surface row bBounce/MinImpactSpeed and the victim's
  ImpactResponse still restrict. Chain in sync state `Core.Hit.GroundBounceCount/FirstFrame` (live while tumbling, inside the profile wire block,
  reset by a new stagger). Trampolines (row bForceDefault) unchanged and not limited.
- Tech bounce, Defence: `FGSTN_GroundBounceRules` (DefenceRules > Ground Bounce): bScaleWithImpactSpeed (on), NoBounceBelowSpeed 0, ScaleMinSpeed 300,
  ScaleMaxSpeed 1200, MinHeight 12 cm, MaxHeight 40 cm, HeightExponent 1, SelfKnockdownImpactSpeed 900 (dropkick user landing, no contact record),
  gravity scale 3 as before. Height -> speed `UGSTN_DefenceMath::TechBounceSpeed` (v = sqrt(2 g s H), entity gravity). Old fixed speed when off.
  Before: always 300 cm/s = ~15 cm, playtest "not visible". Now 12..40 cm (40 cm ~ 21 frames in the air).
- Per move (the hit's Impact profile): Force=Bounce always hard (spent on first floor), Forbid Bounce never hard (always tech), NEW bNoTechBounce
  lands flat (packed bit 7), SpeedScale scales both judgements.
## Choices applied (rule 6), da confermare (13d)
1. Owner's "cancel after N frames" read as: no more HARD bounces after N frames (BounceWindowFrames) or N bounces (MaxBounces); then floors are
   landings. Other reading (bounce becomes actionable after N frames) NOT done.
2. Hard bounce only while in hitstun by default (Smash-like): a launch upward coming down after hitstun lands with a (max height) tech bounce.
3. Dropkick (owner 22:32Z, like Big Band): its USER does the tech bounce on landing. Per move: new Defence track "Self Landing Bounce"
   (FGSTN_SelfLandingBounceTrack: Mode Rules/Height/None, Height 25 cm) next to the KnockdownOnLanding tag window; without it the rules'
   SelfKnockdownImpactSpeed. Desktop: add the track to the dropkick timeline. Its victim follows the generic rules. Air timeout (2 s -> air idle)
   ends the move, so no self knockdown after it; the hop is vertical + stick steer, a downed user falling off a ledge does not bounce again.
4. Flat landing (bNoTechBounce / NoBounceBelowSpeed) still starts the kit's GroundBounce GA (the clip); no-clip variant not done.
## Desktop checks (nothing compiled or run here)
1. Build _ue59 (IN-USO.txt). 2. Tests: `GSTNCombat.Core.Impact.GroundImpactRules`, `GSTNCombat.NP.Hits.Impact` (new GroundBounceBySpeed),
`GSTNCombat.NP.Hits.State` (new GroundBounceChain), `GSTNDefence.Core.Bounce` (new TechBounceHeight), `GSTNDefence.NP.*Landing*`, then full GSTN. Risk: integration tests
that expect a fast spike to down at once may now see 1-2 hard bounces first (intended); judge per test.
3. PIE L_Test_Hub: IMPATTI kit X (spike) at 100% = hard bounce(s) then tech bounce + lying; weak launch = small hop; dropkick = tech bounce.
Tune in CombatRules_Standard > Ground Impact and DefenceRules > Ground Bounce (do NOT save unless the owner wants).
**Cost**: estimate ~9 $ (Opus, ~110k context, no build).
