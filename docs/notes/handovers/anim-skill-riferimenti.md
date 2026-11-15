# Handover: skill, ciclo revisore/esecutore, Unreal su copia, riferimenti (2026-11-15, cloud, solo doc) — FINALE

Branch `claude/project-thread-ybl6a8` (da `claude/project-thread-0tmywc-docs`, mai su feature). Nessun codice toccato.
Documento (v7): `Artifacts/Animazione/skill-e-riferimenti.html` = https://claude.ai/artifact/TXAt4tWjuWW5CfnjXH6o63
Review con Antigravity: prompt `docs/notes/reviews/agy-check/prompt-per-agy.md` (qui); risposte `esito.md` su `claude/project-thread-anim-r2`
fa79d89b (22 risposte, sezione D prove pratiche NON fatta).

## Piano
- REVISORE (Claude: principi + soglia "alla pari di Smash/tripla A" + riferimenti) scrive UNA istruzione alla volta; ESECUTORE
  (Antigravity via agy-bridge dal desktop, riserva Claude economico) la applica sulla scheda, gate numerici, griglia prima/dopo + scie.
- Istruzione a 8 campi: chiave, tipo, parte, controllo, intento, modifica, principio, verifica. Due livelli: principio (normale),
  misurato (obbligatorio al 2° tentativo). Ingresso: almeno 5 pose, mai da zero. Video in gioco solo inizio e fine.
- Chiavi sparse: max 1 ogni 4-5 frame, max 3 aggiunte e 6 istruzioni per ciclo, 2 tentativi; gate che conta le chiavi.
- Difetto lato gioco nel video iniziale → ciclo sospeso, thread di codice. `animpipe revert` se due passi peggiorano.
- Unreal (owner: niente livello additivo): copia `LS_<mossa>_Owner` → modifica → `ue-import` con diff → integra | uniforma | cambia.
  Scheda = unica verità.

## Verificato nel repo (anim-r2)
- VERO: jab1 v3 ha 24 chiavi (f0-22 tutti; r0 13), jab2 v3 28 (r0 17): schede oggi DENSE. Esistono author, build, check, fidelity,
  verify, poses --over, trails, revert, capture, casc-import/diff, `ue_dump_user_keys.py`, `clip_grid.py`, `_read_manual` (tangenti).
- FALSO: `animpipe ingest-owner` e `ue-import` non esistono. La scheda tiene profili di spacing, non tangenti per chiave.
- Non verificato: che agy scriva solo nella scheda; lavoro da file; rifiuto sulle chiavi owner (servono le prove D).

## Costi stimati
Skill A (2 + contratto) $14-20 · `ue-import` $20-35 (codice, serve il sì) · pilota jab 1 con prove D $5-9 · libreria riferimenti $10-15.

## Decisioni aperte per l'owner
Skill A; ue-import; pilota; esecutore agy; tetti; quale vista vince (dietro vs lato); libreria + clip da dietro.
Licenze delle fonti non verificate sul web (non è un parere legale).

Costo stimato del thread: ~$22-26 (Fable, ~900k token in ingresso con cache, ~45k in uscita).
