# REACT-HOOK-2 handover: game-side fixes of the react-hook-1 review (2026-11-14)
Branch `claude/project-thread-0tmywc` (from 004489fb), tip b5b97be4. _ue59 BUILT on it, editor closed, no maps saved. Tests 1596 / 0 fail.
Captures + numbers: `docs/notes/reviews/react-hook-2/` (README.md). Needs the rule-10 cloud review before the owner.
**Done (code):** HitStand held for the whole Hitstun (rule data); Knockdown after Tumble eases down 85 cm over 5 f (rule EntryDrop*,
mesh Z only); bounce `GroundBounceGravityScale` 3 (measured +17.8 cm, 11-12 f); get-up roll only when grounded (Knockdown+23, any frame
while lying: roll_late +52); timeline `bIgnoreMoveInput` on get-ups + zero velocity at roll start/end (keys = exactly 80 cm, no turn in
the roll); input producer holds facing outside moves under Lock.Movement/Lock.Reaction; owner choice 13:08Z `UGSTN_CombatRules::
bTurnBackAlongKnockback` (default on, rules data). Capture: victim faces attacker (0 deg error), views az45 + az90, other pawns hidden.
**FAIL, stopped after 2 attempts:** roll back still plays AM_React_Roll. Cause found: from Knockdown+14 to +23 the GroundBounce ability's
timeline runs (InTimeline), so the "rooted" hold does not apply and the steer path lets the stick turn the lying pawn (yaw 180 -> 88);
at the get-up the stick no longer points behind it. NEXT FIX (one line, attempt 1 of a new thread): `Timeline.bIgnoreMoveInput = true`
in the UGSTN_GA_GroundBounce and UGSTN_GA_Tech constructors (or: hold also InTimeline when Lock.Movement and the timeline has no steer).
**For the review to check:** HitStand freezes on clip frame 2 for the whole stun and restarts once on the 2nd hit (~v45): is the clip's
held pose the right one? ~15 cm of walking after a roll is the stick still held (capture releases it 26 f later), not a slide.
**Later (owner idea):** directional reactions back/side reusing the front clips with a pre-rotation, as rule data.
**Not done:** standalone splat capture (wall spot), soft first frames of GetUp/Roll clips (anim job), ABP crouch, ECB capsule.
**Cost:** cloud Opus thread ~120k context + 2 desktop runs; estimate ~$15.
