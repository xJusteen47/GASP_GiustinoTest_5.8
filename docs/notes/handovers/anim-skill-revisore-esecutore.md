# Handover: skill revisore + esecutore e contratto comune (2026-11-15, cloud, doc + 1 script)
Branch `claude/project-thread-qksijd` (da feature, mai su feature, nessuna PR). Punto 1 del piano approvato
(`Artifacts/Animazione/skill-e-riferimenti.html`, branch ybl6a8). Punti 2 (ue-import) e 3 (pilota jab 1) NON fatti.

## Fatto
- `.claude/skills/gstn-anim-reviewer/SKILL.md`: revisione video regola 10, sei domande "alla pari di Smash/tripla A", una
  istruzione alla volta, lettura dell'esito, verdetto. Non tocca mai le pose.
- `.../gstn-anim-reviewer/scripts/rv_frames.py`: da un mp4 compone UNA griglia etichettata `v<N>`, ritaglio di una vista,
  base sotto (`--vs`), pixel cambiati (`--diff`). Subagente Sonnet; provato qui su un mp4 del progetto, non su Windows.
- `.claude/skills/gstn-anim-executor/SKILL.md`: applica una istruzione sulla scheda, gate, `esito.json`, prima/dopo. Vale
  per Antigravity (letto per percorso) e per un Claude economico; sezione per la sessione Claude che guida Antigravity.
- `Tools/AnimPipeline/contract/`: `README.md` (ruoli, ciclo, limiti, chiavi con intenzione, stato degli strumenti),
  `instruction.md` (otto campi + intestazione, due livelli, `esito.json`, sei rifiuti), `rubric.md` (rubrica unica: sei
  domande attacchi, cinque provvisorie reazioni, riferimenti, voci griglia con gli stessi id), `glossary.md` (frame
  f/v/m/a, spazio, parti, controlli del rig, parole per `intento` con la sezione di teoria).
- `animpipe-move`: `rubric.md` rimanda alla rubrica unica; `SKILL.md` +5 righe "Roles". Vecchia `gstn-move-animation`
  archiviata in `Tools/AnimPipeline/contract/archive/` con avviso in testa.

## Scelte fatte
- Skill in inglese come `animpipe-move`; campi dell'istruzione e risposte all'owner in italiano.
- Contratto in `Tools/AnimPipeline/contract/`, non in `.claude/`: Antigravity lo legge per percorso.
- Cartella di ciclo `docs/notes/reviews/<mossa>-c<N>/`: `inizio/`, `passo-NN/`, `fine/`, `ciclo.md`, `review.md`.
- Le skill dicono cosa NON esiste ancora: chiavi per controllo e tangenti per chiave nella scheda, `ue-import`, copie
  `_Owner`/`_Agy`. Fino ad allora "curva" = profilo di spacing del tratto + ritardi per parte.

## Non verificato
- Nessun ciclo reale: lo prova il pilota. Prove pratiche D di Antigravity da fare. Taratura sul video non eseguita.
- `verify` e sezione 4b di `animpipe-move` sono su `claude/project-thread-anim-r2`, non ancora su feature.
- `segnali` calcolati a mano dall'esecutore (nessun gate nel codice). Controlli del rig: verificati solo braccia e gambe.
- La skill di account `gstn-move-animation` la toglie l'owner: da un thread non si può.

Prossimo: proporre le due skill; poi pilota jab 1 ($5-9) e ue-import ($20-40, serve il sì), in thread separati.
Costo stimato del thread: ~$13-17 (Fable ~0,9M token in ingresso con cache, ~30k in uscita; subagente Sonnet ~62k).
