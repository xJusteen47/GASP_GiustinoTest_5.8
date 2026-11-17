# Wall + ceiling default bounce (cloud code, desktop build/test, Opus medium, 2026-11-17). Branch `claude/project-thread-h2hlp5` from `claude/playtest-fixes-1117c` 23fb94a
## Owner design (12:30-12:31Z) A launched victim hitting a wall/ceiling bounces BY DEFAULT, same rules as the ground (Smash); with little knockback a tiny
bounce with a clip, NOT the full tech bounce. Splat and hitbox-forced bounce are separate and unchanged.
## What changed (data, no Project Settings)
- `UGSTN_CombatRules::WallImpact` / `CeilingImpact` (`FGSTN_SurfaceBounceRules`, GSTN_ImpactTypes.h). bEnabled (on). Hard bounce: bSameAsGround (on) =
  GroundImpact threshold 1200 cm/s, only in hitstun, restitution 0.6, MaxBounces 2, window 90; off = own `Bounce` rules. Chain counted apart from the floor's,
  walls + ceilings together (sync `Core.Hit.WallBounceCount/FirstFrame`).
- Judge (setting, both behaviours shipped): Speed (default, like the ground) or RemainingHitstun (>= BounceMinHitstunFrames 20 = hard).
- Bump (under the hard bounce): push off the face BumpMinDistance 4..BumpMaxDistance 12 cm over impact 100..1200 cm/s, drag 0.25/frame (start speed = D x drag x 62.5) (exponent 1), keeps 50% of the slide,
  no damage, nothing under 50 cm/s, also after hitstun (bBumpOnlyDuringHitstun off). BumpFrames 14: hitstun at least that long, one bump at a time.
- Presentation: `Combat.State.Bump` + `.Wall`/`.Ceiling` projected for BumpFrames (sync `BumpEndFrame/BumpKind`, rollback safe); event `Combat.Event.Impact.Bump`.
- Order: Defence wall tech, then surface row (splat/break/forced bounce) and hit Force, then this default. Hit Forbid Bounce = always bump.
- Impact step now runs for any entity with CombatRules (before: needed a surface table or a Defence extension) -> floors of such entities hard-bounce too.
## Choices applied (rule 6), da confermare (13d)
1. "Poco hitstun" judged by SPEED like the ground (owner's clarification); hitstun judge available as setting.
2. Bump size scales with impact; owner said "piccolo piccolo": 4-12 cm off the wall (PIE 1: plain 210 cm/s carried ~45 cm and slid after landing, so bump is now a distance + drag; drag is horizontal, a ceiling bump is mostly gravity). Tune in CombatRules_Standard > Wall Impact.
3. No clip exists: desktop must add reaction anim rules (RequiredTags Combat.State.Bump.Wall / .Ceiling, priority above Tumble, End BlendOut) with a placeholder.
## Desktop checks (fisso, worktree `GASP_Giustino2_wallbounce`, _ue59 untouched)
Build Editor Win64 Dev UE 5.9 OK 580 s (`Saved\Logs\ci\wallbounce-build1.log`). Tests (Select-String "Test Completed. Result=\{(\w+)\}"):
`GSTNCombat.NP.Hits`+`GSTNDefence` 163/0 (`wallbounce-test1.log.ue.log`), both new tests pass; full `GSTN` 1720/1 (`wallbounce-test2.log.ue.log`): only
`GSTNCombat.Editor.Preview.Spike.MontageSampling`, asset load errors (AS_OverlordDropkick_Land: no target USkeleton) in the fresh worktree, not this change (not rerun on base).
Remains: bump clip rule + placeholder in the reaction anim set (desktop, asset edit); PIE check on L_Test_Combat wall. Cost estimate ~8 $ (Opus, ~95k context, 1 build).
## PIE 1 on _ue59 (local merge f189e6f9, see wall-bounce-ue59-check-1.md): weak f45 432 cm/s -> bump, Bump.Wall f46-59; strong f35 3968 cm/s -> hard 2381 cm/s, no Bump.
FAIL "gioco": (1) bump clip montage gone after 1 frame (replaced by another montage, diagnosis pending); (2) push ~45 cm -> fixed by distance + drag (this commit);
(3) tumble pose clips into the wall mesh before contact (queue). MontageSampling passes on _ue59 (content), so the worktree failure was missing content.
## Integration on _ue59 (fisso, 2026-11-17 13:50-15:30Z, owner OK typed in thread; local only, nothing pushed)
HEAD 6cdbaa74 = 0281ba60 (v94blx 7d7b4f20 + this branch) + jprs4t a41f5bac + kup1kk 93a036a9 + rwaob2 16131b6c + 2 local fixes for kup1kk:
9dfdfc68 lambda param Skid -> SkidPhase (C4459, GSTN_PresentationTests.cpp:226; kup1kk later fixed it too) and 6cdbaa74 safe divisor (C4723, GSTN_PresentationLogic.cpp:18).
setup_qa_movement_1117 OK (KIT_Test_Playtest JumpVelocity 700->1240, uncommitted); setup_anim_layers OK after kup1kk 93a036a9 (slots 1 Wall/2 Skid/3 RunStart,
ABP 0 errors 3 warnings); SandboxCharacter_Mover_ABP + ALI_GSTN_Skid + ALI_GSTN_RunStart uncommitted in _ue59, to be committed on kup1kk.
Tests: GSTN 1739/1739 (Saved/Logs/ci/integ1117_test_gstn.log), DepthMarker Mover+Runtime 35/35. Desktop ~/.claude/settings.json: permanent allow for local git/merge/build/test/setup
scripts; deny push feature/main, force, branch deletion, reset --hard, clean, worktree remove, git rm, Remove-Item/rm (all sessions on that PC).
Total cost estimate this thread ~25 $ (Opus, ~150k context, 6 desktop runs).
