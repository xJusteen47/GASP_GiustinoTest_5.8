# Handover: blocco run start 29w3 e v4r5 (2026-11-23)

Brief: owner decisions 11-23 (29w3 approved "perfetta"; v4r5 option A, re-tag).
Macchina: cloud, solo git. Modello: Haiku medium. Budget: 1 $.

## Esito
- 29w3 bloccata: tag `approved/runstart-v29w3` -> `967de748` (ultimo commit che tocca
  `Content/GSTN/Animation/RunStart/AS_GSTN_RunStart_Generic_w3.uasset`, blob `9f8175c0`).
  Il blob e' identico alla punta di `claude/runstart-generic-desktop` (230ad65a).
  Conto: `git ls-tree 967de748 -- <file>` e `git ls-tree origin/claude/runstart-generic-desktop -- <file>`.
- v4r5 ri-puntata (tag gia' esistente, sovrascritto come da decisione A):
  - vecchio target: `af857cff` (Lock run start v4r5). NON contiene il file: `git ls-tree af857cff -- <file>` vuoto.
  - nuovo target: `9fe083bd` (Reviews s1 PASS, 2026-11-21). Contiene blob `58b7f421`.
  - primo commit che aggiunge il file: `git log --diff-filter=A -- <file>` -> solo 9fe083bd.
- Doc: riga 29w3 e nota v4r5 in `docs/notes/anim-approvate.md` su `claude/runstart-lock-2026-11-23`.

## Push
- Branch `claude/runstart-lock-2026-11-23` (52dfbc95): PUSHATO.
- Tag `approved/runstart-v29w3` e `approved/runstart-v4r5` (force): NON PUSHATI. Il proxy chiude la connessione (`send-pack: unexpected disconnect`), 2 tentativi per tag; `ls-remote` mostra ancora v4r5 su af857cff.
- Comandi se il push tag viene negato:
  `git push origin refs/tags/approved/runstart-v29w3`
  `git push --force origin refs/tags/approved/runstart-v4r5`  (sovrascrive: serve il permesso dell'owner)

## Scelte applicate (regola 6)
- Nome tag 29w3: `approved/runstart-v29w3` come da brief (le altre run start usano `runstart-w1/w2`).
- Tag annotati, messaggio con data owner.

## Costo
COSTO: ~0.05 $ | 0 min desktop | token review 0 (lavoro git e doc, nessuna review)
