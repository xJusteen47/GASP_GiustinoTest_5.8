# Handover: down-air v8 e commit di Antigravity (2026-11-11)

## Fatto
- A. Worktree Antigravity `...\.gemini\antigravity\worktrees\GASP_Giustino2_feature\animate_3d_moves_unreal`, branch
  `experiment/3d-silhouette-anim`, nessun rebase/conflitto. Commit `899d7524` (card spostate in `cards/overlord/{dair,dropkick,ftilt,jab}/`,
  card jab, card dair v7, `move_sheets.json`; contiene anche le 3 note `docs/notes/anim-theory/*` + `handovers/anim-theory.md`, già in stage)
  e `21baa9c3` (verbi animpipe, `ue.py`, script jab/UE, `user_tweaked_keys.json`). Push: nuovo ramo remoto `experiment/3d-silhouette-anim`.
  Esclusi (non committati, lasciati sul disco): `renders/` (2.5 MB di png di prova), `test_f27_az180.png`. Nessun segreto trovato.
- B. `docs/notes/anim-review-loop.md` su `claude/desktop-anim-workflow`, commit `70a2a9c5`. `anim-process.md` non toccato;
  4 differenze sotto "Da decidere".
- C. v8: **non generata**. Due chiamate `agy-bridge` `delegate` (sola lettura, modello "Gemini 3.8 Flash (High)") tornate vuote:
  "agy returned empty output". Fermato dopo 2 tentativi.

## Causa probabile del blocco
- Config del bridge in `C:\Users\jacop\.claude.json`: `AGY_SKIP_PERMISSIONS=false`, `AGY_SANDBOX=true`. Con permessi non saltati
  `agy -p` (headless) non può approvare la lettura dei file e finisce senza output. `--sandbox` probabilmente blocca anche la
  remote execution verso Unreal che `animpipe build` usa.
- Nessun Unreal Editor aperto ora. Un `agy.exe` (pid 35344) è in esecuzione: non toccato.

## Stato v8
- Ultima versione: v7 (`cards/overlord/dair/overlord_dair_v7.json`, `AS_/LS_OverlordDAirV7_Build`). Nessun file cita v8.
- Non si sa ancora cosa deve cambiare la v8 né quale script la genera: è la domanda fatta ad Antigravity.

## Domande per l'owner
1. Sblocco bridge: metti `"AGY_SKIP_PERMISSIONS": "true"` (e, per far girare Unreal, `"AGY_SANDBOX": "false"`) nella voce
   `agy-bridge` di `C:\Users\jacop\.claude.json`, poi riavvia la sessione. Oppure fai generare la v8 ad Antigravity dalla sua app.
2. Per `build`/`review` serve l'editor aperto sul worktree di Antigravity: quale progetto apri e quando?
3. I 4 punti "Da decidere" in `anim-review-loop.md`.

## Costo
- Thread: ~90k token di contesto Claude; 2 chiamate Antigravity a vuoto (quota Gemini 3.8 Flash minima o nulla).
