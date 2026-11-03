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

## Hand tweaks in Unreal: the "Manuale" layer
`animpipe build` deletes and recreates `LS_<name>` and the clip from the card, so keys edited on the base section are
lost. Every build adds an **additive** section ("Manuale") on the Control Rig track, over the whole clip, and makes it the
section to key. Before rebuilding it reads that section's keys and puts them back, so the clip and all checks include them.
1. Open `/Game/AnimPipeline/Rig/LS_<name>` (e.g. `LS_OverlordDAirV4_Build`). The rig track has two sections: the
   base one (from the card) and the additive one, already the section to key (right click > "Set as section to key" if not).
2. Pose a control on a frame and key it: the key goes on the layer and adds to the card's pose. Key the frames around the
   tweak at 0 so it fades in and out. On this rig the arms are FK (`*_fk_ctrl`), the legs IK (`foot_*_ik_ctrl`).
3. Save the sequence, then `python -m animpipe build <card>` and `check` (or `review`): the build prints `manual layer
   {keys: N}`. A copy of the layer is kept in `Saved/AnimPipeline/animpipe/<move>_manual_layer.json` (used only if the
   sequence is gone). To drop a tweak, delete its keys on the layer and rebuild.
Card changes and layer tweaks stack: if a card change moves the same control, re-check the tweak.
4. `python -m animpipe capture <card>` (or `--from=<file>`) copies the layer backup into git as `cards/<kit>/manual/<move>.json`
   and adds the card block `manual` (file, by, date, channels, keys, parts -> frames). Commit both.
5. Build fallback for the layer keys: the sequence `LS_<name>`, then `Saved/.../<move>_manual_layer.json`, then the card copy.
6. `diff` adds "manuale sotto una posa cambiata: arm_r 5 (ricontrollare)" when a changed pose is at (or +-1 frame from)
   a manual key, else "manuale: intatto"; `animpipe card` prints the keys per part.
Do not undo past your own edits: the editor's Ctrl+Z history also holds the build ("Bake to Control Rig", "Add actor"),
and undoing it removes the rig track from the open sequence. Delete a key to drop a tweak; if it happens, close without saving.

## Editing in Cascadeur: sparse keys, back into the card
The card stays the source of truth; Cascadeur is the editor (keys only on the card's key frames, never baked curves).
1. `python -m animpipe casc-export <card>`: the card's take -> FBX, and a fresh `Saved/AnimPipeline/casc/<move>.casc` on the
   UEFN rig (refused if one exists: it may hold your edits; `--force` keeps it aside).
2. In Cascadeur (script server on): open that `.casc`, run `Tools/AnimPipeline/casc_prepare.py` with `PLAN=` the
   `<move>_export.json` it printed. It imports onto the selected joints with up axis Z, deletes every non-key frame,
   sets clamped bezier, saves and exports `<move>_casc.fbx`. Then `casc-import <card> --ref` (Cascadeur's readback, no edit).
3. Edit in Cascadeur (move keys' poses, add keys), save, File > Export the joints (or export_joints) to `<move>_casc.fbx`.
4. `casc-import <card> --keys=<Cascadeur key frames>`: only the parts you changed (arm_x, leg_x, trunk, head, vs the
   readback, 0.5 cm / 1°) are rewritten; the rest stays byte for byte. A new key becomes a pose. Locked (`approved`)
   parts stop it (`--unlock=part@frame`). Fingers and `wrist_out` stay the card's. Then `author`, `build`, `check`.
5. `casc-revert <card>` puts back the card as before the last import (snapshots in `cards/<kit>/history/`);
   `casc-diff <a> <b>` lists the key frames and parts that differ between two cards.
6. Spacing like the build: `casc-spacing <card> --fit` (plan; run `casc_spacing.py` in Cascadeur on a `<move>_spacelab.casc`
   copy: per segment and layer it picks interpolation + easing weights against the build's bones), then `--apply` (writes
   them into `<move>.casc`, refuses if a key pose moved), `casc-import --ref` again, `--score` (worst cm per segment).
   Keys are never added or moved, so import, locks, revert and diff are unchanged.
Without step 6 the in-betweens you see in Cascadeur are its own curves (a preview): the build keeps the card's spacing. A trunk bend that
the card cannot hold (all in one vertebra; the card spreads it on five) is reported as residual > 1 cm, never hidden.
