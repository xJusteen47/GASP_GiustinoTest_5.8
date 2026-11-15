# Handover: skill revisore + esecutore e contratto comune (2026-11-15, cloud, solo doc + 1 script)

Branch `claude/project-thread-qksijd` (da `feature/gas-npp-5.8`, mai su feature, nessuna PR aperta). Punto 1 del piano
approvato (`Artifacts/Animazione/skill-e-riferimenti.html`, branch ybl6a8). Punti 2 (ue-import) e 3 (pilota jab 1) NON fatti.

## Fatto
- `.claude/skills/gstn-anim-reviewer/SKILL.md` (7,5 KB): revisione video regola 10, sei domande "alla pari di Smash/tripla A",
  una istruzione alla volta, lettura dell'esito, verdetto. Non tocca mai le pose.
- `.claude/skills/gstn-anim-reviewer/scripts/rv_frames.py`: estrae frame da un mp4, ritaglia una vista, compone UNA griglia
  etichettata `v<N>`, affianca la base (`--vs`), conta i pixel cambiati (`--diff`). Scritto da un subagente Sonnet, provato
  qui su `jab1_jab2_hit_ingame_slow.mp4` (griglie guardate: etichette leggibili, frame in ordine). Non provato su Windows.
- `.claude/skills/gstn-anim-executor/SKILL.md` (6,3 KB): applica una istruzione sulla scheda, gate, `esito.json`, prima/dopo.
  Vale per Antigravity (letto per percorso) e per un Claude economico; sezione per la sessione Claude che guida Antigravity.
- `Tools/AnimPipeline/contract/`: `README.md` (ruoli, ciclo, limiti, chiavi con intenzione, chi fa cosa, stato degli
  strumenti), `instruction.md` (otto campi + intestazione, due livelli, `esito.json`, sei rifiuti), `rubric.md` (rubrica
  unica: sei domande attacchi, cinque provvisorie per reazioni, riferimenti, voci griglia con gli stessi id),
  `glossary.md` (frame f/v/m/a, spazio, parti, controlli del rig, parole per `intento` con sezione di teoria).
- `animpipe-move`: `rubric.md` ora rimanda alla rubrica unica; `SKILL.md` ha 5 righe "Roles". Nient'altro toccato.
- Vecchia `gstn-move-animation`: testo archiviato in `Tools/AnimPipeline/contract/archive/` con avviso in testa.

## Scelte fatte (da segnalare)
- Skill in inglese come `animpipe-move`; nomi dei campi e risposte all'owner in italiano.
- Contratto in `Tools/AnimPipeline/contract/` e non in `.claude/`: Antigravity lo legge per percorso.
- Cartella di ciclo `docs/notes/reviews/<mossa>-c<N>/` con `inizio/`, `passo-NN/`, `fine/`, `ciclo.md`, `review.md`.
- Le skill dicono chiaro cosa NON esiste: chiavi per controllo e tangenti per chiave nella scheda, `ue-import`, copie
  `_Owner`/`_Agy`. Fino ad allora "curva" = profilo di spacing del tratto + ritardi per parte.

## Non verificato / limiti
- Nessun ciclo reale provato: lo prova il pilota (punto 3). Le prove pratiche D di Antigravity restano da fare.
- `verify`, sezione 4b di `animpipe-move`, `esito.md` sono su `claude/project-thread-anim-r2`, non ancora su feature.
- I `segnali` (chiave su ogni frame, chiave ridondante) li calcola l'esecutore a mano: nessun gate nel codice.
- Nomi dei controlli: verificati solo quelli di braccia e gambe; la lista vera è `controls` in `<mossa>_build_ue.json`.
- Taratura sul video in gioco non eseguita. Lista per le reazioni provvisoria.
- La skill di account `gstn-move-animation` va tolta dall'owner (o dal coordinatore con una proposta): da qui non si può.

## Prossimo
Proporre le due skill all'owner; poi thread separati per punto 3 (pilota jab 1, $5-9) e punto 2 (ue-import, $20-40, serve il sì).

Costo stimato del thread: ~$13-17 (Fable ~0,9M token in ingresso con cache, ~30k in uscita; subagente Sonnet ~62k token).
