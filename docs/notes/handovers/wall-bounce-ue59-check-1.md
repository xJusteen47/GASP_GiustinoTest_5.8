# Wall bounce: _ue59 build, test, bump reaction rules, PIE check (fisso, Opus, 2026-11-17)
_ue59 HEAD = a local merge of 7d7b4f20 (v94blx) + h2hlp5 06078c80 = f189e6f9. It is not pushed. Owner OK'd it: "Ok merge locale su _ue59".
## Results
- Build Editor OK 212 s (`Saved\Logs\ci\wallbounce-ue59-build1.log`). `GSTNCombat.Editor.Preview.Spike.MontageSampling` = Success (`wallbounce-ue59-test1.log.ue.log`,
  Select-String "Test Completed. Result=\{(\w+)\}"): with the sample content it passes, so the worktree failure was missing content.
- `DA_ReactionAnims` (/Game/GASNPPTest/Combat/Reaction) gets BumpWall and BumpCeiling: RequiredTags Combat.State.Bump.Wall / .Ceiling, Priority 21
  (Tumble 20; Splat 30 and Knockdown 40 stay above), BlendOut, in 2, out 6. PLACEHOLDER clip: AS_React_HitStand (taken from the HitStand rule, the
  only hitstun rule). Edited by `Tools/GASNPPTest/pie_wall_bounce.py` (edit_reaction_set) and only that asset saved.
- PIE (`pie_wall_bounce.py`, L_Test_Hub, listen + 2, 60 fps, test surface box leaf Wall with no impact table, a visual-only cube on it). Output is in
  `Saved/WallBounce/run4/*/summary.json` and frames.jsonl. Media from `wall_bounce_post.py` are in `docs/notes/reviews/wall-bounce-1/`.
  - weak (0 %, wall 335 cm ahead of the attacker): Tumble, 432 cm/s into the wall, contact f45. Bump: vin -210 cm/s, Combat.State.Bump(.Wall)
    on f46-f59 (14 frames), then Tumble on f60 only, then landed. Max off the wall 82.7 cm from the capsule centre (about 45 cm past contact;
    the handover expected 2-8 cm). The push carries on as a slide after the landing.
  - strong (250 %): 3968 cm/s into the wall, contact f35. Hard bounce to -2381 cm/s (0.6) in Tumble, no Bump tag, bounce count 3 by the end
    (the wall, then the floor).
  - Runs 1-2: at 80 % the weak launch reached 1552 cm/s (a hard bounce, not a bump). At 0 % with the wall at 455 the victim landed 77 cm short.
## Odd, for the code queue ("gioco")
1. The bump reaction does not hold. The BumpWall montage shows on f46 at position 1.0, and from f47 get_current_active_montage is None for the
   whole bump. The grid shows the character's own locomotion pose (running legs) against the wall, not AS_React_HitStand.
2. The push off the wall is about 45 cm, not "piccolo piccolo". Tune BumpMin/MaxSpeed or the slide kept after landing.
3. In the weak grid the tumble pose clips into the wall face before contact (mesh only).

## Round 2 (e73cb44 bump as a distance; _ue59 local merge 0281ba60, not pushed)
- Build OK 211 s (wallbounce-ue59-build2.log). GSTNCombat.NP.Hits.Impact 10/10 Success, WallCeilingDefaultBounce included (wallbounce-ue59-test2.log.ue.log).
- Weak run only, with WALL_DIAG=1 (Saved/WallBounce/r2, docs/notes/reviews/wall-bounce-1/weak_r2*): contact f45, deepest point 30.7 cm from the capsule centre.
  The push ends at 36.9 cm on f58 (about 6 cm). It lands on f55 at 36.7 cm and moves 0.2 cm after that: no slide.
- Why the clip stops: AS_React_HitStand is 3 frames long (0.05 s). A BlendOut rule stops its montage once Remaining <= BlendOut 6 f + 1.5 f lead.
  It starts on f46 and stops on f47 (ActiveRule None from f47), ConsumedRule = BumpWall, so it does not restart while Bump.Wall holds (to f59).
  No other montage starts f44-f62 (OnMontageStarted only AnimMontage_3 on f46 and the Tumble rule's on f60). The character's own Falling/Walking
  pose shows. This is data (placeholder too short for BlendOut), not the game code: a clip of about 14 frames, or HoldLastPose, would hold it.
