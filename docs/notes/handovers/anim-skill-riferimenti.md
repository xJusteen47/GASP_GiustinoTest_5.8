# Handover: skill, ciclo revisore/esecutore e libreria di riferimenti per le animazioni (2026-11-15, cloud, solo doc)

Branch `claude/project-thread-ybl6a8` (da `claude/project-thread-0tmywc-docs`, mai su feature). Nessun codice toccato.
Documento (v4): `Artifacts/Animazione/skill-e-riferimenti.html` = https://claude.ai/artifact/TXAt4tWjuWW5CfnjXH6o63

## Modello approvato a grandi linee dall'owner (11-15 00:58Z: "ok penso vada bene")
REVISORE (Claude) giudica con principi + soglia "alla pari di Smash/tripla A" + riferimenti e scrive UNA istruzione alla volta.
ESECUTORE (Antigravity o modello economico) la applica su chiavi SPARSE, fa girare i gate numerici, mostra prima/dopo.
Pose di partenza mai da zero: pose dell'owner, clip libera, o pose chiave misurate da un riferimento (`ref_keyposes.py`).
Istruzione a 7 campi (chiave, tipo, parte, intento, modifica, principio, verifica), due livelli: di principio (normale) o misurato.
PROSSIMO PASSO (owner 01:00Z): interrogare Antigravity sul desktop con `docs/notes/reviews/agy-check/prompt-per-agy.md`
(22 domande + 5 prove, anche critica del piano) → `esito.md` nella stessa cartella → versione finale del documento.

## Raccomandazioni e costi stimati
1. Opzione A: skill revisore + skill esecutore + contratto comune (rubrica unica, glossario, formato). ~$14-20. (B solo revisore
   $8-12; C una skill a due modi $10-14.) `gstn-move-animation` in archivio.
2. Unreal (owner 01:13Z: il livello additivo "Manuale" NON va bene): copia `LS_<mossa>_Owner` → modifica chiavi e curve → `ue-import`
   con diff → "integra | uniforma | cambia". Scheda unica verità; anche Antigravity su copia `_Agy`. Codice, ~$20-35, serve il sì.
3. Libreria riferimenti solo numeri in `Tools/AnimPipeline/reflib/`; immagini/GIF/video solo sul desktop. Pilota 10 mosse ~$10-15.
4. Mossa pilota col ciclo completo ~$5-9 (realistico $10-18 se servono due cicli).

## Fatti verificati (branch anim4x)
- Owner: Antigravity fa bene modifiche di principio ("più follow-through qui") con principi + pipeline; fallisce con sole 3 pose.
- Scheda animpipe = chiavi solo sui frame chiave; skill Antigravity (`extracted_animator/smash-animator.md`) vieta il baking denso.
  Percorso denso vecchio ancora presente (`DENSE=1`, "direct to Unreal").
- Silhouette: `legibility.py`, `tangency.py`, rubrica `animpipe-move`, audit Antigravity; doc teoria silhouette solo su ujyw42.
- Nessuna skill cita `docs/notes/anim-theory/`. Riferimenti: `ufd_fetch.py`, `ufd_measure.py`, `ref_*.py`, `motion_ref.py` esistono.

## Buchi principali
Due archivi di chiavi (scheda vs Sequencer scritto da Antigravity); Claude non può avviare Antigravity; stima dei cm da immagine;
6 istruzioni possono non bastare; taratura del revisore; editor desktop conteso; lista domande solo per attacchi (manca reazioni).

## Decisioni aperte per l'owner
Chi esegue; fonte di verità delle chiavi / `ue-import`; opzione A/B/C; tetti (2 tentativi, 6 istruzioni, 8 chiavi aggiunte);
clip da dietro. Non verificato: se gli strumenti di Antigravity rileggono già le chiavi dal Sequencer; licenze delle fonti.

Costo stimato del thread: ~$16-19 (Fable, ~600k token in ingresso con cache, ~30k in uscita).
