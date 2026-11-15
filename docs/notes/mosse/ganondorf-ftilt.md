# Scheda mossa: Ganondorf ftilt v3.1 (card `cards/overlord/ftilt/overlord_ftilt_v31.json`, kit overlord)
Stato: costruita nel motore unico (fase B, 11-02), "verde con note" in gioco-analisi; approvazione dell'owner sul braccio sinistro (domanda 4b) non registrata nel repo: [DA CONFERMARE].

## 1. Identità
| # | Tratto | Riferimento | Chi |
|---|---|---|---|
| 1 | Calcio frontale con la gamba sinistra, sagoma molto leggibile, peso sulla gamba d'appoggio | owner 5 frame: `refs/owner_ganon_ftilt` (first 0, anticipation 7, active 10, recovery 27, last 37) | [OWNER] pose |
| 2 | Braccio sinistro: gomito dietro dentro il profilo, avambraccio non nel fianco; da davanti non deve "chiudersi a triangolo" al f10 | `decisioni-2026-11-02.md` §4b | [DATI]; esito [DA CONFERMARE] |
| 3 | Gamba che scende in un blocco solo dal f20: ora una discesa morbida su 13 frame | card `drop` | [DATI] |
Riferimento: `ganon_ftilt_v31_poses_ab.png` (project files).

## 2. Fasi
| Fase | Frame | |
|---|---|---|
| Primo | 0 | OWNER |
| Carica | 3 | |
| Anticipo | 7 | OWNER |
| Snap | 9 | blend 85% verso f10 (`hitstop_blend`) |
| Attivo | 10 | OWNER; finestra attiva e FAF: non nella card, vedi CSV UFD `GanondorfFTilt_timing.csv` (non nel repo: [DA CONFERMARE]) |
| Tenuta/atterraggio | 20, 23, 25 | |
| Recupero | 27 (OWNER), 32 | |
| Ultimo | 37 | OWNER |
Chiavi owner bloccate: 0, 7, 10, 27, 37.

## 3. Curve [DATI]
| Segmento | Easing | Overshoot | Follow-through |
|---|---|---|---|
| Startup | `charge_long`; load f3 `arrive_1_2` | n/d | testa 1 frame di ritardo (`part_delays.head=1`) |
| Attivo | `hold_recoil`; hitstop_blend f9→10 85% | | |
| Recupero | `limb_led_recovery`; gamba in discesa f20-33 | | |

## 4. Hitbox: OWNER
Frame attivi e hitbox: dati UFD/kit; hitstop di analisi 8 (ipotesi, da tarare in combat, `handovers/anim-pipeline.md`). Valori reali: [DA CONFERMARE].

## 5. Blocchi
Approvata: [DA CONFERMARE] se l'owner ha detto OK alla 4b (consigliata: se sì, tag `approved/ftilt-v31` sul commit della card). Buone: nessuna.
| n | Tag | Commit | Card | Data | Buono | Manca |
|---|---|---|---|---|---|---|

## 6. Domande aperte
1. L'owner ha approvato la v3.1 (4b)? 2. Ganondorf è la mossa di riferimento "Ganon" o del kit Overlord? (la card è sotto `overlord/`).

## 7. Storia
Pilot AS_Pilot_GanonFTilt_V3 → build v3.1 (11-02) → braccio sinistro corretto (varD) → analisi gioco "verde con note".
