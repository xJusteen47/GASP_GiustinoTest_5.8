# Handover: piano di pulizia repo e harness (2026-11-24)

- Thread cloud, Opus high, sola lettura sul repo. Nessun branch, asset o file toccato oltre a questi documenti.
- Piano: `docs/notes/pulizia-2026-11-24/piano.md` (copia in project files `handoffs/pulizia-repo-harness-piano-2026-11-24.md`).
- Elenco branch con categoria e azione: `docs/notes/pulizia-2026-11-24/branch-elenco.csv` (183 remoti: A 37 gia' in anim-skill-t3,
  B 22 sottoinsiemi, C 81 storia vecchia, D 26 solo docs, E 11 con codice, 5 tenuti + main + feature).
- Prova: `git fetch --all`, poi per ogni `origin/*` `git merge-base --is-ancestor <b> origin/claude/anim-skill-t3`,
  `git branch -r --contains`, `git diff --name-only $(git merge-base t3 b) b`; script nel thread, risultato nel CSV.
- Fatti chiave: `feature/gas-npp-5.8` non e' in main (1063 commit suoi) ma CLAUDE.md e Cleanup-Branches.ps1 lo usano ancora;
  main ha 9 radici orfane (snapshot 17-21/11); registri INDICE/costi doppi e divergenti (repo 10 righe, project files 93);
  AGENTS.md rimanda a regole 1-15 del 22/11; jab v12 e ftilt v3.1 approvati non sono in git; guard rifiuta i tag approved/*.
- Proposta: tronco main, un'integrazione al giorno (oggi anim-skill-t3, non toccarla prima del PASS di T3), ordine di merge
  dei branch E (piano §2), regole e skill in un posto solo, indice/costi generati dagli handover, registro-clip.csv scritto solo
  da `animpipe publish/approve`, asset approvati in LFS con lock, AGENTS.md che rimanda senza copiare.
- Decisioni owner: 1A 2A 3A (owner 2026-11-24 01:48 UTC): tronco main, indice/costi generati, Published/ in LFS con lock.
- Prossimo: fasi P1-P6 del piano, stima ~13 $ + 80 min fisso + 15 min owner, dopo la review cieca dair v9 e il PASS di T3 (via del coordinatore).

ESITO: piano consegnato, nessuna modifica eseguita
SUBAGENTI: 0
COSTO: 3 $ | 0 min desktop | 0 token review
