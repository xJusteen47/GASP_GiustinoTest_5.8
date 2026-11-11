# Handover: jab Overlord da zero, card pronte (ANIM-JAB-1, 2026-11-11)
Branch `claude/desktop-anim-jab-1` (da `claude/desktop-anim-workflow`). Nessun editor aperto, nessuna build.
**Niente è ancora una variante finita: nessuno ha visto un video in-game** (regola 10).

## Fatto
- Design: ritrovati nel log di Antigravity la descrizione dell'owner (8/11) e il suo video di riferimento; l'owner l'ha confermata.
  Jab 1 = slap di dorso col destro, sinistro piegato sul ventre. Jab 2 = schiaffo ad artiglio in affondo, posa tenuta in cui si guarda
  la mano, poi mano che trema e ritorno alla idle. Due asset separati.
- Diagnosi del jab di Antigravity: `docs/notes/reviews/overlord_jab_v2/diagnosi-jab-v1.md`.
- Card in `Tools/AnimPipeline/cards/overlord/jab/`: `overlord_jab1_v2.json` (29 f, key 0/4/6/8/13/22/28, attivo 6-8, FAF 22) e
  `overlord_jab2_v2.json` (53 f, key 0/6/9/11/20/26/38/52, attivo 9-11, FAF 26). Timing CSV in `timing/` (copiati in
  `Saved/AnimPipeline/refs/ufd/` di m7 e del repo principale). `animpipe author --no-write`: PASS lunghezze ossa su entrambe.
- `Tools/AnimPipeline/stick_preview.py`: risolve le pose offline e disegna una griglia stick figure; `--plant` ricalcola le gambe per i
  piedi fermi, `--roll` scrive la rotazione dell'avambraccio. Griglie guardate una volta: `reviews/overlord_jab_v2/*_stick.png`.
- UE 5.9: installato in `C:\Program Files\Epic Games\UE_5.9` (5.9.0, editor e plugin Mover/NetworkPrediction/GameplayAbilities presenti).
  Nessuna copia del progetto con EngineAssociation 5.9 in `Documents\Unreal Projects` (tutte 5.8).

## Scelte di design (da confermare sul video)
Colpisce il destro. Piedi fermi in entrambi i colpi (affondo senza passo). f0 jab 1 = idle; f0 jab 2 = posa tenuta del jab 1 (il cancel
parte da lì). Il colpo arriva sul primo frame attivo, poi solo deriva (niente pop dopo l'hitbox). `part_delays` 0. `fingers` vale per
tutte e due le mani: anche la sinistra sul ventre è semiaperta.

## Passi rimasti (con editor, dopo l'OK del coordinatore)
1. Editor 5.8 su un worktree con `Content/AnimPipeline` (`GASP_Giustino2`, `_m7`, `_feature`), con queste card.
2. `python -m animpipe author <card>` → `build` → `check` per le due card; max 2 correzioni per problema.
3. Tremore jab 2: key solo su `hand_r_fk_ctrl` (Rotation, ±4° che decade a ±0,5°, f27-f37) sulla sezione additiva "Manuale" di
   `LS_OverlordJab2V2_Build` (campo `_tremor` della card). Non esiste ancora.
4. `AM_` di anteprima, aggancio all'ability del jab (tasto X), mp4 PIE con camera di gioco + griglia in `reviews/overlord_jab_v2/`,
   poi thread di revisione che guarda il video. Non verificabile offline: compenetrazioni, dita, arco dello slap f4-f6, lettura in camera.

Costo: circa 165k token di contesto Claude, nessuna chiamata ad Antigravity. Output non tracciati di `author --no-write` rimasti in m7.
