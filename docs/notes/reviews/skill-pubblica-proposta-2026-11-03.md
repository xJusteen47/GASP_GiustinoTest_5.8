# Pipeline mosse come skill pubblica e indipendente dal modello (proposta, 2026-11-03)

Richiesta owner 03/11 03:16Z: per renderla pubblica (e usabile da qualunque modello) servono comandi più rigidi, con le
cose separate (es. check delle silhouette ≠ interpolazione), il meno macchinoso possibile, anche la finestra Python sopra
Cascadeur. Solo design, nessun codice. Le regole owner restano tutte (chiavi owner mai spostate, dita delle chiavi owner
vincono, `approve` solo dopo il suo ok, max 2 giri di revisione, una variante per giro, snapshot prima di ogni scrittura).

## Il problema oggi, in una riga per punto
- `animpipe` ha già un ingresso unico e scrive su file, ma: 20 sottocomandi che si sovrappongono (`review` = build+check+
  render; `capture` = backup del layer Manuale; `casc-*` con argomenti diversi), argomenti per percorso della scheda,
  uscita a testo libero, percorsi fissi (`~/Documents/Unreal Projects/GASP_Giustino2/...` in `__main__.py` e `_bridge.py`).
- L'ordine dei passi vive nei documenti e negli handover (README 230 righe + handover 456): un modello diverso deve
  leggerli e interpretarli. È qui che nascono gli errori di processo.
- ~150 script storici (`v8_*`, `jab_v*_build`, probe) stanno accanto ai comandi veri.

## (a) Superficie dei comandi: un verbo per compito, una forma sola
Forma: `animpipe <verbo> <mossa> [--opzioni]`, dove `<mossa>` è il nome (`overlord_dair`), non il percorso.

| Verbo | Fa una cosa sola | Scrive |
|---|---|---|
| `status` | stato della mossa e **il prossimo verbo ammesso** (`next`) | `status.json` |
| `import` | legge l'export Cascadeur: chiavi owner intere e bloccate, dita dell'owner, mai frame spostati | scheda |
| `revert` | rimette la scheda/chiavi all'ultimo snapshot (o `--to=<id>`) | scheda |
| `interp` | pose intermedie `origin: claude` **solo su frame liberi**, ognuna con `why` | scheda |
| `build` | scheda → rig → clip nel motore | clip |
| `check` | solo gate numerici (ossa, clearance, hitstop_snap, co_motion, fidelity, strike, rules, jitter) | report |
| `render` | **una** immagine composita + `manifest.json` (silhouette per parte, fantasma owner, traiettorie, hitstop) | `r<N>/` |
| `verdict` | valida `verdict.json` scritto dal revisore (oggi `visual-gate`) | esito |
| `variant` | `new` / `show` / `apply` / `discard`: una variante per giro, l'originale mai toccato | `<mossa>_var<A>` |
| `snapshot` / `restore` | zip di scheda + chiavi owner + layer Manuale; `restore` + elenco leggibile | `snapshots/` |
| `export` | scheda → Cascadeur, chiavi sparse + spacing della build | `.casc` |
| `owner-ok` | registra l'ok dell'owner su un giro (`--round=N --quote="..."` o dal pulsante Cascadeur) | `owner_ok.json` |
| `approve` | blocca le pose; **rifiuta** senza `owner_ok` sul giro corrente e con verdetto bloccante | lock |

Contratto uguale per tutti:
- stdout = **una riga JSON** `{ok, verb, move, exit, result, next}`; il dettaglio in `runs/<mossa>/<stamp>_<verbo>.json`;
- exit code fissi: 0 ok, 1 gate bloccato, 2 uso sbagliato, 3 serve l'owner (`chiedi_owner`), 4 serve un'app (Cascadeur/editor);
- deterministico: niente rete né modelli dentro i comandi, stesso input → stesso file; camera e risoluzione del `render` fisse;
- `snapshot` automatico prima di `import`, `interp`, `build`, `variant apply`, `restore` (nessuno lo deve ricordare);
- percorsi e app in `animpipe.toml` (cartella dati, rig, scheletro, adattatore DCC = cascadeur, motore = unreal, lingua owner).
- Silhouette e interpolazione separate davvero: `interp` scrive pose, `check`/`render`/`verdict` giudicano. Nessun verbo fa le due cose.

## (b) La skill: un sottile strato di istruzioni, il resto lo decide `status`
La skill non spiega l'ordine: chiede a `status` cosa fare. Così un modello qualunque segue lo stesso percorso.
Una skill sola, `animpipe-move`, che assorbe `gstn-visual-review` come file `review.md`. Schema di `SKILL.md`:

```
---
name: animpipe-move
description: Turn an animator's key poses into a finished, checked move (import, interpolate, check, render,
  review, variant, approve). Use when asked to import, interpolate, review or approve a move.
---
1. Setup: run `animpipe status <move>`. Read only: the move card, rules.md, the last result file. Never old logs.
2. Loop: run the `next` verb from status. Read its one-line JSON. exit 1 -> write a 3-line diagnosis, fix ONE thing,
   run `next` again (max 2 attempts per gate). exit 3 -> stop and ask the owner. exit 4 -> ask for the app.
3. Visual review (only if you can read images; otherwise stop with exit 3 "needs vision"): follow review.md,
   look at render's image once, write verdict.json, run `verdict`. Max 2 rounds.
4. Owner contact: send the image + a 5-line summary; record the owner's words with `owner-ok`; only then `approve`.
5. Hard rules (the commands also refuse them): never move or edit an owner key or a locked part (propose instead);
   fingers come from the owner's keys; one variant per round, owner says apply/discard; never delete snapshots;
   no third review round; no edits to app settings.
Files: rules.md (owner rules, per project), review.md + rubric.md + verdict.schema.json, kits/<kit>.md (style rules).
```

Regole di scrittura: niente nomi di modelli né di strumenti di un'azienda; "esegui il comando", "guarda il file immagine";
le regole di stile del progetto (mani villain, "troppo è giusto", Smash) vanno in `rules.md`/`kits/`, non nella skill.

## (c) Cascadeur e la piccola app: due finestre, ognuna col suo compito
Limiti `csc`: niente C++, barra strumenti nativa non modificabile, server script da avviare a mano. Oggi: menu Scripts →
GSTN + barra flottante QML con 4 pulsanti. Opzioni valutate: tutto nella barra Cascadeur (contestuale), oppure una piccola
app Python a parte (idea owner 03:17Z). **Consiglio: entrambe, divise per compito.**
- **Dentro Cascadeur restano solo le azioni che hanno bisogno della scena aperta**: `Segna chiave` (frame corrente) e
  `Invia` (salva + esporta + richiesta import), più una riga di stato (mossa, chiavi owner, richiesta in attesa). Via
  "Stato" e "Annulla import". I pulsanti non lanciano `animpipe`: scrivono `requests/<stamp>_<mossa>_<azione>.json`,
  così Cascadeur non si blocca. La barra flottante resta (un pannello agganciato al QML si perde a ogni cambio di layout).
- **App "GSTN Studio"** (PySide6, una finestra, `python -m animpipe.app`), per tutto il resto, senza Cascadeur aperto:
  elenco mosse con `status`; **griglia composita + verdetto** del giro corrente affiancati; pulsanti `Ok` (scrive
  `owner-ok`: l'ok viene dall'owner, non da un agente) e `Approva`; variante: griglia originale/variante con i frame
  cambiati e i motivi, `Applica` / `Scarta`; **snapshot** con data e motivo, `Ripristina`; `Annulla ultimo import`.
  Chiama solo i verbi di (a) e mostra la loro riga JSON: nessuna logica propria, quindi nessun secondo percorso.
- Perché non solo l'app: segnare una chiave richiede il frame e la scena di Cascadeur. Perché non solo Cascadeur: le
  immagini, le varianti e gli snapshot in una barra QML dentro Cascadeur sono scomodi e fragili, e l'owner deve poter
  dire ok o scarta anche senza aprirlo. Lavori interpretativi (`interp`, revisione visiva) restano all'agente.
- Etichette dalla lingua in `animpipe.toml`. Per un utente pubblico l'app è anche la "faccia" della skill: chi non usa
  agenti vede comunque lo stato e i check.

## (d) Cosa c'è già e cosa cambia (costi a spanne, modello consigliato)
1. Già fatto: ingresso unico, output su file ≤5 righe, `casc-import/revert/diff`, snapshot della scheda (solo json),
   `fidelity`, `visual-gate`, `approve` con lock, skill di revisione, menu + barra GSTN. **$0**
2. `animpipe.toml` + via i percorsi fissi (`__main__.py`, `_bridge.py`, script Cascadeur/UE). Sonnet, **~$1-2**
3. Verbi nuovi come involucro sui moduli esistenti, mossa per nome, riga JSON + exit code fissi; `capture` assorbito da
   `snapshot`, `review` diviso in `build`/`check`/`render`, `visual-gate` → `verdict`. Sonnet, **~$2-3**
4. `status` con `next` (la macchina a stati: import → check → interp → build → check → render → verdict → owner-ok →
   approve → export). Opus medium, **~$3**
5. `snapshot`/`restore` zip con chiavi owner + layer Manuale, automatici (decisione 6). Sonnet, **~$1-2**
6. `owner-ok` + gate in `approve`. Sonnet, **~$1**
7. `variant` (decisione 5). Sonnet, **~$2**
8. `interp`, `render`, `import` con chiavi owner bloccate e dita dell'owner: già in programma (CP2/CP4), qui solo con i
   nomi nuovi. **$0 in più**
9. Skill `animpipe-move` (fusione con `gstn-visual-review`, regole in `rules.md`/`kits/`). Cloud, **~$1**
10. Barra Cascadeur ridotta (Segna chiave, Invia, riga di stato). Sonnet sul desktop, **~$1**
11. App "GSTN Studio" (griglia + verdetto, ok/approva, varianti, snapshot). Sonnet, **~$3-4**
12. Script storici in `legacy/` fuori dalla superficie pubblica. Haiku, **~$0,5**
13. Solo quando si pubblica: estrarre `animpipe` + skill in un repo a parte con un esempio (mossa jab). **~$2**

Totale in più rispetto a CP2/CP4: **circa $15-20**. Ordine consigliato: 2-6 in un thread Sonnet senza editor (test
sintetici come per PR #24), **prima** del passo desktop CP2/CP4, così quel passo nasce già sui verbi nuovi invece di
costruire due volte; poi 7, 9, 10, 11 insieme al passo desktop; 12-13 solo quando l'owner decide di pubblicare.
