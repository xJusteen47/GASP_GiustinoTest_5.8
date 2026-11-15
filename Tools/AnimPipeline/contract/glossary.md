# Glossary: frames, space, parts, rig controls, words

## Frames
| Tag | Counts | Used by |
|---|---|---|
| `f<N>` | Anim frames of the card (60 fps, no hitstop) | Card, gates, instructions (`chiave.anim`) |
| `v<N>` | Frames of the in-game video, 0-based, printed on every video and grid cell | The owner, the video review |
| `m<N>` | Frame of the timeline that plays the move | Capture labels |
| `a<N>` | Frame of the montage | Capture labels |

`v` and `f` differ by the hitstop freeze. Never translate by hand: pass the owner's numbers with `--video` and quote the
`frame_map` of the result (`animpipe-move` section 4b). A frame inside the freeze is refused.

## Space and directions
- Character (component) space, the one of the bone dump `sheets/raw/<take>_legbones.json`: **X left, Y forward, Z up, cm**.
- Words in `modifica`: avanti (+Y), indietro (-Y), sinistra (+X), destra (-X), su (+Z), giù (-Z); fuori = away from the
  body's midline, dentro = toward it. Rotations in degrees, with the axis and the control.
- Views: `dietro` = az180 and the game camera (the one the owner looks at most); `lato` = az270 or az90; `camera` = the
  in-game camera. A silhouette cannot be good from every angle: the instruction says which view wins.

## Parts and controls
Parts (the only values of `parte`, the same as the card locks): `arm_l arm_r leg_l leg_r trunk head hand_l hand_r`.

| Part | Controls | Note |
|---|---|---|
| `arm_<s>` | `upperarm_<s>_fk_ctrl`, `lowerarm_<s>_fk_ctrl`, `hand_<s>_fk_ctrl` | Arms are FK on this rig. |
| `hand_<s>` | `hand_<s>_fk_ctrl`, `hand_<s>_ik` | Fingers and `wrist_out` on owner keys are the owner's. |
| `leg_<s>` | `foot_<s>_ik_ctrl`; `thigh_<s>_fk_ctrl`, `calf_<s>_fk_ctrl` | Legs are IK: move the foot target, not the thigh, unless told. |
| `trunk` | pelvis, `spine_04`, `spine_05` / chest | A trunk bend is spread on five vertebrae by the card. |
| `head` | head, neck | |
| whole body | `global_ctrl` | Not used for attacks: the capsule does not move in the clip unless the card says so. |

`<s>` is `l` or `r`. The authoritative list of control names is the `controls` array of `<move>_build_ue.json`, written by
`animpipe build`. **Copy names from that list. Never write a control name from memory**: if the one you need is not there,
the instruction is `ambigua`.

## Words for `intento` (operational meaning, and where the theory is)
`corp` = `principi-animazione-corporea.md`, `picch` = `animazione-picchiaduro-action.md`, in `docs/notes/anim-theory/`.

| Word | Means | Theory |
|---|---|---|
| primo frame | The first frame of the move is already a pose clearly different from the one before. No soft blend-in. | picch §1, owner's feel rule |
| anticipazione | A preparation the opposite way, proportioned to the action and to the startup available. | corp §5 |
| carica | The loaded extreme before the release; stays alive if long. | corp §5, picch §1 |
| spacing | How far a part travels per frame. A strike grows until contact: most of the path in the last 1-2 frames. | corp §2 |
| scatto | The largest step of the move. It falls ON the first active frame. | picch §2 |
| posa d'impatto | The pose the hitstop freezes: a clean key, readable at game distance, not a passing pose. | picch §2.2 |
| tenuta | The strike pose held after contact (moving hold: alive, not frozen on strong moves). | corp §8, picch §5 |
| follow-through | The part keeps travelling past the stop, then returns. Delayed, decaying, it ends. | corp §7 |
| overshoot / settle | Going past the target and coming back, each swing smaller than the one before. | corp §6 |
| overlap / drag | Parts start and stop in sequence, from the centre out, 1-2 frames apart. | corp §7.2 |
| rientro (recupero) | The return to guard. Led by the limb: fist first, shoulder next, trunk 1-2 frames later. | picch §1, §2.3 |
| peso | The whole body takes part: pelvis, shoulders, support foot; the centre of mass moves with the strike. | corp §4 |
| arco | The path of hand, foot or head is a smooth arc; a change of direction has a reason. | corp §9 |
| silhouette | At game distance the outline says what is happening; the striking part leaves the torso outline. | corp §11, picch §4 |
| linea d'azione | One clear line through the body, different between charge and strike, coherent with the knockback. | corp §11 |
| frame morto | A frame equal to the one before where the move should be moving. | picch §10 |
| pop | A visible jump of a part between two frames outside the active window. | corp §12 |
| twinning | Both arms or both legs doing the same thing by accident. | corp §12 |
| tangenza | Two outlines touching so that a limb reads as part of the body. | corp §11 |
| coperta corta | Fixing one view or constraint breaks another. The instruction says what wins. | |
| gioco | A defect of the game (Anim BP, montage, blend), not of the clip. Not for the executor. | |
