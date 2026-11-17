# Handover M1: import skid/180/wall slide + reaction bones + depth marker holo (2026-11-17)
Desktop, Sonnet medium (device) + cloud Opus coordination; reviews Fable 5.2 high. Branch claude/project-thread-kup1kk (all pushed; never feature).
## Commits
- c6984b3e Mover ABP + ALI_GSTN_Skid + ALI_GSTN_RunStart from _ue59 (merge a3af77f2 with remote 31eef4e6 C4723 fix).
- 8f406e8d R3: 10 assets in Content/Blueprints/Animation/Ported57/{Skid,WallSlide}. Same skeleton SK_UEFN_Mannequin -> no retarget. Root motion off.
- 76a742e0 R4: docs/notes/polish-m1/abp-wallsliding-export.md.
- bc33c37b/90c4c0aa + 3df3853f reaction bones diagnosis + fix. 635a70b5 merge rwaob2 56c87963; 6fe4e6ea holo mats; 5cf4e677 capture r1; 3f334a6a depth marker r2.
## Findings
- Driving_* are LevelSequences (CR keys), not AnimSequences. Source skids have NO Distance curve (only contact_l/r) -> none added; distance matching in M4 needs one generated.
- Driving_WallSlideIdle refs /Game/Rigs/CR_UEFNMannyTatoolsRig (absent in ue59); CR_WallIkControlRig refs an Epic_Content preview mesh (absent): missing refs expected.
- R4 keep/drop (applied, rule 6): ABP_WallSliding = EMPTY SHELL (no vars/state machine/CR node, unlinked graph; 5.7 and NPP copies byte-identical) -> DROP. KEEP clips + CR_WallIkControlRig for the M4 wall layer.
- Reaction "bones detached/stretched": NOT a retarget. animpipe/ue_rig.py::_lock_lengths copied Control Rig scale3d (head 1.15, thighs 1/1.12/1.12, feet 1/0.89/0.89). Fix: scale forced to 1 on ALL tracks (compensating thigh/foot pair), all 8 AS_React_* fixed; max |scale-1| = 0 on 93 tracks (docs/notes/reviews/m1-reaction-bones/rebake/react_rebake_log.txt, script Tools/GASNPPTest/react_rebake_m1.py). Fable: PASS (bone fix scope).
- Depth marker: r1 FAIL (look) root cause = M_DM_Holo missing ISM usage flag -> default material. r2: flag + emissive 4/whiten 0.55, dark Fresnel outline, ring 7.5 cm, dashes 5 cm, decal DiscAlpha 0.45. Fable r2: PASS ring+dashes, ground+jump scope.
## Tests
251/251 pass (DepthMarker.Mover 3, Runtime 33, Core.ProjectionDirection 1, GSTNCombat.NP 209, Visual.DepthMarker 5), log Saved/Logs/ci/m1_test.log.ue.log on desktop _ue59, BEFORE the r2 header-default change (r2 only built: m1_build2.log). Rerun tests at next build.
## Open
1. Blob decal not visible in any frame: needs top-down/decal-debug capture (unsigned).
2. Sideways projection volume not captured: Python can't spawn into PIE; place AGSTN_DepthProjectionVolume in L_Test_Hub (map save = owner OK) or a test map.
3. Reaction side grid f0 column shows end pose for GroundHit/GetUp/Roll/RollBack (capture script stale pose); Tumble dark shapes under shin = probably unlit far leg; ground contact in-game unverified (M6).
4. Leftover untracked Content/GiustinoMoves (3 staging assets) in _ue59: delete by hand.
5. Run-Timed editor scripts hang at quit unless they end with QUIT_EDITOR.
Da confermare (13d): Fade.ShowHeight 60 on feet (tiny hops show no marker).
