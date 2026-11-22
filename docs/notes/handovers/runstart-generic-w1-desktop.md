# runstart generic w1 (desktop, 2026-11-21): sprinter weight transfer + frantic limbs, built, ASSERT OK, Fable PASS -> owner
Branch claude/runstart-generic-desktop (continues 4618a789). Lock _ue59 14:21-14:40Z; owner closed the editor, I reopened it.
DA_Locomotion_Default (owner's uncommitted change) untouched. Re-check (rule 11): s2 "ASSERT OK" confirmed (s2_h1/assert.txt).
Owner ask: "lo sprinter mi piace di più per come avviene il passaggio di peso: variante generica su questo principio". 7/8/9 untouched.
## Principle (rule 13c, full sheet docs/notes/reviews/runstart-w1_h1/curves-sheet.md, one-off measure on generic_{g3,s2,w1}_pose.npz)
The sprinter FALLS before it steps: pelvis -9 cm and torso 0 -> 45 deg together in 4 frames while the push foot stays planted, holds
low (79-83 cm, 42-47 deg, bounce 0.3) through two drive steps, and rises once, on the second push-off (pelvis gets 12-20 cm ahead of
the feet). g3 leans as much (36-41 deg) but its pelvis sinks only 3-5 cm and stays over the feet: it leans, it does not fall.
## Done
- w1 = gstn.RunStart.Variant 20 (Handoff sibling 30). NOT 10: 10-19 are the Handoff copies of 0-9 (offset 10), so 10 = v4r7+Handoff.
  Recipe: TAG=w1 R0=2.3 N_REV=19 HOLD=12 RATE_SAG=0.15 LEAN_OUT=20 ENTRY=4 ENTRY_LEAD=1 BOUNCE=0.3 LEAN_DEG=24 LEAN_IN=4 ARM=1.7
  ARM_BACK=10 DROP_CM=12 DROP_IN=4 DROP_HOLD=12 DROP_OUT=19 (rate 2.29 at f0: frantic from frame 1, no sprinter load). 64 f.
- Cascadeur RunStart_Generic_w1.casc (copy of s2, key 63 added, range 0-63); 0 backward knee frames, min 6.3 cm ahead. FBX up Z baked.
- Cvar help + debug menu label "generic w1 (sprinter weight)" (4b62a642); real build OK 19 s, both modules (Saved/Logs/ci/build_w1.log).
- Clip + stock foley (L f3/12/31/54, R f8/20/..., AS_GSTN_RunStart_Generic_w1_build.json) + rows 20/30 "enabled 4 (expected 4)"
  (Saved/AnimPipeline/rows/runstart_generic_w1.txt). Divergence row 21b updated.
- Capture: ASSERT OK, min 1.00 f97-140 (Saved/RunStartGeneric/w1_h1/assert.txt). Travel f0-200 1017.0 = base (w1_h1.evidence.txt).
  Per-contact foot travel (runs z<13, f80-240, one-off on metrics_*_h1.json): w1 L 14.3/20.5/25.6/28.3/29.3, R 1.1/11.3/33.9/28.8/24.7;
  s2 L 9.6/6.0/32.3/32.5/26.6/25.0, R 5.3/19.1/24.3/22.6/31.6/22.7.
## Review (Fable, life first, docs/notes/reviews/runstart-w1_h1/review_fable.md) + my look at the az45/az90 strips v86-v120
- PASS, 3 of 3 traits (fall by v90, low hold v90-v102, rise v108-v116; frantic from v88; clean hand-off). w1 vs s2: same fall-hold-
  rise, livelier drive, paid with more slide on the drive contacts and a less clean single rise.
- Watch (non-blocking): A posa v100-108 small rise then loop compress before the real rise (fix: DROP_HOLD 14 / DROP_OUT 21);
  B dati montage v96-100 second drive contact slides 20.5 cm (s2 6.0): do not raise the rate; C gioco v86-88 idle blend (code queue).
## Open / to the owner
- Keep w1 as the generic start? If yes, the polish round would be watch A (+ B with R0 2.1). Foley double at the hand-off: same
  A/B/C options as in runstart-sprinter-s2-desktop.md (A recommended).
COSTO: ~4.5 $ | ~4 min desktop | ~105k token review (over the 80k target: 24 tool calls, 8 extra frames)
