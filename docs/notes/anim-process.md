# Processo di animazione condiviso (modello leggero + Claude)

Questo file lo leggono tutti gli agenti che toccano le animazioni (Antigravity, Claude). Le regole le decide solo l'owner.

## 1. Regole del vecchio processo (da rivedere)
**Non vincolanti per Antigravity finché l'owner non conferma.** L'owner usa regole nuove che possono contraddirle e che trova più funzionali: dove le nuove regole sono diverse, valgono le nuove. Queste restano qui come promemoria dei danni che evitavano (chiavi owner sovrascritte, video sfasati, lavoro perso fuori da git).

1. **Video prima di "applica".** Nessuna variante va sulla carta principale senza che il video in-game sia stato guardato frame per frame (dall'owner o da una revisione cloud). I numeri (`check`, `verify`) non bastano.
2. **Chiavi dell'owner bloccate.** Non spostare mai le pose chiave dell'owner (dair: 15, 31, 37, 42) né le parti nei blocchi `approved`. Niente `--unlock`. Se una correzione le tocca, chiedi all'owner.
3. **Frame = frame di ANIMAZIONE, contati da 0.** Il video in-game ha 8 frame di hitstop dopo lo stomp con la stessa etichetta `f<N>`: non contare i frame del video.
4. **Varianti separate.** Ogni tentativo è una variante (`cards/<kit>/<kit>_<mossa>_v<N>_var<X>.json` o copia equivalente). La carta principale cambia solo dopo "applica" dell'owner; "scarta" la lascia com'è.
5. **Ogni giro buono finisce in git.** Quello che è solo nell'editor si perde con un crash o con `animpipe build` (che ricrea `LS_<nome>`). A fine giro: carta variante (o layer "Manuale" salvato) + video e griglie in `docs/notes/reviews/<variante>/`, commit.
6. **Cattura corretta prima di fidarsi dei render.** Serve il fix `3f3d01e` di `standalone_capture.py` (ramo `claude/project-thread-0pafeo`, non ancora in feature): senza, ogni immagine mostra la posa del frame precedente. Controllo: nel video lo stomp della dair deve cadere su `f15`.
7. **Una modifica per giro, max 2 tentativi** sulla stessa correzione; diagnosi di 3 righe (cosa fallisce, causa, modifica) prima di ogni build.
8. **Non salvare mappe** e non toccare il checkout principale `GASP_Giustino2` (editor dell'owner).
9. Regole di stile dell'owner: `Tools/AnimPipeline/cards/README.md` (sezione "Owner rules").

## 2. Processo attuale (Antigravity) — da compilare dal modello
- Strumenti usati (animpipe? editor via unreal-mcp? Cascadeur?):
- Dove salva il lavoro di ogni giro (carta, asset, file):
- Come produce video e griglie, e dove li mette:
- Come numera i frame nei suoi rapporti:
- Come evita le chiavi dell'owner e i blocchi `approved`:
- Ramo git su cui lavora:
- Cosa fa diversamente dalla pipeline `animpipe` e perché è meglio:
- Regole nuove dell'owner che usa (anche se contraddicono la sezione 1):

## 3. Log (una riga per variante prodotta, la più recente in fondo)
Formato: `AAAA-MM-GG HH:MM | autore | mossa/variante | frame toccati | dove stanno video e griglie | esito`
- 2026-11-06 15:15 | Claude (cloud) | — | — | revisione del processo: `docs/notes/reviews/anim-process-review-2026-11-06.md` | file creato
- 2026-11-06 15:16 | Claude (cloud) | — | — | — | sezione 1 resa non vincolante finché l'owner non conferma
- 2026-11-06 15:35 | Claude (cloud) | — | — | — | il processo vero di Antigravity sta nel suo worktree `C:\Users\jacop\.gemini\antigravity\worktrees\GASP_Giustino2_feature\animate_3d_moves_unreal` (ramo `experiment/3d-silhouette-anim` = f6c5659), file `docs/notes/anim-process.md` lì. Le sue regole hanno la precedenza; sotto solo i 3 conflitti che fanno danno.
- 2026-11-06 15:35 | Claude (cloud) | cattura | — | — | DANNO 1: il worktree è su f6c5659, PRIMA del fix `3f3d01e`: `standalone_capture.py` è la versione vecchia, ogni render mostra la posa del frame precedente (stomp dair su f16 invece di f15). Rimedio: portare quel solo file da `claude/project-thread-0pafeo` (cherry-pick 3f3d01e), con ok owner.
- 2026-11-06 15:35 | Claude (cloud) | git | — | — | DANNO 2: tutto il lavoro di stanotte (dair v5/v6, dropkick startup/loop/land/v1, ftilt varA-C, anim-process.md, assemble_dropkick_montage.py) è NON committato sul ramo; e gli asset `/Game/AnimPipeline/` non sono mai in git. Se Antigravity ricrea o cancella il worktree si perde. Rimedio: un commit a fine sessione + push del ramo.
- 2026-11-06 15:35 | Claude (cloud) | dair v5/v6 | 15, 31 | — | DANNO 3 (solo se non voluto): v5 e v6 cambiano le pose chiave owner 15 e 31 rispetto a v4 (37 e 42 identiche) e non hanno più il blocco `approved` di v4 (frame 0,3,8,14,15,17,31,37,42). Se l'hai chiesto tu va bene; altrimenti v4 resta il riferimento delle tue chiavi.
- 2026-11-06 15:35 | Claude (cloud) | — | — | — | Nessun conflitto su numerazione frame e ruoli: la divisione Antigravity = animazione, Claude = GAS/NPP/integrazione va bene. Il ramo del worktree non contiene le varianti K/L/M della dair (stanno su 0pafeo / in _feature).
