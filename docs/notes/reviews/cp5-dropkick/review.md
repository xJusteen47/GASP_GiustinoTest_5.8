# Review: Overlord dropkick, aria → atterraggio — FAIL (solo lato gioco) — 2026-11-15

Cattura `dropkick_dropkick_whiff.mp4` (integ-cp3 3d9567e5). Frame video n = v − 36.
Mappa: salto da v22, attacco a0 = v43, estensione a15 = v58, tenuta in aria (loop a16-26) v69-v102, a terra v102/v103
(il montage salta ad a59), montage finito a v129, timeline finita a v133.

**Catena di cattura: valida.** Limiti: solo a vuoto (contatto e hitstop non giudicati); nessuna variante. Dopo
l'atterraggio la camera di gioco taglia il corpo sul bordo basso (G103-G124): l'atterraggio è giudicato da lato e dietro.

| domanda | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| impatto | sì | v58 lato, fronte 3/4, cam di gioco | corpo orizzontale, piedi avanti; di fronte le suole guidano; da dietro le gambe sono in scorcio ma escono dal profilo |
| tempo | sì | v44-v57 → v58 lato | raccolta lunga e viva (a2-a13), poi le gambe partono in un frame: v58 è il passo più grande (8279 px) |
| primo_frame | sì | v43 | 6686 px contro i circa 1300 della caduta: ginocchia su, braccia fuori |
| peso | sì | v44-v58 | il busto va indietro mentre le gambe partono, braccia in contrappeso |
| recupero | **no** | v102 → v103; v129 → v132 | il corpo scende di circa 60 cm in un frame; da sdraiato ad accovacciato in 3 frame, poi 37 frame per alzarsi (`gioco`, note) |
| pulizia | **no** | v45-v53, v69-v102 | frame doppi nello startup e per tutta la tenuta in aria (`gioco`) |

**Confronto.** Classe calcio in salto (Big Band j.HK come idea): raccolta, scatto in un frame e tenuta ci sono, la
parte in aria regge il confronto. Da dietro nessun riferimento Smash: giudicato per sagoma (braccia larghe in
diagonale, gambe verso l'alto a destra nella camera di gioco). Non è alla pari di un tripla A finché l'atterraggio
resta quello di oggi.

## Scarti (tutti `gioco`: nessun giro di posa consumato)
1. **gioco** — atterraggio, v102 → v103 lato (`rv_dk_land_side.jpg`): a v102 il corpo è ancora in posa di volo sopra
   il cerchio della capsula, a v103 è a terra. Manca il contatto: il peso della caduta non si vede. È il calo di
   bacino di 53 cm già noto (traccia capsula, scelta dell'owner in attesa).
2. **gioco** — nessuna rialzata, v129 → v132: sdraiato, seduto a mezz'aria, accovacciato, poi Crouch_to_Stand fino
   a v169. Noto (clip di get-up da fare, poi `DROPKICK_FOLLOW_UP`).
3. **gioco** — tenuta in aria a 30 Hz: nel loop "Hold Until Landed" la posizione va `16 18 18 20 20 22 22 24 24 26 26`
   per tutti i 34 frame di caduta, e `0 2 2 4 4 …` nello startup (5 frame doppi su 49 fuori dal loop). Stessa causa
   dello scarto 2 del jab.

Note di posa, senza giro: la posa sdraiata è del tutto ferma da v119 a v129 (0-5 px); il rimbalzo dopo il tocco
(v106-v110) è piccolo ma si vede, coerente col design "piccolo rimbalzo".

## Lato gioco, per il thread di codice
Scarti 1-3, più la camera di gioco che dopo l'atterraggio non inquadra il corpo (probabile stessa causa dello scarto 1).

## Decisioni dell'owner
Nessuna nuova: resta quella già posta sulla traccia capsula per i 53 cm.

Numeri: px = `python cp5-data/tools/rv_diff.py cp5-dropkick/dropkick_dropkick_whiff.mp4 36 side`; mappa e loop =
`python cp5-data/tools/rows_rle.py cp5-data/dropkick/dropkick_whiff.jsonl`; frame doppi = `cd cp5-data && python tools/count_repeats.py`.
