# getup-momentum-1 handover: knockdown get-ups with FAF tail and exit momentum (laptop, 2026-11-21/23). PARTIAL
Branch `claude/project-thread-rqxsn8` (from `claude/runstart-generic-desktop` 84cc0057), folder `GASP_Giustino2_n216qs`. Brief: getup-momentum-brief.md.
## Done
- Code (GSTNDefence NP): `FGSTN_GetupExit` hands an exit velocity to the next move only on `TailCancelled` (one-frame PushBack): roll along its
  travel, stand-up along the facing. Data `ExitSpeed`, cvars `gstn.Getup.{Stand,Roll,RollBack}ExitSpeed` (-1 = data) in the GSTN Debug window.
  New class property `ActionableFrame`: the FAF written only in the timeline struct came back as the clip end after save + reload
  (probe: saved tail/exit survived, FAF did not; a BP compile does not reset it), so the get-ups rebuild FAF + tail from it on init/load/edit.
- Data (owner OK in thread 2026-11-23 "ok salva"): `GA_GetupNeutral_Test` / `GA_GetupRoll_Test` / `GA_GetupRollBack_Test`: FAF sim 21 of 27/24/24,
  **tail cut when the player acts = sim [21,27) / [21,24) / [21,24)** (clip f20-25 / f20-23), tail blend 6, exit 120 / 220 / 160 cm/s.
  FAF rule: first clip frame the pelvis stays >= 85 % of standing with both feet planted (`getup_faf_measure_ue.py`, data/faf_measure.txt).
- Build OK (Saved/Logs/ci/getup-build2.log.esito); tests GSTNDefence 84 Success / 0 Fail (`Select-String Result=` on getup-test2.log.ue.log),
  new test `GSTNDefence.NP.Getup.ExitMomentum`.
- Captures (`getup_capture_timed.ps1`, all variants in one Play session, az180 behind + az90 side, ASSERT: OK x3, data/*.assert.txt):
  base 6 (old data), new3 15 (idle / run / jab / run v0 / run x1.5), run4 9 (stick pushed again at FAF).
  Media: docs/notes/reviews/getup-momentum/{base,new,run}/<seq>.mp4 (input-10..+70) and <seq>_{az180,az90}.jpg (getup+12..+41, FAF = +21).
## Numbers (data/speeds*.txt, capsule speed from frames.jsonl, python one-liner in the thread log)
- Jab at FAF: tail cut, 99 (roll) / 122 (roll back) / 100 (stand) cm/s in the first 6 frames after FAF; idle: 0 (tail plays out, no slide).
- Run (stick): identical in every speed variant, ~0 for 6 frames after FAF then walking: **the stick does NOT cut the tail** of a get-up, held or
  pushed again at FAF. So the run-out has no momentum yet. Likely cause (not verified): the get-up's `bIgnoreMoveInput` zeroes the move input
  (`GSTN_CombatSimComponent.cpp:971`) so the tail's stick cancel never sees it. Label: **gioco**, next code thread (Opus medium, ~6 $).
## For the cloud review (rule 10): base vs new per get-up, identity traits in the brief (poses untouched); jab-at-FAF and idle tail first.
## Owner question answered in thread: get-up clips are ours (pose plans, 2026-11-14); no 360: two axes only (feet / head), no turn, no get-up attack.
## Not done / da confermare (rule 13d): stick tail cut (above); 360-degree get-ups (owner design); FAF 85 % vs 75 % (longer tail).
Laptop: no editor left open; _ue59 untouched; untracked K5/K6 review dirs and _probe_notify.py in this folder are another thread's.
COSTO: ~24 $ | 0 min desktop | 0 token review
