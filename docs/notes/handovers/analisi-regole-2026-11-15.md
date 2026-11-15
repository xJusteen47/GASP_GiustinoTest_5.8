# Handover: analisi delle regole di lavoro (cloud, 2026-11-15)

Branch `claude/project-thread-na8pct` (base `claude/project-thread-tvze3n` dc82069, l'ultimo con `Artifacts/`).
Artifact: https://claude.ai/artifact/1LEUDPK4VjC1bRgKj8qYqP, copia in `Artifacts/Progetto/analisi-regole-di-lavoro.html`,
riga aggiunta in `Artifacts/README.md`. Nessun codice, nessuna regola o istruzione di progetto cambiata.
## Esito
8 proposte, ognuna con problema, testo della regola e costo/beneficio. L'owner approva per numero ("ok 1 2 3 5").
1. Approvazioni di macchina una volta per sessione (eccezione alla regola 9; un brief = una macchina).
2. Posti per macchina al posto di "2 codice + 1 anim" (1 editor per macchina + 2 cloud; `_ue59` un thread alla volta,
   compilata solo dall'ultimo thread della sessione). Cambia la regola in memoria "ogni thread finisce con _ue59 compilata".
3. Budget nel brief, stop al tetto, +30% dal coordinatore una volta, registro `handoffs/costi.md`.
4. Animazioni: cattura della clip BASE prima; voci di FAIL etichettate posa / dati montage / gioco; max 3 giri di posa.
5. Build, test e catture solo con script a tempo massimo (serve un thread Sonnet, circa 3-6 $ stimati).
6. Brief in `handoffs/`, handover nel repo, indice unico `handoffs/INDICE.md`, artifact su un branch `claude/artifacts`.
7. Decisioni per l'owner: un messaggio per sessione, max 3, risposta "1A 2B 3A".
8. Ogni numero di un handover cita log e comando; un controllo a campione prima di fidarsi.
Consigliate per prime: 1, 2, 3.
## Difetti trovati (16 nell'artifact), i meno ovvi
- Regola 4 contro regola 10: le review guardano 6-9 immagini e finiscono a 130-150k token. Proposto tetto di 8 immagini.
- Memoria con regole superate: push su feature "approvato" (10-23), tetto 400k token, nota "mai Fable".
- `MEMORY.md` 6,1 KB in un file che mescola regole e stato; `CLAUDE.md` dice ancora 5.8 e `takeover-state.md`.
- Video e griglie di review in git senza LFS: 165 file, 53 MB su questo branch.
- Non verificato: se le sessioni Remote Control ricevono le istruzioni di progetto o solo `CLAUDE.md`.
## Scelte fatte
Base del branch = tvze3n (feature non ha `Artifacts/`). Letti solo regole, indice memoria + 4 note, 3 handover, README
Artifacts, CLAUDE.md; conteggi dal remoto. Le cifre "stima mia" nell'artifact non sono misurate.
## Prossimo
Il coordinatore porta all'owner il link e raccoglie i numeri approvati. Le regole approvate le scrive l'owner (istruzioni
di progetto) o il coordinatore (memoria). Seguono, se approvati: script timeout (5), indice (6), pulizia memoria.
## Costo
Un thread cloud, Fable 5.2, circa 110k token di contesto alla fine, nessuna immagine. Stima 4-7 $.
