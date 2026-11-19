# Prompt per Gemini (Antigravity / agy): peer review del design della telecamera da platform (2026-11-19)

Da incollare ad Antigravity sul desktop, DOPO la ricerca alla cieca (`agy-camera-research-prompt.md`). **Solo lettura e ricerca:
non modificare file del progetto, non aprire l'editor, non toccare `_ue59`.** Scrivi la risposta in
`docs/notes/camera-platforming/agy-review-esito.md` in un worktree tuo sul branch `claude/project-thread-mu9mh4` (commit + push),
in italiano, massimo ~200 righe.

## Cosa leggere
`git fetch origin claude/project-thread-mu9mh4` e leggi con `git show origin/claude/project-thread-mu9mh4:<path>`:
1. `docs/notes/camera-platforming/design.md` — il documento da recensire.
2. Se serve per verificare §2: `Plugins/GSTNCamera/README.md`, `Plugins/GSTNCamera/Source/GSTNCameraCore/Public/GSTN_CameraTypes.h`,
   `Source/GASP_Giustino2/Public/Camera/GSTN_CameraAnchor.h` (dal branch `origin/claude/ue59-sync-1119`).
3. La tua ricerca precedente `agy-research-esito.md`, se esiste: confrontala col documento.

## Domande (numera come qui; verdetto breve + esempio + fonte; distingui FONTE da OPINIONE)
1. **Errori di fatto** su Astro Bot, DK Bananza, Mario Odyssey/Galaxy, Nesky (§1). Cita la riga.
2. **Cose che la tua ricerca ha trovato e il documento no** (max 8, le più importanti prima).
3. **Architettura (§3.1)**: ha senso il plugin proprio con l'anchor come "soggetto" invece del rig Gameplay Cameras di Epic in UE 5.9? Rischi non visti (ordine dei tick con Network Prediction, control rotation, online)?
4. **Distanza regolabile (§3.3)**: preset + slider + limite d'area è un buon modello? Valori sensati?
5. **Punti di focus (§3.4)**: mancano parametri o casi d'uso? Cosa li renderebbe fastidiosi o nauseanti? Frame/Look/Reveal e Soft/Firm/Lock sono la divisione giusta?
6. **Modalità Platform (§3.2) e valori (§4)**: qualcosa contraddice come si comportano davvero Astro/Odyssey? (es. rotazione automatica, fascia verticale, pitch automatico in caduta)
7. **Ostacoli (§3.5)**: retino + silhouette + pull-in è la scelta giusta per hardware debole in Unreal 5.9?
8. **Piano e costi (§5)**: fasi nell'ordine giusto? Cosa toglieresti?
9. **Decisioni owner (§8)**: le opzioni sono chiare e la consigliata è ragionevole? Manca una decisione più importante?
10. **Le tre modifiche più utili** al documento, ognuna in una riga.

## Regole
- Se non sai, scrivi "non so". Niente numeri inventati. Niente riscrittura del documento: solo critica e proposte puntuali.
- In fondo: tempo impiegato e cosa non hai potuto verificare.
