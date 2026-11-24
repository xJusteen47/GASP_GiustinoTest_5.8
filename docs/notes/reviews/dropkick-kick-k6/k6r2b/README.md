# Dropkick K6 pose round 2 (of 3) - review pack k6r2b (laptop, 2026-11-21)
Build: branch claude/project-thread-l1dcng (cards ad0422af). Base r1 = K6 round 1 (Saved/DropkickK6/k6w|k6hit|k6high, pack k6a);
variant r2 = this round (Saved/DropkickK6/k6r2*). Same montage length and timeline (90 f), hitboxes untouched.
Views: az90 side, az180 BEHIND (owner rule 11-21 19:03), az315, game. Whiff/hit cameras 250 cm (same as r1). High press: r2 cameras
at 350 cm (head and feet in frame), r1 was 160 cm, so r1high vs r2high differ in framing, not in pose.
ASSERT OK on the three runs (assert_lines.txt). a<N> = N frames after the montage start, burned into every frame.

## What changed (review k6a "Cosa correggere")
1. Arms as a MOVEMENT: a3 OPEN (startup key 2: guard arms winged, elbows to shoulder height and back, left fist up and out, shoulders
   back, chest open), a5 SLAM into the press (key 4, arrive_1_2: chest curls 6 deg, shoulders up 4), a6 hold, a8 RELEASE (key 7:
   elbows out ~8 cm, shoulders down 5, forearms 3 cm off the chest). Old key 8 removed.
   Offline elbow travel (Poser, chest frame): open -> press 30+ cm; press -> release left 8.4 cm, right 4.2 cm.
2. Settle doubled: pelvis peak +11 deg past the lock (was +5), hips drive 3.5 cm, chest whip -7, toes -14; rebound -3.5 under the
   lock (legs stay +3.2 deg above the body plane); head peak 2 frames after the body: chin +11, turn 9, tilt 5; forearms lift
   2.5/5/2 cm off the chest at settle f2/f4/f7 (left one higher) and fall back last.
3. Untouched: a15 line, snap a13-a15, smug, timeline, hitboxes.

## Thread's own look (not the review)
tiles az315/az180: settle at a17 clearly bigger (legs pitched further up). The a3 open is modest from behind: the upper arm is only
26.7 cm, so the elbows cannot go much wider than the guard; the open reads mostly as elbows/fist LIFTED, not wider. Judge on video.

## Checks
Clearance PASS in UE on startup, settle, loop (Saved/AnimPipeline/animpipe/overlord_dropkick_*_k6_check.txt). Other blocking checks
FAIL as in round 1 (legibility needs matplotlib / KeyError, contact_faf and hitstop_snap not applicable to this air move).
Offline: %TEMP%/k6/k6_fold_report.txt from `python cards/overlord/dropkick/k6/make_k6_cards.py` (worst 0.67 cm, settle k7).
