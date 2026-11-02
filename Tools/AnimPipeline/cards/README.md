# Move cards, one folder per kit

A **kit** is a moveset, the proxy of a character (in game any character can use any kit). Cards live in
`cards/<kit>/<kit>_<move>_v<N>.json` and the card's `move` field carries the same name, so every output
(`sheets/`, `Saved/AnimPipeline/animpipe/`, in-game previews) is prefixed by the kit.

| Kit | Character idea | Moves |
|---|---|---|
| `overlord` | "Elegant violence": composed boss, controlled power, arms crossed or behind the back (M. Bison, Rugal) | `ftilt` (from Ganondorf ftilt v3.1), `dair` (Ganondorf dair timing + Bison head stomp) |
| `pilot` | pipeline test moves | `jab` (Mario jab 1) |

The kit name is provisional (owner may rename). Unreal assets built before the kits keep their paths
(`/Game/AnimPipeline/Rig/AS_GanonFTiltV31_Build`); new ones keep the build folder `/Game/AnimPipeline/Rig/` with the kit in the name (`AS_OverlordDAirV1_Build`).

## Owner rules for authored moves (2026-11-02, from the overlord dair reviews)
Every card and every review pass applies these; the checks that enforce one are named.
- **Timing comes from the owner** with each reference (key frames, hitbox frames via `anim_active`); never derive it from a GIF.
- **"Too much is just right"** (Sakurai): push key poses to the extreme, never soften the owner's poses.
- **Frame 0 breaks clearly from the air/ground idle** but is not yet the charge; the charge is an extreme of an already loaded pose.
- **Body synergy**: arms, trunk and legs travel together on every segment; no limb glued while the body moves (`co_motion`).
- **Charge**: knees stay in front, tucked toward the chest, never swinging back behind the pelvis; a long charge stays alive.
- **Slow release** over a few frames, the largest step still ON the first hitbox frame; nothing (head included) snaps
  after an active frame (`hitstop_snap`, head turn counted). Hold the strike pose long.
- **Villain hand** (overlord): open, fingers straight and together (`fingers`), bent outward at the wrist off the arm
  line (`wrist_out`); hands in the owner's Cascadeur files are default, never copy them.
- **No interpenetration** accepted (`clearance`): fix forearms/head/crossed arms rather than accept them.
- **Feet**: side by side unless the pose says otherwise; check the back view for crossed ankles and frog knees.

## Disciplina di giro (plan 02/11 §8)
- Un giro = una modifica dichiarata (parti x frame) + `animpipe diff` + `review`; il resto resta sotto lock.
- Massimo 2 tentativi su una stessa correzione, poi handover con ciò che serve (regola 5 del progetto).
- Prima di toccare una parte si legge il blocco `approved`; se la correzione chiesta tocca una parte bloccata si chiede
  all'owner (una domanda sola) invece di sbloccare.

## Blocchi di scheda per pose approvate e regole
- `approved` `[{frames, parts, ref, by, date, why}]`: pose accettate dall'owner. Parti: `arm_l arm_r leg_l leg_r trunk head
  hand_l hand_r`. `ref` e' la scheda di riferimento (path sotto `cards/`); `load()` ferma `build/check/review` se le pose
  `author` ai frame bloccati differiscono dal `ref` oltre 0,5 cm / 1 grado (errore con frame, parte, campo). Il lock non
  scade con la versione: la scheda v5 nasce con gli `approved` di v4. Si supera solo con `--unlock part@frame` E togliendo la
  voce dalla scheda con un `why`.
- `animpipe approve <scheda> --frames=13,14 --parts=leg_l,leg_r --why="..."`: aggiunge la voce (`ref` = la scheda stessa) e
  scrive lo snapshot `cards/<kit>/approved/<move>.json`.
- `animpipe diff <scheda> [--vs=<scheda>]`: spostamento massimo per frame x parte rispetto alla versione precedente; da
  incollare nell'handover di ogni giro ("cambiato: ...; intatto (lock): ...").
- `rules` `{frame_0, knees_front, feet_side_by_side, villain_hand}`: le regole di sopra come check bloccanti, con le soglie
  nel blocco (`frame_0`: `charge, min_cm, max_frac`; `knees_front`: `frames, tol`; `feet_side_by_side`: `frames, min_cm`;
  `villain_hand`: `frames, min_deg`). Blocco assente = check non eseguito. Soglie da tarare sul desktop (dair v4, jab v12).
