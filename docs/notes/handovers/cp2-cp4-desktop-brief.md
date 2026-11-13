# Brief: passi desktop rimasti della pipeline (CP2 e CP4), 2026-11-03

Partire DOPO che il thread "Importare la dair dalle nuove chiavi di Cascadeur" ha finito (la scheda dair v4 è sua).
Leggere solo: questo brief, `Tools/AnimPipeline/cards/README.md` (sezioni Disciplina di giro, source_fidelity,
Revisione visiva), il brief "interpolazione fatta da Claude" in `anim-pipeline.md` e i file che si toccano.
Già su feature (e2e4b6f): PR #24, `animpipe fidelity` (`animpipe/fidelity.py`), `animpipe visual-gate`
(`animpipe/visual.py`), skill `.claude/skills/gstn-visual-review/`. Regole: diagnosi di 3 righe prima di ogni build,
max 2 tentativi, script ≤5 righe, handover ≤30 righe con costo.

## CP2 (Sonnet, desktop, editor)
1. `animpipe review --visual <scheda>`: UNA immagine `grid.png` (lato lungo 1568 px, niente mp4) + `manifest.json` in
   `Saved/AnimPipeline/visual/<move>/r<N>/` (N = giro successivo). Pannelli e campi del manifest: SKILL.md "Input".
   Riusare `capture_ue.py` / `ingame_judge`; il fantasma grigio = posa owner (`source`) sulle chiavi owner.
2. Jitter numerico nel `check` (accelerazione su frame non chiave, solo measure finché non tarato).
3. Prova su jab v12, dair v2, ftilt v3.1: poi un thread cloud fa la calibrazione (`calibrazione.md`).

## CP4 (Opus medium, desktop) = brief "interpolazione fatta da Claude" più:
- `casc-import --owner-keys`: ogni chiave owner entra intera (piedi compresi) con `origin: "owner"`, lock `approved`
  (`by: owner`, dita e `wrist_out` esclusi) e `source: {file: "cards/<kit>/refs/<move>_owner_bones.json", frame,
  tool: "casc-import"}`; il file delle ossa va copiato in `refs/` (in git). Con `origin: owner` `load()` blocca se la
  posa non è quella dell'owner (0,5 cm / 1°): verificare `_bones_errors` in `fidelity.py` sui dati veri (scritto senza
  editor, testato solo con dati finti).
- `merge()` in `cascadeur.py`: quando riscrive una posa che aveva `source` di tipo punti, aggiornare o togliere `source`
  (oggi resterebbe vecchio e `check` darebbe FAIL).
- Dair v4 attuale: `fidelity` mai eseguito con il rig vero. Il frame 31 ("f23 con assestamento di 2 cm") quasi certo
  fallisce: dichiararlo in `source.keep` con il why, oppure `derived`.
- Gate finale: `check` + `fidelity` → `review --visual` → skill (cloud) → `visual-gate` → `approve`. Prima mossa vera:
  la dair dalle nuove chiavi dell'owner (f0, f4, f8, f14, f15 hitbox, ultime 2 = FAF da confermare).

## Aggiornamento 2026-11-03 (dopo la skill pubblica, commit 0308102): il passo sulla dair vera parte dai verbi nuovi
- Ingresso: `python -m animpipe status overlord_dair_v4` e poi il `next` (skill `.claude/skills/animpipe-move/`,
  ex `gstn-visual-review` = `review.md`). Niente più comandi con il percorso della scheda né `visual-gate`/`capture`/`casc-*`.
- Cascadeur: l'owner preme **Invia** (non più "Esporta per Unreal"); la richiesta va in `casc/requests/` e la consuma
  `animpipe import <mossa>`. C'è già una richiesta dair in attesa (vecchia): chiedere all'owner se vale ancora.
- CP2 si innesta in `render` (`animpipe/verbs.py` `v_render`): oggi copia la griglia `poses`; sostituirla con il composito
  (fantasma owner, traiettorie, hitstop) mantenendo `visual/<mossa>/r<N>/grid.png` + `manifest.json`.
- CP4 si innesta in `import` (dita/mani dalle chiavi owner) e `interp` (pose `origin: claude` da file `--from`).
- Ok e approvazione dall'owner con **GSTN Studio** (`python -m animpipe.app`, tkinter): Ok, Approva, varianti, snapshot.
- Barra Cascadeur e riga di stato non ancora provate dentro Cascadeur: prima cosa da verificare (serve il server script,
  lo avvia l'owner).
