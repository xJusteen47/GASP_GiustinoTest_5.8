# Handover: bibliotecario della documentazione, giro del 2026-11-27

Routine settimanale, solo documentazione, cloud. Branch `claude/docs-bibliotecario-2026-11-27` da `main` ca2a8952
(il branch del 23-11 è già in main). Nessuna PR (come chiede la routine).

Fatto:
- `docs/INDICE-PROGETTO.md`: data della fotografia, §4 (build di main), §5.6 (spin Ganon in main), §5.14 (M12 in main),
  §5.16 (MCP e capdet in main, indice dei thread generato), §6 rifatta (36 branch della settimana non uniti), sezione Artefatti (23).
- `docs/STATO-ATTUALE.md`: blocco "Righe smentite dal repo" in cima (regole, tag, rete, due copie, tag di archivio). Nessuna decisione cambiata.
- `docs/notes/archivio-proposta.md`: rapporto con riga GRAVE (dair v9 pubblicata cambiata dopo il tag), 3 decisioni, sezioni A-F.
- `handoffs/INDICE.md`, `handoffs/costi.csv`: rigenerati con `Tools/CI/Build-Indice.py` (decisione 2A), non a mano.

Non fatto, di proposito:
- Copie nei project files (`handoffs/costi.csv`, `handoffs/INDICE.md`): la routine chiede righe a mano, la decisione 2A lo vieta (D9).
- Nessuno spostamento di handoff vecchi (7 candidati, sezione C), nessun artefatto toccato.
- Le sezioni 5.1-5.13, 5.15, 5.17 dell'indice non sono state rilette riga per riga.

Prossimo giro: rifare §A con lo stesso metodo; controllare se la decisione 1 (tag `approved/dair-v9-2b`) è stata presa.

BRANCH: claude/docs-bibliotecario-2026-11-27
ESITO: OK indice, stato e rapporto aggiornati; una riga GRAVE (dair v9 pubblicata cambiata dopo il tag) e 3 decisioni per l'owner
SUBAGENTI: 0
COSTO: 2.8 | 0 | 0
