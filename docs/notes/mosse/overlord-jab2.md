# Scheda mossa: Overlord jab 2 (v3, card `cards/overlord/jab/overlord_jab2_v3.json`)
Stato 2026-11-15: **FAIL** (`reviews/cp5b-jab-chain`: al contatto la sagoma da dietro è uguale al jab 1; tremore non visibile). Nessuna approvata/buona.

## 1. Identità
| # | Tratto | Riferimento | Chi |
|---|---|---|---|
| 1 | Schiaffo "ad artiglio" col palmo verso SINISTRA (opposto al jab 1), passo avanti, discesa diagonale | pose Cascadeur dell'owner (jab 2: palmo verso sinistra) | [OWNER] |
| 2 | Passo avanti breve: 15-20 cm col piede sinistro (la card ha 30 cm: dato provvisorio) | memoria progetto design | [OWNER] 15-20; card 30 [DATI] → [DA CONFERMARE] |
| 3 | Da dietro deve leggersi DIVERSO dal jab 1: mano all'anca, almeno 30 cm sotto la tenuta del jab 1 | review cp5b-jab-chain, istruzione round 2 | [DATI] review; [DA CONFERMARE] come regola dell'owner |
Tremore della mano / osservazione ("guarda la sua mano"): decisione dell'owner ancora aperta; la card r2 l'ha tolta, resta il layer Manuale f27-37.

## 2. Fasi
| Fase | Frame |
|---|---|
| Da jab 1 f12 (50%) | 0 |
| Rottura / caricamento | 1-6 (f6 ginocchio sinistro su, claw dietro la testa) |
| Rilascio, tallone | 7-8 |
| Contatto | 9 (affondo, braccio in diagonale discendente) |
| Rake in basso a sinistra | 12-13 (punto più basso) |
| Rientro, passi | 15-23 |
| FAF | 26 anim (27 gioco); idle f52 (30 cm avanti rispetto alla partenza) |

## 3. Curve [DATI] `spacing`/`part_delays`
| Segmento | Timing | Easing | Overshoot | Follow-through |
|---|---|---|---|---|
| Startup 0-8 | 9 f | `limb_led_recovery` | n/d | ritardi 0 |
| Attivo 9-11 | 3 f | `hold_recoil` | rake f12-13 | mano resta sul bersaglio per l'hitstop |
| Recupero 12-26 | | `limb_led_recovery` | nessuno | passi 15-23 |
Tremore: 78 key su `hand_r_fk_ctrl`/`lowerarm_r_fk_ctrl`, periodo 4 f, 5°→0,5° (layer Manuale, `anim-jab-5.md`). Altro: [DA CONFERMARE].

## 4. Hitbox e frame attivi: OWNER
| Cosa | Valore | Fonte |
|---|---|---|
| Finestra attiva | anim f9-f11 | card; CSV `OverlordJab2V3_timing.csv` frame 10-12 |
| Portata | 160 px da f10 | CSV |
| FAF | 26 / gioco 27 | card |
| Hitbox gioco | piazzata dall'owner; valori nel kit | [DA CONFERMARE] |
Avanzamento sul colpo in gioco 34,1 cm (`anim-jab-5.md`, lato gioco).

## 5. Blocchi
Approvata: nessuna. Buone: nessuna.
| n | Tag | Commit | Card | Data | Buono | Manca |
|---|---|---|---|---|---|---|

## 6. Domande aperte
1. Passo 15-20 cm (owner) vs 30 cm in card: consigliata 20 cm, vincolo di gioco (root motion) separato dalla posa.
2. Tremore: tenerlo? consigliata sì, ma visibile in game cam solo con ampiezza maggiore.
3. Capsula scende 12 cm quando parte il root motion (lato gioco, coda codice).

## 7. Storia
v2 → v3 (`anim-jab-5.md`) → cp5b: fix montage 30 Hz confermato, FAIL posa (round 2 di 3 da eseguire).
