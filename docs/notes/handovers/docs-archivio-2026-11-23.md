# Handover: archivio doc, artefatti e regole (2026-11-23)

Thread cloud "Update artifacts, archive old handoffs and fix CLAUDE.md". Brief: nota del coordinatore, decisioni owner 1A 2A 3A
sul rapporto del bibliotecario (`docs/notes/archivio-proposta.md`). Branch `claude/docs-archivio-2026-11-23` (da `claude/docs-bibliotecario-2026-11-23`). Riuso: indice e STATO-ATTUALE del bibliotecario, nessun codice.

Fatto
1. Regole (3A): testo pieno delle regole 1-16 copiato parola per parola dalle istruzioni di progetto in `docs/notes/regole-progetto-2026-11-23.md` (copia: project files `decisioni/istruzioni-progetto-2026-11-23.md`). `CLAUDE.md` ci punta, dice coordinatore = Claude (Antigravity no) e base `main` invece di `feature/gas-npp-5.8`. Riga SUPERATO in cima a `regole-progetto-2026-11-20.md`, `-22.md`, `memoria-progetto.md` (Prova B), project files `decisioni/istruzioni-progetto-2026-11-15.md` e `-20.md`. `AGENTS.md` e `INDICE-PROGETTO.md` puntano al file nuovo. Controllo: `grep -n Antigravity CLAUDE.md` = solo la riga che dice che non coordina.
2. Handoff (2A): i 103 candidati della sezione C spostati con `mv` in project files `handoffs/archivio/2026-11/` (i handoff non sono in git, quindi niente `git mv`); `handoffs/` passa da 152 a 51 voci (`ls | wc -l`, compresa la cartella archivio). 33 riferimenti `handoffs/<nome>` nel repo e 11 nei file vivi di `handoffs/` riscritti al percorso nuovo. `handoffs/archivio/LEGGIMI.md` (criterio, ripristino) e una riga in `handoffs/INDICE.md`.
3. STATO-ATTUALE: decisioni del 23-11 registrate (ftilt v3.1 = F.L, N.S a parte; pummel tolto; schema lanci; 29w3 approvata con Gemini, verdetti 23-26 obsoleti, turnaround allineati; niente server dedicato con host + EOS + predizione avversario + limite di ping + ritardo input all'host spento di default; regole 7 e 16). §7 ridotta a 3 domande aperte.
4. Artefatti (1A, subagente Sonnet): aggiornati Mosse di esempio GSTN (con la Combat Move passo passo fusa dalla Guida mosse Blueprint), Architettura plugin combattimento, Networking (5.9 + decisione 1v1). Banner ARCHIVIO su Guida mosse Blueprint, Cose da fare a mano, Analisi regole, e sui 4 documenti di fine ottobre (strumenti AI, Cascadeur, pipeline agentica, depth marker). Stessi URL, nessuno cancellato; copie HTML su `claude/artifacts` 1f30f96e.

Limiti
- Il subagente ha riscritto 5 pagine dal testo letto invece di copiare il file salvato: possibili piccole differenze di spazi, nessuna pagina aperta a video. Le versioni precedenti restano nella storia di ogni artefatto.
- Nessuna PR verso main aperta (i branch base non sono ancora in main).

A mano / per il coordinatore
- Unire in main `claude/indice-progetto` → `claude/docs-bibliotecario-2026-11-23` → questo branch (PR, decide l'owner).
- Memoria: le voci "DESIGNER GUIDE" e "MANUAL-TODO" vanno corrette (la seconda ora è ARCHIVIO).
- La 3A della mappa input ("server dedicato per il ranked") va segnata superata nel file sul branch `claude/project-thread-uvefj5`.

SUBAGENTI: 1, aggiornamento e archivio degli 11 artefatti, Sonnet (circa 346k token).
COSTO: 4,0 $ | 0 | 0
