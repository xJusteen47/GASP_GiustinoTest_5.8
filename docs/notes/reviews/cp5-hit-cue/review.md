# Review: colpo con cue CP4 e hitstop shake (KIT_Example Attack su dummy) — FAIL — 2026-11-15

Cattura `cue_cue_hit.mp4` (integ-cp3 3d9567e5), dummy 95 cm avanti. Frame video n = v − 14.
Mappa: tap v20, montage dell'attacco a0 = v21 (clip GASP `M_relaxed_ragdoll_shove_stand_F_A`, 1.55 frame di clip per
frame video), colpo e hitstop v26-v27 (2 frame sull'attaccante), la vittima tiene la posa v27-v30.

**Catena di cattura: valida.** Hitstop visibile, etichette leggibili. Limiti noti: kit di esempio con clip segnaposto,
nessun asset VFX (slot Niagara vuoti): la scintilla non può esserci.

## Vittima (domande per una reazione)
| domanda | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| primo_frame | **no** | v26 lato e fronte 3/4 | per un frame il corpo è girato di circa 90° verso la camera di lato e spostato; da v27 c'è la vera posa di colpo (`gioco`) |
| direzione | sì | v27-v30 | busto indietro, lontano dall'attaccante, ginocchia piegate, braccio avanti: si legge |
| continuita | **no** | v26 → v27 | lo scatto di un frame sopra; da v31 il barcollo prosegue liscio |
| peso | sì | v31-v40 | il piede dietro arretra, il bacino assorbe |
| pulizia | sì | v27-v40 | nessuna compenetrazione |

## Momento del colpo
| voce | sì/no | frame e vista | cosa si vede |
|---|---|---|---|
| attacco e colpo insieme | **no** | v21-v34 lato | l'attaccante è fermo in idle quando la vittima reagisce (v26); alza le mani a v36 e spinge a v44-v50 (`dati montage`) |
| primo frame dell'attacco | **no** | v21-v25 | 148-189 px per frame, come l'idle prima del tap (136-164): nessun cambio di posa |
| fermo di hitstop | sì | v26-v27 | attaccante fermo 2 frame; vittima ferma v27-v30 |
| shake della vittima | **no** | v27-v30 lato | il baricentro oscilla di circa 1 px (82.4 / 83.6 / 82.7 / 82.9), circa 1 cm: a distanza di gioco non si vede |
| cue visivo | n.d. | | nessun asset VFX: al contatto l'unica informazione è il fermo |

**Confronto.** In Smash attaccante e vittima si fermano insieme sul frame di contatto, la vittima trema di alcuni
centimetri e c'è una scintilla. Qui il fermo c'è, il resto no: non è alla pari.

## Scarti
1. **dati montage** — KIT_Example Attack: la finestra di colpo è al frame 5-6 della timeline, il contatto della clip
   arriva circa 20 frame dopo. Per un esempio pubblico la vittima che reagisce prima della spinta è il difetto più
   visibile. Correzione consigliata: far partire il montage più avanti nella clip, così la spinta cade sul frame di
   colpo e il primo frame cambia posa; in alternativa spostare la finestra di colpo a circa 26-30.
2. **gioco** — posa sbagliata della vittima per un frame sul frame di colpo (v26; sagoma 3489 px contro circa 2650
   dei frame dopo). Si vede anche nel colpo del down-air (v91, pose diverse nelle due esecuzioni). Da controllare cosa
   disegna la vittima nel frame in cui il colpo viene applicato (primo frame della reazione con rotazione o offset
   non ancora corretti, prima del fermo).
3. **gioco, taratura** — shake troppo piccolo: almeno 6 cm picco-picco, che cala durante l'hitstop, come impostazione
   del cue. Con gli slot VFX vuoti serve anche una scintilla di default, altrimenti il contatto non comunica nulla.

## Lato gioco, per il thread di codice
Scarti 2 e 3. Lo scarto 1 è dati del kit di esempio (script di setup CP3/CP5).

## Decisioni dell'owner
Nessuna: ampiezza dello shake come impostazione, valore di default da provare nel playtest.

Numeri: px = `python cp5-data/tools/rv_diff.py cp5-hit-cue/cue_cue_hit.mp4 14 side`; mappa =
`python cp5-data/tools/rows_rle.py cp5-data/cue/cue_hit.jsonl`; baricentro e sagoma: misurati sul ritaglio lato
della vittima (x 215-360, y 100-345 della vista az90), non rifatti da uno script committato.
