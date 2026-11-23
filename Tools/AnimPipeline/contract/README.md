# Animation contract: one reviewer judges, one executor applies

Shared by the skills `gstn-anim-reviewer` and `gstn-anim-executor` (`.claude/skills/`) and by Antigravity. Whoever plays a
role reads this file plus the one it needs: `instruction.md` (the hand-over format), `rubric.md` (how a move is judged),
`glossary.md` (words, frames, rig controls). `animpipe-move` stays the manual of the commands. Theory is not repeated here:
`docs/notes/anim-theory/` (`principi-animazione-corporea.md`, `animazione-picchiaduro-action.md`, `silhouette-multidirezionali.md`).

Why two roles (owner, 2026-11-15): no model has built a good move from text or from a few minimal poses. Antigravity works
when it is told exactly what to do. So judgement and hand are separate, and neither does the other's job.

| Role | Who | Does | Never |
|---|---|---|---|
| Reviewer | Claude with vision (Fable), cloud | Looks at grid or video, applies principles, the quality bar and the references. Writes ONE instruction at a time. | Edits a card, a pose, a key, a sequence. Re-measures what the gate measured. |
| Executor | Antigravity (through `agy-bridge`), or a cheap Claude with `animpipe` | Applies that one instruction on the card, runs the gates, returns before/after. | Decides what to fix, judges how it looks, proposes the next step, touches an owner key. |
| Owner | Giustino | Gives the starting poses, decides on his own keys and on move design, sees only what passed the review. | |

## Entry conditions (the cycle does not start without all three)
1. **Starting poses exist, never from scratch.** Source, in order: the owner's poses (enter whole, locked); a free clip
   (Quaternius, Game Animation Sample, Paragon, Mixamo: keep its key poses, redo the timing); a measured reference
   (`ref_keyposes.py`, side view: the class of the move, not the move; this pose goes to the owner before the cycle).
   None of the three: the move does not start, the reviewer asks the owner for a pose or a clip.
2. **At least five poses** with frames, hitstop and FAF: idle break, charge, contact, follow-through apex, recovery.
3. **The baseline in-game video shows no game-side defect** (crouch when the montage ends, frozen frame, blend, leg jump).
   If it does: mark it `gioco`, suspend the pose cycle, ask for a code thread. Never send it to the executor.

## The cycle (folder `docs/notes/reviews/<move>-c<N>/`)
1. Executor: build, gates, baseline in-game video and grids into `inizio/` (variant and base on the same frames).
2. Reviewer: looks at `inizio/`, checks the capture chain, answers the rubric, lists the gaps by gravity (max three).
3. Reviewer writes `passo-NN/istruzione.json` for the gravest gap. One instruction, one change.
4. Executor applies it, runs every gate, runs the instruction's check, writes `passo-NN/esito.json` and
   `passo-NN/prima_dopo.png` (touched keys and neighbouring frames only, with trails).
5. Reviewer reads the result line, looks at the before/after once, appends one line to `ciclo.md`, goes to 3.
6. After the last step: executor captures the final in-game video into `fine/`; reviewer writes `review.md` (rule 10).
   Only a PASS goes to the owner.

In-game video is captured twice per cycle, start and end. The steps in between are judged on the grid.

## Limits and stops
| Case | What happens |
|---|---|
| Six instructions in a cycle | Final video review. Max two cycles per move, then the move goes to the owner with the gaps written plainly. |
| The same instruction fails once | Second attempt, and it must be at the `misurato` level. |
| The same instruction fails twice | Stop. It goes to the owner with before and after. |
| Two instructions in a row make the move worse | Back to the last good snapshot (`animpipe restore <move> <stamp>`), then the next gap. |
| After three instructions the problem is the idea of the pose | Early stop: a new starting pose is needed. Small steps do not save a wrong base. |
| The instruction touches an owner key or a locked part | It becomes a proposal to the owner. The executor does not apply it. |
| The defect is in the game (Anim BP, montage, blend) | `gioco`: cycle suspended, code thread. |
| Startup, active frames or FAF must change | Move design: the owner decides. |
| One fix breaks another view or another constraint | The reviewer states in the instruction which view or constraint wins (`vista`, `non_peggiorare`). |
| The owner edited keys in Unreal | Nobody rebuilds over them. See "State of the tools". |

## Keys with intent (owner, 2026-11-15)
There is no cap on the number of keys. Keys are used with intent, and where a curve can do the job the curve does it.
- **Curve first.** Tool order for any change: tangent or curve weight, then moving a key in time, then a tween between
  two neighbouring keys, and only then a new key.
- **Keys per control.** A key goes only on the controls that need it (the hand that keeps travelling, not also pelvis and feet).
- **Every key has a written reason.**
- **Never bake.** A key on every frame in a row, or keys on all controls where the curve between the neighbours gave the
  same result, are reported in the result as a warning (`segnali`). The jab 1 and jab 2 v3 cards (24 and 28 full keys) are
  the case to avoid.

## Who does what, once
| Work | Executor | Reviewer |
|---|---|---|
| Numeric gates (bone lengths, clearance, balance, elbows, hitstop snap, glued limbs, numeric silhouette, fidelity to owner keys) | Every step, all of them. | Reads the result line. Never re-measures. |
| The instruction's `verifica` | Runs it, writes PASS or FAIL with the value. | Writes it. Does not re-check it by eye. |
| Before/after grid with trails | Produces it. | Looks at it once. |
| Judgement by eye (pose reading, weight, quality bar, references) | Never. | Only the reviewer. |
| In-game video | Captures at start and end. | Watches those two. |
| What to fix next | Never. | Only the reviewer, one at a time. |

## State of the tools (2026-11-15, check `animpipe status` before trusting this)
- Exist on the animation branch: `author`, `build`, `check`, `fidelity`, `verify --req`, `poses --over`, `trails`, `diff`,
  `snapshot`, `restore`, `revert` (back to before the last import), `render --standalone`, `capture`, `casc-import|diff`.
- The card holds whole poses per frame and a spacing profile per segment (plus `part_delays`). It does **not** yet hold keys
  per single control or tangents per key. Until that work is done, "curve" means the segment's spacing profile and the
  part delays, and a key on some controls only is a pose whose other parts are the interpolated values, declared as such.
- `ue-import` and the `LS_<move>_Owner` / `LS_<move>_Agy` copies do **not** exist yet. Today the owner's Unreal edits survive a
  rebuild only on the additive "Manuale" layer (`animpipe capture`). An edit on the base section is lost at the next build:
  if the owner says he edited in Unreal, stop and ask before any build.
- Nobody writes keys into the sequence generated by the build. The card is the only truth.
