# Bibliotecario della documentazione, primo giro (2026-11-23)
Brief: nota del coordinatore (cloud, Sonnet medium, 3 $) + 2 aggiunte dell'owner (artefatti; STATO-ATTUALE e briefing per Antigravity).
Branch: `claude/docs-bibliotecario-2026-11-23`, partito da `claude/indice-progetto` (4406d561). Riuso: INDICE-PROGETTO, audit-codice.
Fatto (sola lettura, niente cancellato o spostato, nessun artefatto toccato):
- `docs/notes/archivio-proposta.md`: rapporto 10 righe, 3 decisioni, A orfani (130/167, 11 senza alcuna citazione; script su
  basename, vedi metodo §A), B voci senza file (0 rotte in main, 4 solo su branch, `git cat-file -e`), C 103 handoff
  pre-19-11 da archiviare, D 9 contraddizioni con file:riga, E 11 artefatti con proposta, F handoffs/INDICE.md.
- `docs/INDICE-PROGETTO.md`: sezione Artefatti in cima, §6 con mappa input, ricerca rete, presa d4714a4c, riordino Content,
  link a STATO-ATTUALE. `main` invariato da a5b20950 (`git log origin/claude/indice-progetto..origin/main` vuoto).
- `docs/STATO-ATTUALE.md` (fonte unica: regole 1-16, kit, notazione, approvate/tag, sospeso, in corso, ultimi 7 giorni,
  7 "da confermare") e `docs/BRIEFING-ANTIGRAVITY.md` (una pagina per agenti esterni).
- `handoffs/INDICE.md` (project files) riempito: 70 righe da costi.csv, stato branch da `git ls-remote`; copia in
  `docs/notes/bibliotecario/handoffs-INDICE-2026-11-23.md`.
Attività programmata "Bibliotecario documentazione settimanale": venerdì 19:48 ora di Roma (CRON_TZ=Europe/Rome 48 19 * * 5,
spostata di 12 minuti dalle 20:00 per evitare l'ora piena), prima esecuzione 27-11; ogni giro apre un thread nuovo nel
progetto, notifiche push/email spente nel task (le notifiche dei thread seguono le impostazioni del progetto). Contraddizione
grave = prima riga del rapporto "GRAVE:". Il modello non si sceglie dal task: usa quello di default del progetto (da confermare
che sia Sonnet).
Disattivarla: chiedere al coordinatore "spegni il bibliotecario settimanale" (strumento update/delete trigger, id
trig_01X3GMju1yjo16kn9AJJpg2M), oppure da claude.ai nell'elenco delle attività programmate.
Non fatto: i 4 artefatti di fine ottobre letti solo da titolo e metadati; nessun spostamento (aspetta decisioni 1-2).
Da confermare (owner): decisioni 1-3 del rapporto; lista §7 di STATO-ATTUALE.
SUBAGENTI: 1, lettura e confronto dei 10 artefatti, Sonnet.
COSTO: ~3,5 $ | 0 | ~0,2M token subagente
