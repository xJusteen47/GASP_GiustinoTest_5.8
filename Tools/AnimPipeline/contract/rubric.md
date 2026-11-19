# Rubric: the one list a move is judged by

Replaces the three lists that existed (the old `gstn-move-animation` skill, `animpipe-move/rubric.md`, Antigravity's audit).
Used by the reviewer only. The executor never judges.

## The quality bar (owner, 2026-11-15)
> «Questa animazione è alla pari di una della qualità di Smash Bros o di altri giochi tripla A simili?» Se no, si rivede.

Asked at every review, always in the same way: the six questions below, yes or no, on the **in-game video**, from behind
and from the side. Then the comparison with two references of the same class.

**Verdict.** PASS only if all six are yes AND, next to two reference moves of the same class, the move does not look out
of place. Otherwise FAIL: list at most the **three largest gaps**, each with frame, principle and correction. Doubt between
yes and no is a no. Only a PASS goes to the owner.

## Six questions for an attack
| id | Question | Yes looks like | Typical no |
|---|---|---|---|
| `impatto` | Freezing the hit frame at game distance, is it clear what hits and where, without knowing the move? | The hitstop frame is a clean key pose. The striking part leaves the torso outline from the side and stays visible from behind; the other arm never covers it. The line of action agrees with the knockback. In the hit capture the striking part overlaps the dummy. | A passing pose frozen. Arm hidden behind the trunk from behind. The hit visibly misses the target. Two arms forward in one outline. |
| `tempo` | Is the largest step on the first active frame, with a still hold right after? | Long build, then the limb arrives in 1-2 frames (reference: at least 40% of its path in the last 2 frames), no braking before contact. Then a hold as long as the class asks; on strong moves alive with a small decaying recoil. | Even spacing. The largest step before or after the first active frame. The limb retracting at once. A strong move frozen like a statue. |
| `primo_frame` | Is frame 1 already a pose different from the one before? (feel rule) | The first video frame of the move shows a clear change from idle, or from the hold of the previous move in a chain. No soft blend-in. Not yet the charge. No still frame right after. | First frame equal to idle or to the previous hold. Still, jump, still, go. |
| `peso` | Does the whole body take part, not only the limb that hits? | A preparation the opposite way. Pelvis, shoulders and support foot work; the centre of mass travels with the strike on a smooth path while the limb is sharp; the body and the other arm counter-move. | A limb glued while the body travels. Arms as passive cargo. The trunk pushed like a doll. |
| `recupero` | Is the return readable, without pops, with the body where it must be at FAF? | The limb leads (fist, then shoulder, then trunk 1-2 frames later). Every swing has a cause and is smaller than the last. At FAF the body is ready and close to idle; the tail after FAF keeps playing cleanly until it ends or a new action cancels it. | The body still far from the capsule at FAF. A limb dropping in two frames then nothing. A pop after an active frame. |
| `pulizia` | No dead frame, no interpenetration, no pose jump at entry and exit? | Smooth arcs for hands, feet and head. Elbows down or back, never reverse-bent; knees over the toes; wrists in line unless exaggerated in the direction of the force. Feet planted. | A dead frame. A forearm in the head. Zig-zag on an arc. A crouch or a jump when the montage ends (that one is `gioco`). |

For each no write: `v` frames, view, what is seen in one sentence, the principle (`glossary.md`), and whether it is a pose
defect or `gioco`. The method behind every answer is `principi-animazione-corporea.md` §13 (intent, prediction from the
principle, measure on the frames, explanation) with the questions of `animazione-picchiaduro-action.md` §10.

## Five questions for a reaction, a held pose or a loop (provisional, not calibrated yet)
| id | Question |
|---|---|
| `primo_frame` | Does the first frame already show the hit taken, with no soft blend-in? (feel rule) |
| `direzione` | Does the body go where the hit sends it, readable from behind and from the side? |
| `continuita` | Between one clip and the next (stun, fall, bounce, lie, get up) is there no pop, no slide, no dead frame? |
| `peso` | Does the body absorb contacts with the ground, and does the centre of mass follow a believable path in the air? |
| `pulizia` | No interpenetration, no feet sliding, and a held pose or loop that stays alive without wobbling? |

## Comparison with references
- Two references of the **same class** (jab, tilt, smash, aerial, jump kick, reaction). The class, not the move: never trace
  a recognizable move pose by pose.
- Timing structure against the class range, limb extension per frame against the reference curve, then phases side by side
  (first frame, charge, contact, recovery). Numbers live in `Tools/AnimPipeline/reflib/` when it exists; until then:

| Reference (startup / active / total) | Measured |
|---|---|
| Mario jab 1: 2 / 2-3 / 19 | |
| Terry ftilt: 8 / 8-13 / 27 | 1-frame snap, 5 frozen hold frames |
| Ganondorf ftilt: 10 / 10-12 / 39 | 6-frame build, 90% of the path in the last 2, about 9 hold frames at 69% extension, no recoil |
| Ryu fsmash: 15 / 15-17 / 45 | 78% of the path in the last 2 frames, 4 decaying recoil reversals of about 9% of reach |
| Little Mac fsmash: 14 / 14-15 / 43 | settles at 72%, frozen 12 frames |
| Hold frames by class | jab 1, tilt 3, smash 6 or more |

- Smash references are **side views**. From behind there is no Smash reference: judge with `silhouette-multidirezionali.md`
  and the owner's own reference clips. Say in the review which view each answer comes from.
- Images, GIFs and videos of other games stay on the desktop (`Saved/AnimPipeline/refs/`). Never in the repo, in an
  artifact or in a reply. Numbers may be written anywhere, with the source. Never use animation or model files taken from a game.

## What the eye does not judge again
Numbers the gate already has (bone lengths, clearance, balance, elbows, hitstop snap, glued limbs, contact and FAF, locks,
fidelity to owner keys, numeric silhouette, and every `verifica`) are read from the result line. The eye looks for what the
numbers miss. The one measure the reviewer takes itself is on the in-game video, where no gate exists: changed pixels
between consecutive frames, to find a dead frame or a first frame that does not change (`rv_frames.py --diff`).

## Grid items (ids used by `verdict.json` in the `animpipe-move` grid review)
`regola` in the verdict is the id in the first column. "Blocca" says when the item is `gravita: blocca`; otherwise `nota`.

| id | Where to look | What is wrong | Blocca |
|---|---|---|---|
| frame_0 | side grid f0 vs idle | f0 looks like the idle, or is already the charge | f0 indistinguishable from idle |
| troppo_giusto | key poses, game camera | owner's extreme softened, pose timid compared to his ghost | the pose is visibly smaller than the owner ghost |
| sinergia | side grid, startup | a limb stays glued while trunk/legs travel (arms parked on the chest) | a limb still for 3+ frames while the body moves |
| ginocchia_avanti | side grid charge | knees swing back behind the pelvis during the charge | always |
| carica_viva | side grid, charge hold | a long charge frozen like a statue | no: nota |
| rilascio | hitstop strip + trails | release not slow, or the largest step is not ON the first active frame | biggest step not on the first active frame |
| niente_scatto | hitstop strip, trails after active | head or a limb pops after an active frame (gap in the dots) | a visible jump in a trail after an active frame |
| tenuta | side grid, strike | strike pose not held long (stomp/hold poses) | held fewer frames than the card's active + hold |
| mano_villain | key poses, overlord kit | hands not open-flat, fingers apart, wrist not bent outward, thumb not along the index | on overlord cards: always |
| pugni | key poses, other kits | fists not closed (default) | always, unless the card says open |
| gomito_dentro | back view | back elbow sticking out of the torso silhouette | no: nota |
| compenetrazione | all panels | forearm in the head, arm through the chest, foot through the thigh | always (clearance may have an owner exemption: then nota) |
| piedi | back view | feet not side by side when the pose does not ask it; crossed ankles or frog knees not in the owner pose | when it differs from the owner ghost |
| silhouette | key poses, game camera | the move does not read as itself (a kick read as a punch, an arm hidden behind the body) | the striking part is not recognizable from the game camera |
| parte_che_colpisce | game camera, active | the hitting part is covered or not leading | always |
| intermedio_manca | side grid + trails | a jump between two frames: dots far apart with no in-between, shape changes in one frame | over 10 cm in a frame outside the active window (segment `manca`) |
| intermedio_brutto | side grid between keys | in-between inside the body, knee bending backwards, limbs collapsing to mush | always (segment `brutto`) |
| jitter | trails | dots zig-zag or wobble on non-key frames | a visible back-and-forth on 2+ consecutive frames |
| fantasma_owner | grey ghost on owner keys | the coloured pose does not cover the owner ghost | never correggi: `chiedi_owner` if over ~1 cm visible |

A problem on an owner key or a locked part is never corrected: it is written down and goes to the owner.

## Calibration (redo whenever this file or the reviewer skill changes)
Two moves known good and one known bad must come out right before verdicts are trusted: jab v12 and ftilt v3.1 pass, dair
v2 fails on `peso` (arms fixed on the chest f6-12), `recupero` (head pops after active) and `pulizia` (forearm in the head).
Record date and result in `.claude/skills/animpipe-move/calibrazione.md`. Not yet run on in-game video.
