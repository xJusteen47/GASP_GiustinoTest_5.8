# REACT-HOOK-2 handover: game-side fixes of the react-hook-1 review (2026-11-14)
Branch `claude/project-thread-0tmywc` from `004489fb` (claude/project-thread-2n1mrc). Review: `reviews/react-hook-1/review.md` on fy7agl.
**Code (cloud, written before the build):**
1. HitStand held for the whole Hitstun: DA rule HitStand end = HoldLastPose, 5-frame blend out when the tag ends (script data).
2. 85 cm landing drop: new rule data `EntryDropCm/EntryDropFrames/EntryDropAfterRules` (UGSTN_ReactionAnimComponent): after Tumble the
   Knockdown mesh starts 85 cm up and falls into place over 5 frames (accelerating). Mesh relative Z only, written as a difference over
   Mover's own writes. First frame still the new pose. Real fix later: GroundHit from tumble height or ECB (anim/design).
3. Bounce: `FGSTN_GroundBounceRules::GroundBounceGravityScale` = 3 (300 cm/s -> ~15 cm, ~13 frames; was 46 cm / 38 f).
4. Get-up roll only when grounded (`Context.bGrounded`) and actionable, on any frame while lying (test updated: no roll from the air).
5. New timeline flag `bIgnoreMoveInput` (default on for UGSTN_GA_GetupRoll/Neutral, set by the script): no move input, facing held.
   Roll starts and ends with a one-frame zero-velocity freeze: travel = keys only (80/65 cm), no slide.
6. Back-hit (owner 13:08Z, recommended option): `UGSTN_CombatRules::bTurnBackAlongKnockback` (default ON, rules data, not project
   settings): launch and push back turn the victim back-first along the knockback. LATER (owner idea): directional reactions back/side
   reusing the front clips with a pre-rotation; the rule data can grow a direction filter. Not built.
7. Capture: victim placed in front of the attacker and turned to face it; views az45 (front, attacker hidden) + az90 (side); all other
   pawns hidden from the captures; dead zone 40 cm; new sequence roll_late (stick after 50 lying frames).
**Desktop steps:** merge into _ue59, build, run `Tools/GAS/setup_reaction_anims.py` headless, tests `GSTNDefence`, `GSTNCombat` then the
full suite, capture with pie_capture_react_ingame.py + pie_capture_grid.py into `docs/notes/reviews/react-hook-2/`, commit, push.
**Not done:** standalone splat capture (needs a wall spot), first-frame softness of GetUp/Roll clips (anim), ABP crouch.
