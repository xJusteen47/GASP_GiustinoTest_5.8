# Handover: revisione del nuovo processo di animazione (2026-11-06)
Thread cloud "Revisionare il nuovo processo di animazione". Ramo `claude/project-thread-295vou` (da feature 60da6fa).

- Fatto: revisione `docs/notes/reviews/anim-process-review-2026-11-06.md` (1 pagina, rischi + divisione del lavoro + domande owner).
- Letto in SOLA LETTURA `GASP_Giustino2_feature` (owner: non modificare il processo; correzioni solo come copie `-proposta`, dette prima).
- Il processo nuovo (modello leggero su Antigravity) non lascia file in cartella: lavora nell'editor via `unreal-mcp`
  (editor aperto da ~01:47Z 06/11). Le sue istruzioni non sono leggibili da qui: confronto riga per riga in attesa.
- Variante M dair completa in locale dal 04/11 13:53 (carta, req, diagnosi, 3 mp4, 6 griglie) ma NON pushata e mai revisionata.
- Fix cattura 3f3d01e + PR #28 ancora solo su 0pafeo (35 commit sopra feature).
- La cartella è un worktree: git non funziona dal bridge (gitdir in percorso Windows), solo lettura file.

Prossimi passi (dopo risposte owner): confronto con le istruzioni Antigravity; eventuale `AGENTS-proposta.md`;
push di M (con ok); merge 0pafeo in feature (coordinatore).
Costo stimato: ~60k token, ~$1,5 (Opus).

## Aggiornamento 15:35Z: confronto con il processo di Antigravity
- Worktree di Antigravity: `C:\Users\jacop\.gemini\antigravity\worktrees\GASP_Giustino2_feature\animate_3d_moves_unreal`,
  ramo `experiment/3d-silhouette-anim` (= f6c5659, PR #28 senza capture fix). Il suo `docs/notes/anim-process.md` (12,7 KB,
  15:20Z) è IL processo ufficiale: priorità assoluta, non modificarlo senza ok owner + backup. Ruoli: Antigravity = authoring
  animazioni (Startup/Loop/Land + AM_ montage, animpipe author/build, check ossa 0,1 cm, sheet 360°); Claude = GAS/NPP/integrazione.
- Danni trovati (scritti nel Log di `_feature/docs/notes/anim-process.md`, NON nel worktree): 1) manca il fix cattura 3f3d01e
  (render un frame in ritardo); 2) tutto il lavoro dal 06/11 01:32 non committato (dair v5/v6, dropkick x4, ftilt varA-C);
  3) dair v5/v6 cambiano le chiavi owner 15 e 31 e tolgono il blocco `approved` di v4 (ok se chiesto dall'owner).
- Per Claude in futuro: le consegne di Antigravity portano asset, tabella frame e offset bacino Y/Z per sincronizzare Mover.
Costo totale thread stimato: ~120k token, ~$3 (Opus).
