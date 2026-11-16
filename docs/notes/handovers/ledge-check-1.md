# Ledge Check track (A) + design of tumble air recovery (B) and balloon knockback — 2026-11-16

Thread "Check ledge detection and post-hitstun air recovery", cloud, Opus. Branch `claude/project-thread-ujco9r` from pcq7jx 846708f3. NOT COMPILED (no editor in cloud).

## Done: A, Ledge Check track (owner 2026-11-16, approved)
- `FGSTN_TimelineTrack_LedgeCheck` (`GSTNCombatCore/.../Timeline/GSTN_Ledge.h`): window [Start, End), ProbeAhead 40 cm, ProbeDepth 60 cm, Action Stop / EndMove / Recoil (Response) / ContinueWithMove (ContinueMove).
- Clock (`GSTN_CombatTimelineLogic.cpp`, after the ground rules): on the ground, not hitstop, from timeline frame 1, probe through `FGSTN_TrackHookEnvironment::HasFloorAhead` = `UGSTN_CombatSimComponent::ProbeFloorAhead` (Mover pose of previous frame, facing, root body channel).
- Stop: sync bit `FGSTN_CombatSyncCore::bLedgeStop` (1 bit on the wire, cleared every tick) read by new `FGSTN_LayeredMove_LedgeGuard` (priority 1, queued by GA_BaseCombat via native `UGSTN_GE_LedgeGuard` when the timeline has a Stop window). Recoil/Continue reuse the Landing paths (StrikeTrack, `LandingContinueTrack`).
- Fix found on pcq7jx: Landing tracks' lag montage and ContinueMove were never loaded with the timeline content (LoadTimelineDataContent); now loaded for Landing and Ledge.
- Tests: `GSTNCombat.NP.Ledge.*` (3, fake probe). Desktop: build _ue59, run `GSTNCombat.NP.Ledge` + `GSTNCombat.NP.HoldUntilLanded` + Landing tests, then PIE: a dash with a Stop window toward the hub edge.
- Not done: a real move with the track (needs a setup script like setup_dropkick_hold_until_landed.py), the editor lane colour/label (generic struct track for now).

## Next thread: B, tumble air recovery (owner approved, ~8 $)
FACT: `GSTN_CombatTick_Victim.cpp:760` keeps Tumble until grounded; commands are blocked by the Combat.State.Tumble tag. Recommended design (Smash-like, keeps the owner's knockdown): after HitstunEndFrame in the air the victim stays Tumble (landing still = tech/knockdown) but becomes able to act: drop the command lock (projected tag e.g. Combat.State.TumbleActionable instead of State.Tumble), any started command leaves Tumble. Data: rules Tuning `TumbleAirRecovery` (UntilGround / AfterHitstun) + per-entity override on the combat component (players vs dummies/AI). Owner said "dipende dalla gameplay ability", meaning unclear: ask whether a move's hit can force "no recovery" (per-hit flag).
## Next thread: balloon knockback study (owner asked)
Study Smash Ultimate's balloon knockback (fast launch that decays quickly, less time in the air) from public sources, compare with `GSTN_KnockbackModel` + Launch layered move decay, propose data in Tuning (not hardcoded), implement if in budget.
