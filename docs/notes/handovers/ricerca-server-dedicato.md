# Handover: ricerca rete 1v1 (server dedicato → P2P/host), 2026-11-23

Thread cloud, Opus, solo ricerca, nessun codice, `_ue59` non toccata. Branch `claude/ricerca-server-dedicato` da main a5b20950.

OBIETTIVO: ricerca su server dedicato per il 1v1 classificato e ritardo con cui si vede l'avversario. A metà lavoro l'owner
ha cambiato ambito: niente server dedicato all'inizio, i picchiaduro sono P2P con rollback; domanda "abbiamo già un rollback
misto?". Il documento copre il nuovo ambito; il server dedicato è una sezione breve.

ESITO: `docs/notes/server-dedicato-ricerca.md` (riassunto 10 righe, §1 prove con file e righe, §6 tre decisioni).
- Sì, rollback misto client-server già presente (combattimento+ASC+Mover rigiocati, stato di combattimento e input nel sync
  state: `GSTN_CombatSimTypes.h` r.102-208, `GSTN_InputSimTypes.h` r.155-221). L'avversario non è predetto (interpolato).
- Listen server già supportato e testato (testing.md r.25). Rollback P2P deterministico vero = simulazione nuova, sconsigliato.
- Consigliata A: listen + EOS P2P + predizione in avanti del solo avversario (spike S-FP) + limite ping; 25-45 $ stimati.
- Artefatto networking 8-11: coerente, superata la nota "buffer 100 ms sul ramo feature"; README fork NP dice ancora 100 ms.

DECISIONI APPLICATE: nome file mantenuto come da brief anche se l'ambito è cambiato.
DA CONFERMARE (owner): decisioni 1-3 di §6.
NON VERIFICATO: §9 del documento (nessuna misura in listen, nessuna prova EOS, backend Mover dedotto).

SUBAGENTI: 1, lettura di 4 pagine web (Epic dedicated server, infil netcode, EOS multiplayer, Redpoint P2P), modello di sessione (web-fetch).
COSTO: ~2,5 $ | 0 minuti desktop | 0 token review
