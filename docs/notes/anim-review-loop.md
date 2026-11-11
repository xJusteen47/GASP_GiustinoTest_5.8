# Ciclo "Antigravity anima, Claude rivede" (regole pratiche)

Stato: 2026-11-11. Documento separato e complementare a `docs/notes/anim-process.md` (branch `experiment/3d-silhouette-anim`),
che resta il processo ufficiale e non si modifica senza l'OK dell'owner. Qui ci sono solo le regole del ciclo di revisione.
Dove i due documenti divergono, la differenza è elencata in fondo sotto "Da decidere", senza sceglierla.
Pipeline e formato di correzione: `docs/notes/anim-workflow-antigravity.md`.

## 1. Chi fa cosa
1. **Antigravity** (o un modello economico) anima con la sua pipeline: card JSON → `animpipe author` → `build` → `check` → `review`.
   Lavora nel suo worktree e committa card e script a ogni versione (una versione = un commit, con una riga "cosa cambia").
2. **Claude rivede** guardando un video **in-game reale**: PIE, mossa lanciata attraverso la sua ability, con Mover, timing
   dell'ability e camera di gioco. Il video scrubbato di `L_PilotCapture` serve ad Antigravity, non sostituisce questo.
3. **Claude scrive la correzione** in `docs/notes/handovers/<YYYY-MM-DD>_<move>_review.md` con il template del §2.
4. Antigravity applica la correzione sulla card e produce la versione successiva. Si ripete.

## 2. Revisione
- Materiale per ogni variante, in `docs/notes/reviews/<variante>/`: mp4 in-game della variante e della base, griglia degli stessi
  frame per entrambe, path della card e commit, frame di hit e FAF.
- Si estraggono i frame del video e si guardano uno per uno attorno all'hit e al FAF, confrontando con la base e con le regole
  dell'owner (nessun pop dopo i frame di hitbox, gomiti e articolazioni naturali, equilibrio, entrata da idle eretto).
- Esito: PASS o FAIL. Il verify numerico (`check`, read-back ossa) non sostituisce mai lo sguardo sul video.
- Se l'owner dice che il video non torna, si controlla prima la catena di cattura, poi l'animazione.
- Template della correzione:
  ```
  # Review: <move> (v<N>)  Verdict: REVISE | APPROVE
  - Frame <F> | part `<part>`: rot [rx,ry,rz], loc [x,y,z]   (assi card: +X sinistra, +Y avanti, +Z alto; +rx inclina indietro)
  - part_delays: {"<part>": <frames>} | spacing: <preset>
  - Exemptions: contact <bone_a>|<bone_b> frames <a>-<b> (motivo)
  ```
  Correzioni a livello di card (frame di posa, parte, valore), mai per frame cotto.

## 3. Regola dei keyframe (owner)
- Default: **key sparsi + curve di interpolazione editate**, così l'owner può modificare le pose a mano sul `LS_<Move>`.
- Key per singolo controllo dove serve (es. solo il polso, solo la testa), non per forza su tutto il corpo.
- Key densi accettabili per mosse molto veloci o per dettaglio su pochi controlli.
- Mai un bake "un key per frame" su tutto come sorgente editabile. L'`AS_` cotto è un output, la sorgente è card + `LS_`.

## 4. Cosa arriva all'owner
- Niente arriva all'owner se nessuno ha guardato il video in-game. Una variante non guardata si presenta come "non rivista".
- Solo PASS va all'owner, con video, griglia e cosa è cambiato rispetto alla versione precedente.

## Da decidere (differenze con `anim-process.md`)
1. `anim-process.md` §4.A vuole ogni mossa aerea con impatto in 3 moduli (`Startup/Loop/Land`); il down-air v1-v7 è una clip unica.
   Vale anche per il dair, o il dair resta clip unica?
2. `anim-process.md` §2 prevede come verifica di consegna un "report visivo multi-camera"; qui il cancello verso l'owner è il video PIE
   in-game. Il report multi-camera resta come controllo interno di Antigravity o va sostituito?
3. `anim-process.md` §1 assegna a Claude solo l'integrazione (GAS, NP, hitbox); qui Claude è anche revisore delle animazioni.
   Va aggiunto il ruolo nel processo ufficiale (serve l'OK dell'owner e un backup prima)?
4. `anim-process.md` non dice nulla su key sparsi vs bake; la regola del §3 va riportata nel processo ufficiale?
