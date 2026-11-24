# dropkick-k6-2 start notes (from the K6 round-1 thread jx1fv2, 2026-11-21): technical tips for pose round 2 of 3
Read with review k6a (claude/project-thread-1akb6a 2bd9b2ef, docs/notes/reviews/dropkick-kick-k6/k6a/review.md). Checkout
GASP_Giustino2_n216qs, branch from jx1fv2 5a821c53. Round 1 closed at its checkpoint (rule 2). KEEP: Big Band line, snap a13-a15, smug.

Where the knobs are (Tools/AnimPipeline/cards/overlord/dropkick/k6/make_k6_cards.py, run from Tools/AnimPipeline; it prints the
fold3 clearance of every key and blend to %TEMP%/k6/k6_fold_report.txt):
- PRESS / RELAX / CLAMP / LOOSE dicts (clav = protraction deg, shrug = clavicle ry, - lifts, across = cm, curl = spine deg) and
  arms(). The build keeps ONLY the card's pose frames: startup has 0/4/8/11/14/15. For the review's a2-a3 OPEN and a5 CLOSE add
  startup keys 2 and 5 (st[2], st[5]; write() builds the card's pose list from sorted(st), give the new frames a spacing, e.g.
  hold_recoil). Game a<N> shows anim frame ~N-1 (k5 trap), so an a3 open = card key 2.
- Clearance budget: the press already sits at 0.07 cm in UE (lowerarm_r|spine_03 @ f4). Buy the contrast with the OPEN before (elbows
  wide = free) and the RELEASE after (elbows down/out, shoulders down), plus shrug (-ry) for "shoulders up". Offline fold3 is ~0.3-0.4 cm
  OPTIMISTIC vs UE on the lying fold: keep >= 0.4 offline there.
- Settle: PEAK (env K6_PEAK_PITCH, now 5 -> review wants 10-12), UNDER (now -2.5 -> 3-4 under the lock; legs stay above the plane:
  the line puts them at +6.7 deg, so -4 leaves +2.7), HEAD_PEAK (chin +6 -> >= 10, two frames after the body), and the forearms
  "lift 4-5 cm off the chest then fall back last": add +4..5 cm chest-frame +y to hand and pole in se[2..4] (off the chest = safe).
- plank() = the owner's line (ARM_OFF 2 cm keeps the lying fold clear of the curl); do not change it.
Pipeline (worked in round 1, ~25 min): owner "vai" needed for the editor; Run-Timed with editor_session.py (plain absolute call),
wait for a FRESH Saved/AnimPipeSession/ready.txt by mtime (rm of Saved files is denied), `python -m animpipe author|build|check
<card>` for startup_k6, settle_k6, loop_k6, then ue.call run_python_file %TEMP%/k6/run_setup_k6.py (sets DROPKICK_KICK_ROUND/
SETTLE/LOOP_ROUND=K6 and runs setup_dropkick_kick_variant.run(refresh=True)) - recreate it from this handover if TEMP was cleared;
echo > quit.txt. Captures: k6/capture_k6.ps1 (one Run-Timed per run, each with its assert spec in Saved/DropkickK6).
Capture fixes asked (not a round): owner rule 11-21 19:03 every capture BEHIND and SIDE: set PIE_CAPTURE_VIEWS=az90,az180,az315,game
(pie_capture_move.py VIEWS has az180); high press with cameras at 350 cm (d in capture_k6.ps1) so head and feet stay in frame.
review_pack_k6.py takes the same dirs; add az180 to its view lists.
