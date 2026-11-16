# combat-sync-1: synced montage ~2 frames ahead, capsule at z 88.1 (desktop, 2026-11-12/13)
Branch `claude/desktop-combat-sync-1` from `7c6c441f` (M11 verify tip), on origin also as `claude/project-thread-ou0elm`. Worktree `GASP_Giustino2_m11v`. UE 5.8. No anim assets, maps or Blueprints touched.
**(a) Cause, three off-by-ones** (measured in `docs/notes/reviews/overlord-jab-v2/chain_hit_rate_frames.jsonl`, branch ipc4gc):
1. `UNetMontageSimulator::SimulationTick` advanced a montage in the tick that started it (abilities tick first): after timeline frame N the sim montage was at N+1.
2. `FinalizeMontageFrame` set the anim instance to the sim time *before* the mesh's animation update, which then advanced it by one more render
   frame (and drift up to 10 ms was tolerated): drawn pose = sim + ~1 frame. Together: first image a2.04 at timeline frame 0.
3. The combat model paused the montage from the hit frame (`IsInHitstop` of the output state), one tick before its move clock stands still:
   this is what "pulled it back" and realigned the montage after a hit.
**Fix** (`e08b3819`): the montage holds its start tick (fork); the combat model pauses it on the frames the move clock stands still (`FrameClockStill`
from `AdvanceHitstopClock`); presentation `ESyncedMontagePresentation::LandOnSimTime` gives the instance the rate (or start position) that lands the
mesh update on the smoothed sim time, rate 0 under the hit presentation's mesh pause. Result: after timeline frame N the pose is anim frame N,
hit frame included, through the hitstop and after. Presentation is per machine (no desync); the sim change is identical on every machine.
**Both behaviours as settings** on `UNpAbilitySystemComponent` (class defaults): `bAdvanceSyncedMontageOnStartTick` (off; on = old start tick and old
pause-from-hit pairing), `SyncedMontagePresentation` (`LandOnSimTime`; `FreeRun` = old). Fork patch rows in `Plugins/GASNPP/PATCHES.md` section 1.
**62.5/60 play rate in the jab kits: keep it.** It is not a workaround of this bug but the unit conversion (timeline frame 16 ms, card frame 16.7 ms);
without it frame 9 shows a8.64 even with the fix. Better later: derive it in code per timeline (sim Hz / anim fps) so every move gets it (not done).
**(b) Capsule z 88.1 is correct, no code bug**: test pawns (`BP_GASNPP_TestCharacter`, `SandboxCharacter_Mover`) have capsule half height 86 (radius 30;
probe `Saved/CombatSync1/capsule_probe.txt`); Mover floats the capsule 1.9-2.4 cm over walkable floor, so rest = 87.9-88.4. The spawn at z 100 is
12 cm high and Mover does not correct it while idle; the first move (any landing, the jab root motion) snaps it. Folded legs = static ik_foot bones (anim side).
**Gates**: build ok; headless `RunTests GSTN` **1556/0** (+2 `GSTNCombat.NP.MontageSync.{LandOnSimTime,StartTickHold}`), 0 ensures (`Saved/CombatSync1/headless-1.log`).
`run_suite` R31_ALL listen+2 **49/50** (`Saved/R31/report-cs1.txt`): known reds hit_basic/hit_trade/hit_host/kit_without_dodge PASS here, hitstop_montage PASS;
`jump_squat` FAIL once (owner request count 3 vs 3,3 on a refused press; the squat plays no montage). A rerun was refused by the permission
check, so its flakiness is not proven: rerun `R31_ONLY=jump_squat` before merging. The suite needs the user ini at PIE_ListenServer / 3 players (restored after).
**Not done / watch**: nobody has seen it in-game; the anim thread should re-capture jab 1/2 (expect no hand pop and the contact pose on the hit frame).
Mover's hitstop freeze still starts on the hit frame (OverrideAll): the montage's root motion step *into* the hit frame is now dropped by it (was
dropped one tick later before). Small for the jab; if a root-motion move ends short on contact, move the freeze to the frame after the hit.
Artifact not republished: the branch merges after the 5.9 cut, the coordinator owns the push line. Max 2 attempts: 1 used per defect.
Cost: ~210k context over two days (one restart), Opus 5.5 high, no subagents, estimated ~$12-16. Wall clock ~2 h (1 editor build, headless, 2 suites, waits for other editors).
