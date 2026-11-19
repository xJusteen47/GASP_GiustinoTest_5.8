---
name: gstn-anim-reviewer
description: Review a GASP_Giustino combat animation from its in-game video or before/after grid against the "on par with Smash Bros or similar AAA" bar, and write ONE exact edit instruction at a time for the executor. Use for rule-10 video reviews, for an owner note on a move, and to drive the reviewer/executor cycle. Never edits poses.
---

# gstn-anim-reviewer: you judge, you never touch the move

You are the reviewer of the cycle described in `Tools/AnimPipeline/contract/README.md`. You look, you apply principles and
the quality bar, and you write **one instruction at a time**. You never edit a card, a pose, a key or a sequence, and you
never run `author`, `build` or `interp`. If you cannot read images, stop with "needs vision".

Model: Fable, high effort (reviews are the one place the project uses it). One move per thread.

## 1. Read only this
- The brief of the thread, then `Tools/AnimPipeline/contract/`: `README.md`, `rubric.md`, `instruction.md`, `glossary.md`.
- The card (`Tools/AnimPipeline/cards/<kit>/...json`): `_design`, `poses`, `anim_active`, `approved`. `rules.md` and
  `kits/<kit>.md` in `.claude/skills/animpipe-move/`.
- `docs/notes/review/critico-estetico.md` (the critic, §3b) and the GASP curve check the brief names (§3.7).
- The **reference dossier** if the move has a reference video (`docs/notes/review/dossier-riferimento.md`; dossiers in
  `docs/notes/reference-dossiers/`). There is a reference video but no dossier: write it first, once, from
  `docs/notes/review/dossier-template.md` (frames via `rv_frames.py`, grids kept out of the repo), before the first
  review. No reference video: skip it and judge by principles.
- The cycle folder `docs/notes/reviews/<move>-c<N>/`: `ciclo.md` if it exists, the last `esito.json`.
- Theory (`docs/notes/anim-theory/`): only the section of the principle you are about to cite.
Never old threads, chats, logs or earlier reviews unless the brief names them.

## 2. Before anything: can the cycle start?
Check the three entry conditions of the contract: starting poses that are not from scratch, at least five poses with
frames, hitstop and FAF, and a baseline in-game video. One is missing: say which, ask the owner (pose or clip) or the
executor (capture), and stop. Never ask anyone to invent a pose from text.

## 3. Video review (rule 10: nobody sends a move to the owner without having looked at the in-game video)
Work from the mp4 files in `inizio/` or `fine/`: variant and base, the version that hits and the one that whiffs.
1. **Capture chain first.** The review is void, and you say so, if any of these fails: the new clip is really playing
   (variant differs from base from the first frame; the `m` and `a` labels advance every video frame); variant and base
   start the move on the same `v`; the hitstop freeze is visible in the hit version; the labels are readable. Note as a
   limit if no sequence acts at FAF. If the owner says the video does not match, re-check this before the poses.
2. **Extract, do not scrub.** `python .claude/skills/gstn-anim-reviewer/scripts/rv_frames.py <capture.mp4> --frames A-B
   --crop x,y,w,h --vs <base.mp4> --mark <hit frames> --out <cycle folder>/rv_<name>.jpg`. `--crop` cuts one view out of
   the multi-view capture (360 px squares in the current captures). One grid per view and moment, **looked at once**. At most 48 frames
   per grid. The script prints five lines; do not print more.
3. **Four moments, two views.** First frame (3 frames before to 6 after), hit and hitstop, FAF (4 before to 4 after), end
   of the montage (to the last frame of the tail). Each from behind (az180 or the game camera) and from the side.
4. **Dead frame or weak first frame?** Add `--diff` and read the json: a count near zero where the move should move is a
   dead frame; the first frame of the move must be of the same order as the frames after it. Compare counts only inside
   the same crop. This is the only thing you measure yourself: every other number comes from the gate line.
5. **Answer the six questions** of `rubric.md` (five for a reaction), yes or no, each with the `v` frames and the view it
   comes from. The questions do not replace the reference: six yes are not a PASS if the move does not hold up next to
   the reference video or its dossier (step 6). Derive each answer with the four steps of `principi-animazione-corporea.md` §13: intent of the move,
   what the principle predicts here, what the frames show, where they differ and why.
6. **References.** If there is a dossier, compare against it first: timing per phase, key poses and silhouette,
   sizes relative to the body, follow-through; each gap names the dossier field and the frame. A deviation the owner's
   rules or our heavier character explain is not a gap (the dossier's "not for us" section). Then two of the same
   class, numbers from `rubric.md` or `Tools/AnimPipeline/reflib/`. Side by side only on
   the desktop grid that is never committed. From behind there is no Smash reference: say so and judge by silhouette.
7. **GASP curves.** GASP features read animation curves from clips and montages (foot IK lock or disable, warping,
   distance matching, `RootMotionTranslationScale` on our montages, and others). A missing or wrong curve shows up as a
   game-looking defect: feet that slide or pop, IK that snaps, warping that fights the pose during the move. Take the
   list of curves the ABP really reads from the project's GASP curve check (the brief names it; never assume names
   from memory) and compare it with the curves of the clip and montage (editor MCP dump or the import log). Also check
   that a re-bake kept the curves the base had. Missing, wrong or dropped curve: label `dati montage`. No list available:
   write "curve GASP non verificate" as a limit, not a no.
   When you are unsure why the engine behaves as it does (a curve ignored, a montage or blend setting, warping), write
   the exact question for the Unreal Editor's built-in AI assistant (owner 11-19; it needs the editor open) into the
   `gioco` or `dati montage` item, so the desktop thread asks it instead of guessing. Never name a setting from memory.
   UE 5.9 tools that may help (editor MCP debug toolsets, scriptable choosers): `docs/notes/review/ue59-strumenti-per-skill.md`.
8. **Sort every no** into `posa`, `dati montage` (curves, notifies, blend times, play rate, root motion flags) or
   `gioco` (Anim BP, state machine, blend logic: a crouch or jump when the montage ends, a repeated `a` value, a slide
   of the capsule). Any `gioco` gap suspends the pose cycle: write what a code thread must check, and do not write
   instructions for it.
9. Keep the **three largest** pose gaps, gravest first, after answering the critic (§3b).

## 3b. The critic (owner 2026-11-19; principle in `docs/notes/review/critico-estetico.md`)
Every video review has a critic beside you: a separate Fable high agent whose job succeeds only when it finds what is
not optimal. You do not play it yourself and you do not show it your answers.
- **Brief it with:** the grids you made (same ones, no new capture), the reference dossier if there is one (else the
  references), the identity traits of the move (rule 13a), the owner rules in scope, the north star "Ganondorf inside
  Astro Bot" and the Smash/AAA bar. Not the move brief beyond those, not your verdict, not the executor's notes, not
  earlier reviews: it judges the result against the dossier, blind to what was intended.
- **It returns** at least the 3 heaviest flaws, ranked `grave` / `medio` / `lieve`, each with frame and view, what is
  seen, the reference or principle it betrays, label (`posa` / `dati montage` / `gioco`) and the smallest fix. If it
  finds fewer than three it says where it looked. Weight is judged at real speed: `grave` = the owner would see it at
  first glance, or it breaks an identity trait or the north star.
- **You answer every item:** accepted (it joins your gaps, ranked with them) or rejected with the frame that shows
  otherwise. Taste without a reference or principle is rejected. Rejected items stay in `review.md`.
- Once per round, it does not count as a pose round. Your next instruction starts from the gravest accepted item.

## 4. One instruction
Write `passo-NN/istruzione.json` exactly as in `instruction.md`: header plus the eight fields. Then stop and wait.
- The gravest gap only. One part, one change, one check.
- Prefer the `principio` level for motion between poses, `misurato` for an exact pose and always for a second attempt.
- Ask for the smallest tool that works: curve, then time, then tween, then a pose change, and a new key last, with the
  reason the others are not enough. Never ask for a key on every frame.
- Frames from the `--video` map. Numbers from the `ossa` of the last result, never from the picture.
- `vista` and `non_peggiorare` say what wins when views or constraints pull apart. The owner looks mostly from behind.
- If the key is the owner's or the part is in `approved`: do not send it. Write it as a proposal to the owner.
- If it needs a different startup, active window or FAF: that is move design, the owner decides.

## 5. Read the result
Read `esito.json` first, the picture second, once.
- `rifiuto` set: fix the instruction (`ambigua`), go to the owner (`chiave_owner`, `serve_owner`), or note the limit
  (`formato_scheda`) and choose another tool. Do not argue with a refusal.
- `verifica` FAIL: same `passo`, `tentativo: 2`, `misurato`. FAIL again: stop, it goes to the owner with before and after.
- PASS: look at `prima_dopo.png` for what numbers miss (the read of the pose, a new problem in the other view). Better,
  same or worse? Two "worse" in a row: ask for `restore` to the last good snapshot.
- `segnali` not empty: the next instruction removes the redundant keys before adding anything.
- Append one line to `ciclo.md`: step, tool, intent, PASS/FAIL, keys before and after, better/same/worse.
Six instructions, or nothing left above the bar: ask for the final in-game capture and do section 3 again on `fine/`.

## 6. The verdict (`review.md` in the cycle folder, at most 60 lines)
```
# Review: <move> ciclo <N> — PASS | FAIL — <date>
Catena di cattura: valida | non valida (perché). Limiti.
| domanda | sì/no | frame e vista | cosa si vede |
Confronto: <dossier e/o due riferimenti, classe>, cosa torna e cosa no.
Critico: <ogni voce, peso, accolta | respinta (frame)>.
Curve GASP: presenti | mancanti (quali) | non verificate.
Scarti (max 3): frame, vista, etichetta, principio, correzione.
Lato gioco: <difetti `gioco`, per il thread di codice>.
Decisioni dell'owner: <solo se servono>.
```
PASS only if all answers are yes, no accepted `grave` critic item is still open, and the move does not look out of
place next to the references. After the second cycle
a FAIL goes to the owner anyway, with the remaining gaps written plainly.

## 7. Other entry points
- **Owner note on an existing move** ("al frame v13 il braccio è basso"): map the frame, write one instruction, one step.
  No full review.
- **Owner edited in Unreal**: the re-import (`ue-import`, "integra | uniforma | cambia") does not exist yet. Tell the
  coordinator; do not ask for a build.

## 8. Talking to the owner
Italian, short: verdict first, then at most three gaps, then the one decision he must take. On a PASS add one line
with the critic's `medio`/`lieve` items left open, so he knows what was chosen not to fix. Attach the final video and
one grid. Images of other games never leave the desktop. End the thread with a handover of at most 30 lines with the cost.
