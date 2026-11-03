---
name: gstn-visual-review
description: Look once at a move's composite review image and write the verdict file the anim pipeline reads (silhouette, per-part legibility, missing or bad in-betweens, jitter, owner rules). Use for "rivedi la <mossa>", a visual review round of an animpipe move, or `animpipe visual-gate` asking for a verdict.
---

# GSTN visual review (one look, one verdict)

You judge a built move from **one image**. You never edit the card, the rig, Cascadeur or the owner's keys. Your only
output is `verdict.json` (plus a copy in project files `anim-pipeline/verdicts/`). Plan:
`docs/notes/reviews/revisione-visiva-proposta-2026-11-03.md`.

## Input (round folder `Saved/AnimPipeline/visual/<move>/r<N>/` on the desktop)
- `grid.png`: the composite (long side 1568 px). Panels: side grid of every frame with per-part coloured silhouettes
  and the owner's pose as a grey ghost on his keys; key poses from the game camera and from behind; hand/foot/head
  trails, one dot per frame; strip with and without hitstop around the first active frame.
- `manifest.json`: numeric results (gate already passed), which frames are owner keys / Claude poses, segments,
  hitbox frames, FAF.
- The card `Tools/AnimPipeline/cards/<kit>/<move>.json` (read `_design`, `poses`, `anim_active`, `approved`, `exemptions`)
  and the owner rules in `Tools/AnimPipeline/cards/README.md`.

From a cloud thread: copy the round folder from the PC with the already approved folder access (no Remote Control
needed). A thread that cannot see images (Remote Control) must not run this skill: hand it to a cloud thread.

## Steps
1. Read `manifest.json`, the card fields above and `rubric.md` (this folder). Do not read old threads, logs or mp4s.
2. Look at `grid.png` **once**. Go through `rubric.md` item by item; for each item note frame(s), part(s), panel and
   what you see. No second look, no zoom requests, no extra renders.
3. Write `verdict.json` following `verdict.schema.json`:
   - one entry in `problemi` per defect, `gravita: blocca` only when the rubric says so, otherwise `nota`;
   - `prova` = what you see, in one sentence, with panel and frame ("lato f9: avambraccio dx dentro la testa");
   - `correzione` = the smallest change: frame, part, direction in character terms (avanti/su/fuori), amount in cm or deg;
   - `segmenti`: every pair of consecutive key frames, `ok | manca` (a jump: in-between missing) `| brutto` (in-between
     inside the body, knee backwards, mush);
   - `esito`: `ok` (no `blocca`), `correggi` (every `blocca` is on Claude poses or unlocked parts), `chiedi_owner`
     (a `blocca` sits on an owner key or a locked part: describe it, never propose to change it yourself).
4. Validate: `python -m animpipe visual-gate <card> --round=N` from `Tools/AnimPipeline` (exit 0 pass, 1 blocked).
   On the cloud side, at least check the file against the schema by eye; the desktop runs the gate.
5. Reply in at most 5 lines: esito, the blocking problems, the segment states. Nothing else.

## Limits
- One look per round, **max 2 rounds** per move (the gate says when to stop); after round 2 still blocked: hand the
  owner `grid.png` + `verdict.json`, do not start a third.
- Never change `approved` parts or owner keys; a problem there is `chiedi_owner`.
- Doubt between `blocca` and `nota`: `nota`, unless the rubric marks the item as always blocking.
- Calibration (`calibrazione.md`) must hold before the verdicts are trusted: ok on jab v12 and ftilt v3.1, correggi on dair v2.
- Cost target: ~2k tokens of image + ~8k of rubric/card/manifest + ~5k answer, Opus medium (~$0.3-0.6 a round).
