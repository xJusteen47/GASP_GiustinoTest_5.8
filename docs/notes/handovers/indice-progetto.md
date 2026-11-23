# Handover: indice del progetto e audit del codice (2026-11-23)

Branch `claude/indice-progetto` da `main` a5b20950. Cloud, Opus medium, sola lettura sul codice (nessun refactor).
OBIETTIVO (owner): i thread rifanno cose che esistono già; serve un indice per sistema e un audit di doppioni,
documentazione mancante e codice da snellire, comprensibile da una persona nuova.

## Fatto
- `docs/INDICE-PROGETTO.md` (~210 righe): glossario, schema "chi chiama chi", legenda di stato, 17 sistemi con percorsi,
  parametri, test e doc, sezione "Non ancora in main", riga di regola per i brief.
- `docs/DOVE-METTO-MANO.md`: 15 casi "voglio cambiare X" + cosa non toccare.
- `docs/notes/audit-codice.md`: tre liste per impatto (doppio, poco documentato, da snellire), 3 decisioni per l'owner,
  ordine dei lavori T0-T5, cosa serve l'editor.
- Riusati: `docs/notes/mappa-cartelle.md` (branch project-thread-f4ovn9) e `milestones-stato-2026-11-21.md` (yepa62).

## Prove (regola 11)
- Ultima build+test completa: 1614/0, `docs/notes/handovers/anim-round-2-postmerge.md` riga 6 (ricontrollata).
- main non compilato dopo il merge 22-11: `docs/notes/handovers/integ-playtest-2026-11-22.md` "Prossimi passi 2".
- Branch non uniti: `git for-each-ref --no-merged origin/main refs/remotes/origin` (126).
- AimOriginSocket mai letto: `grep -rn AimOriginSocket Source Plugins/GSTN* Config Tools` (1 riga).
- Numeri dell'audit (948/5585, 30 campi, 46 percorsi morti) da script di scansione eseguiti nel thread, non nel repo.

## Da fare
- Owner: decisioni 1-3 in cima ad audit-codice.md (consigliate 1A 2A 3A).
- Coordinatore: aggiungere ai brief la riga "prima di scrivere codice consulta docs/INDICE-PROGETTO.md e dichiara cosa
  riusi"; mettere T0-T3 in coda; chi sposta codice aggiorna l'indice nello stesso commit.
- Non verificato: tutto ciò che richiede l'editor (Blueprint che usano GA_Jump, campi mai letti, Shipping).

SUBAGENTI: 4, scansione meccanica (2 inventari plugin, strumenti/test/branch, audit codice), tutti Sonnet.
COSTO: ~4,5 $ | 0 minuti desktop | 0 token review
