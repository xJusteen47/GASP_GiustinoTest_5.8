---
name: animpipe-move
description: Turn an animator's key poses into a finished, checked move (import, interpolate, check, render, review, variant, approve) with the animpipe commands. Use when asked to import, interpolate, review, fix or approve a move, or to "continue" a move.
---

# animpipe-move: follow `status`, one verb at a time

The order of the steps is not in this file: `animpipe status <move>` says which verb comes next. Run every command from
`Tools/AnimPipeline` as `python -m animpipe <verb> <move> [--options]`. Each prints ONE json line
`{ok, verb, move, exit, result, next}`; the full detail is in the file named by `detail`. Read the line, not the detail,
unless a step below tells you to.

## 1. Setup
Run `animpipe status <move>`. Read only: the move card (`cards/<kit>/<move>.json`: `_design`, `poses`, `approved`),
`rules.md` and `kits/<kit>.md` in this folder, and the last result file if you need it. Never old logs, threads or chats.

## 2. Loop: run the `next` verb
| exit | meaning | you do |
|---|---|---|
| 0 | done | run the new `next` |
| 1 | a gate blocked | write a 3-line diagnosis (what failed, where, why), fix ONE thing, run `next` again. Max 2 attempts per gate; `status` says `ask_owner` after that |
| 2 | wrong usage | read `result.error`, correct the command; never guess options |
| 3 | needs the owner | stop and ask the owner (section 4); do nothing else on the move |
| 4 | needs an app | ask the owner to open the engine or Cascadeur (never start or configure them yourself) |

What each `next` asks of you:
- `import`: run it; it reads the owner's "Invia" from Cascadeur. Owner keys come in whole, never moved.
- `interp`: write the in-betweens you propose to a json file `{"poses": [{"frame", "why", "author": {...}, "spacing"?}]}`
  and run `interp <move> --from=<file>`; only free frames are accepted, each pose needs a `why`. If none is needed:
  `interp <move> --none --why="..."`. `--drop=6,11` removes your own in-betweens.
- `build`, `check`, `export`: run them as they are.
- `render`: run it; it writes `visual/<move>/r<N>/grid.png` + `manifest.json`.
- `verdict`: do the visual review (section 3), then run `verdict <move>`.
- `owner-ok`, `variant apply|discard`, `ask_owner`: the owner decides (section 4).
- `approve`: run it only when status says so (the owner's ok is already recorded). `done`: report and stop.

## 3. Visual review (only if you can read images)
If you cannot look at an image file, stop with "needs vision" and hand the round folder to the owner. Otherwise follow
`review.md`: look at `grid.png` once, go through `rubric.md`, write `verdict.json` (`verdict.schema.json`), run
`verdict <move>`. Max 2 rounds per move.

## 4. Owner contact
Send the owner the round's `grid.png` and a 5-line summary (what changed, gates, verdict, the question), in the owner's
language (`owner_lang` in `animpipe.toml`). Record his own words with `owner-ok <move> --quote="<his words>"`, then run
`approve` if status says so. Variants: one per round (`variant new <move> --why=`), built and checked like a move named
`<move>_var<A>`; the owner applies or discards it (`variant apply <move> --quote="<his words>"`), never you on your own.

## 4b. Video frames first (before ANY change to the timeline)
The owner counts frames of the in-game video (`v<N>`, 0-based, printed on every video and grid cell; owner keys are starred in
red); the card counts anim frames. They differ by the hitstop freeze. Before you touch a key, an in-between, a lock or a
render, write the map "requested frame -> key / segment touched" and put it in your message to the owner, e.g.
`v13 = anim 13: between keys 11 and 14`. Never translate by hand: pass his numbers with `--video`
(`render|approve --frames=13,14 --video`, `interp --drop=13 --video`, `interp --from=f.json --video`); the result's
`frame_map` is the map to quote. A frame inside the freeze is refused. If the map shows an owner key, stop (section 5).
In-game video without the owner's editor: `render <move> --standalone [--azs=90,0,45]`. Numeric requirements:
`verify <move> --req=file.json` (format in `animpipe/verify.py`), PASS/FAIL per line, use it instead of judging by eye.

## 5. Hard rules (the commands refuse them too)
- Never move or edit an owner key or a locked part: propose the change to the owner instead.
- Fingers and hands on owner keys come from the owner's keys.
- One variant per round; the original card changes only on the owner's "apply".
- Never delete snapshots; `restore <move> <date>` undoes any write, `snapshot <move> --list` shows them.
- No third review round. No edits to app settings, no new tools installed.
