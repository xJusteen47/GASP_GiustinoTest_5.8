# Handover: run start / stop / skid 1 (desktop, 2026-11-18)
Branch claude/project-thread-98dml5 (from a43d9d94). Opus medium. Brief: docs/notes/mosse/run-start-stop-skid-brief.md (owner 1A 2A).

Done (code, built and tested on the desktop):
- Run start 1A, scramble: FGSTN_RunStartScrambleSettings / State (GSTN_LocomotionLean.h). From still with stick >= 0.4, the run start
  override layer (ABP_GSTN_RunStartLayer, already in the graph) plays M_Neutral_Run_Loop_F at rate 1 + 0.9 x (1 - speed/stage0 MaxSpeed) until
  90% of the speed; blend in 0.032 s, out 0.12 s. Feet slip on purpose (override pose: no stride warping, no GASP start clips). Extra forward
  lean 11 deg on additive layer 0 (spring 6 Hz, damping 0.55: one rebound < 4 deg, test checks it).
- Skid 2A, three sections: FGSTN_SkidSectionSettings::ClipTime. Clip SkidOwner/SS_180_Turn_Root180 (untouched): entry [0, 0.24] over the
  kit's EnterFrames, hold [0.24, 0.92] on 1 - speed/entry speed (ease-out 2), push [0.92, 1] over ExitFrames (ease-in 1.5).
  UGSTN_SkidAnimInstance uses it when the profile has the clip, else its old stretch.
- Values in UGSTN_LocomotionAnimSet (DA_AnimSet_Locomotion_Default) sections RunStart and Skid; C++ defaults, the asset was not resaved.
- Tests GSTNCombat.NP.RunStartSkid.{Scramble,SkidSections}. Run: Run-Timed -Preset Test -Tests GSTNCombat.NP ->
  Saved/Logs/ci/rss-test.log: 221 Success, 0 Fail (`Select-String "Test Completed. Result={Success"` count).

Decisions applied (rule 6/12):
- Skid stays in the existing override layer, NOT a chooser row + MM database + orientation warping as the coordinator note said: the stretch
  problem is solved by the sections; a chooser/PSD edit by Python is risky and over budget. Chooser integration = next thread if the
  review shows the facing/turn is wrong (double 180 risk: check first).
- Push does not yet start the scramble at half (brief 3): scramble only starts from still.

Not done:
- Run stop (GASP stop clips shortened + near-zero blend in the chooser): needs PoseSearch database / chooser edits. Next thread.
- Auto-cancel landing source (brief 5): no budget.
- Captures base vs new: editor start blocked by Saved/Autosaves/PackageRestoreData.json dialog (owner must answer it).
  Base = profile RunStart.bEnabled/Skid.bEnabled false (or the commit before this one).

## Part 2 (same thread, owner 13:26Z: skid into GASP's setup on copies)
- 18d7fe1a: ABP_GSTN_Locomotion + CHT_GSTN_LocomotionAnimations (Content/Blueprints/Animation/GSTNLocomotion/, copies of
  SandboxCharacter_Mover_ABP and CHT_MoverCharacterAnimations_PoseMatch; originals untouched). Profile field MainAnimClass (empty = no change):
  set it to ABP_GSTN_Locomotion only once the copy has the skid.
- MCP 5.9 (127.0.0.1:8000): BlueprintDslToolset refuses AnimBlueprint ("not supported by the blueprint domain yet"); the generic
  BlueprintToolset lists ABP graphs (State Controller has Slide Transition / Slide Loop: the model for a Skid state) and has CreateNode /
  ConnectPins, untested on state machine nodes. No tool edits chooser rows. Stopped at 2 attempts (rule 5): manual steps for the owner.
- Next: C++ BlueprintPure on UGSTN_RunPresentationComponent returning skid clip + section time (for a Sequence Evaluator explicit time);
  owner adds the chooser row and the Skid state copied from Slide; then MainAnimClass, captures, Fable review.
- Camera: cherry-picked aca5d77a (02558de5); DA_CameraAnchor_Default values set in the editor's memory but NOT saved (editor was in PIE):
  save it after PIE (Tools/GASNPPTest/camera_tiltshift_defaults.py redoes it).
