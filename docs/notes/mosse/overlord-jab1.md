# Scheda mossa: Overlord jab 1 (v3, card `cards/overlord/jab/overlord_jab1_v3.json`)
Modello: `_modello.md`. Stato 2026-11-15: **FAIL** (review `reviews/cp5b-jab-chain`), nessuna versione approvata, nessuna `good`.

## 1. Identità
| # | Tratto | Riferimento | Chi |
|---|---|---|---|
| 1 | Schiaffo sprezzante col dorso della mano destra, braccio lungo e teso, che arriva da sinistra verso destra (diagonale sul petto del bersaglio, 29° al contatto f6, 36° f8-f12) | pose Cascadeur dell'owner (jab 1: schiaffo verso destra); card `_design` r2 | [OWNER] pose; angoli [DATI] card |
| 2 | Mano sinistra sul fianco durante lo schiaffo (f5-f12); prima era l'avambraccio sul ventre | card `_design` r2 | [DA CONFERMARE] (domanda 3 dell'owner, 11-15) |
| 3 | Arco completo: il braccio non si ferma a metà, tenuta "di disprezzo" f12 che respira, rientro all'idle eretto (no accovacciata) | owner 11-15 "lo schiaffo si ferma a metà" | [OWNER] |
Piccolo pestone del piede destro a f5 (17 cm, [DA CONFERMARE]). Riferimento visivo: clip owner `reviews/cp5b/q1_jab1_diagonale_mano_fianco_passo.mp4` (project files).

## 2. Fasi (frame anim, da 0)
| Fase | Frame | Note |
|---|---|---|
| Partenza/rottura | 0-2 | f2 bacino per primo |
| Caricamento | 3-4 | f4 chamber: busto girato 120° a sinistra, mano alla mascella sinistra, gomito sul bersaglio |
| Colpo | 5-6 | f5 anche + pestone, f6 contatto |
| Flick/overshoot/tenuta | 7-12 | polso f7, overshoot f8, tenuta f12 |
| Recupero | 13-22 | piede destro indietro f15-19; FAF anim 22 |
| Idle | 28 | |
Chiavi owner bloccate: [DA CONFERMARE] (le poses dell'owner in Cascadeur sono la fonte; la card non marca blocchi `approved`).

## 3. Foglio delle curve [DATI] card `spacing`, `part_delays`
| Segmento | Timing | Easing | Overshoot | Follow-through |
|---|---|---|---|---|
| Startup 0-5 | 5 f | `limb_led_recovery` | n/d | `part_delays` tutti 0 (il ritardo sta nei key) |
| Attivo 6-8 | 3 f | `hold_recoil` (tenuta + rimbalzo) | f8 oltre il contatto (polso f7, overshoot f8) | tenuta f9-12 quasi ferma: l'owner vuole l'arco intero, non la tenuta ferma [OWNER] |
| Recupero 13-22 | 10 f | `limb_led_recovery` | nessuno | piede destro indietro f15-19 |
Il resto (tangenti, ritardi per parte oltre ai key) è da scrivere da chi anima: [DA CONFERMARE].

## 4. Hitbox e frame attivi: DECISI DALL'OWNER
| Cosa | Valore | Fonte |
|---|---|---|
| Finestra attiva (animazione) | f6-f8 | card `anim_active`; CSV `OverlordJab1V3_timing.csv` frame 7-9 (CSV da 1) |
| Portata | 160 px al 100% da f7 | stesso CSV |
| FAF | anim 22, gioco 23 | card `faf_game` |
| Hitbox in gioco | "attiva per quasi tutto l'arco, la piazza l'OWNER" | memoria progetto 11-15 14:19Z; valori nei dati del kit (uasset): [DA CONFERMARE] |

## 5. Versione approvata e blocchi
- Approvata: nessuna. Tag previsto: `approved/overlord-jab1-<v>`.
- Buone: nessuna. Prima candidata: la v3 per quanto riguarda arco/sagoma da dietro, SOLO se l'owner o il revisore la giudica buona; non lo è ancora.
| n | Tag | Commit | Card | Data | Buono | Manca |
|---|---|---|---|---|---|---|

## 6. Domande aperte
1. Squat a fine mossa (piedi/capsula, lato gioco): bacino fermo su `ik_foot`; consigliata: fix codice/ABP, non posa.
2. Conferma: 29°, mano sul fianco, pestone 17 cm.
3. Telecamera di giudizio: dietro (gioco) o di lato (consigliata: dietro vince, lato = controllo).

## 7. Storia
v11/v12 pilota (`cards/pilot`) → v3 (ANIM-JAB-5, `handovers/anim-jab-5.md`: FAIL gambe piegate, tenuta ferma) → r2 diagonale + mano sul fianco → cp5b: pose OK da dietro in parte, FAIL gioco squat (`handovers/cp5b-rule10-review.md`).
