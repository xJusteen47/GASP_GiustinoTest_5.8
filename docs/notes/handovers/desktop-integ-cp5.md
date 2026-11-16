# Desktop handover: integ-cp3 with CP4 + CP5 (2026-11-15, _ue59, Sonnet, ~$3 est.)
Branch `claude/integ-cp3` pushed (tip = this commit's parent chain, d4cb0b54 + this note). _ue59 built (editor target), Unreal closed.
**Done**
- Pushed integ-cp3 (was local only). Merged ckzv21 (CP4) and fxjn45 (CP5): no conflicts.
- Build 1 failed on 1 error: `GSTN_EditorTests_MoveFactory.cpp:199` `UFactory::bCreateNew` is protected -> `CanCreateNew()`. Build 2/3 OK (Saved/Logs/ci/build{1,2,3}.log, .esito).
- Test 1 had 1 fail (CreatesAndBinds): the graph dump shows display names ("On Move Started"), test looked for `K2_OnMoveStarted`. Test matches display names now.
- Test 2: `GSTNCombat` filter, 577 Success, 0 Fail (count: `grep -c "Result={Success}" Saved/Logs/ci/test2.log`; summary in docs/notes/reviews/cp5-factory/tests.txt). Includes MoveFactory 3, MoveCosmetics 5, ReactionAnim.
- CP4 `setup_cosmetic_hooks.py`: "done: 4 cues" after fixing `get_active_frames` unpack. 5.9 Python drops the bool return when a function has out params. Log in reviews/cp4-cosmetics/. Committed: SandboxCharacter_Mover, GA_Example_Light/Strong/Special, Examples/Cosmetics/GCN_Example_Hit_*.
- CP5 `check_combat_move_factory.py`: "RESULT: PASS (3 moves)" after same unpack fix at add_move_to_kit_assets and display-name match. Log in reviews/cp5-factory/.
- `setup_designer_examples.py`: plain `BlueprintFactory` (GA factory ignores parent_class on 5.9). NOT run (examples already exist). The same pattern remains in setup_defence_link_content, setup_overlord_{jab,dair,dropkick}_kit, setup_test_kit_archetypes (not touched).
**Not done**: step 3 (PIE harness abilities / forced_rollback / combat_spike) and the CP4/CP5 PIE visual checks (hitstop shake video, menu screenshot): no editor with rendering was run. Untested: dialog/menu, remote proxies.
**State**: `Content/GASNPPTest/Combat/Overlord/AM_OverlordDAirV9.uasset` was already dirty before this session, left uncommitted. Untracked: `Apri editor 5.9.bat`, `Tools/CI/Run-Timed.ps1` (copied from 1gktju, self-test 4 PASS, not committed). PM_BoostJump etc. were not dirty. No maps saved.
**Note**: Run-Timed.ps1 -Kind with -Exe returns 0xFFFFFFFF as int32 error when the editor exits -1 (cosmetic: ESITO still written).
