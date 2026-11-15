# Scheda mossa: Overlord down-air v9 (card `cards/overlord/dair/overlord_dair_v9.json`)
Stato 2026-11-15: pose PASS da dietro e di lato (`anim-dair-10.md`), **FAIL gioco** (cp5b): gambe scattano alla posa di salto GASP, coda aerea a terra, attaccante entra nella vittima all'atterraggio. Nessuna approvata.

## 1. Identità
| # | Tratto | Riferimento | Chi |
|---|---|---|---|
| 1 | Carica: palla stretta, ginocchia al petto, avambracci incrociati davanti al viso | pose owner Cascadeur (`refs/overlord_dair_owner_poses.json`), key 15 | [OWNER] |
| 2 | Colpo: corpo dritto, inclinato all'indietro, gambe unite, braccia conserte alte sul petto, punte in giù | pose owner key 23 | [OWNER] |
| 3 | Dopo l'impatto: un solo rimbalzo dei piedi, poi il corpo affonda (pesante, non nervoso) | review v9; "too much is just right" | [DATI]; [DA CONFERMARE] come tratto |
Riferimento: Ganondorf dair (timing) + M. Bison head stomp (parte alta); griglie `reviews/overlord-dair-v7-rereview`, `cp5b-dair-v9`.

## 2. Fasi
Chiavi OWNER bloccate: **0, 3, 13, 14, 15, 37, 42** (brief coordinatore).
| Fase | Frame |
|---|---|
| Partenza | 0 |
| Carica | 3-10 (palla più stretta 8-10) |
| Rilascio / colpo | 13-15 (rottura 14, colpo 15) |
| Attivo | 15-17 |
| Rimbalzo | 18-20, assestamento 23, fine tenuta 26 |
| Recupero | 28-32 (tronco, braccia, testa sfalsati), 37, 42 = FAF |
Aereo: nessun idle, il frame 0 è già in tuck.

## 3. Curve [DATI] `spacing`, `part_delays`, `anim-dair-10.md`
| Segmento | Timing | Easing | Overshoot | Follow-through |
|---|---|---|---|---|
| Startup 0-14 | 15 f | `charge_long` | n/d | ritardi parte 0 |
| Attivo 15-17 | 3 f | `hold_recoil` | rimbalzo: piede sx +9,6 cm f19, dx +8,5 cm f20 | giù entro f23 |
| Tenuta 17-26 | | bacino -9 cm, piedi +4 cm, arco -10° | inversione sfalsata: bacino 26, busto 28, braccia 30, testa 32 | |
| Recupero 26-42 | | `limb_led_recovery` | | ogni parte morbida su f37 (87% entro 34) |

## 4. Hitbox: OWNER
Attiva anim f15-17 (card `anim_active`), timing UFD Ganondorf (startup 16, attivo 16-18, totale 44; FAF anim 42). Valori hitbox gioco: dati kit [DA CONFERMARE]. Frame attivi in gioco: non cambiano senza l'owner.

## 5. Blocchi
Approvata: nessuna. Candidata `good/dair-v9-1`: v9 per le pose (aria, impatto, rimbalzo), esclusi i difetti gioco. Commit: `claude/project-thread-se53y5` (origine di `anim-dair-10.md`) o integ-cp3 069287f7 per la card; hash esatto [DA CONFERMARE]. Tag: DA CREARE, comando: `git tag -a good/dair-v9-1 <commit con la card v9> -m "pose v9 buone; gioco da correggere"`.
| n | Tag | Commit | Card | Data | Buono | Manca |
|---|---|---|---|---|---|---|
| 1 | good/dair-v9-1 (DA CREARE) | [DA CONFERMARE] | overlord_dair_v9.json | 2026-11-15 | pose da dietro e lato, rimbalzo | blend-out a posa di salto, coda a terra, compenetrazione |
(Il blocco va dato dal revisore/owner: qui è una PROPOSTA.)

## 6. Domande aperte
1. Sezione di atterraggio per gli aerei (decisione owner pendente da cp5b). 2. Spinta fuori dei personaggi dopo l'atterraggio (codice).
3. Colonna da dietro nei 3 frame attivi: coprire con VFX (scia, anello) o no.

## 7. Storia
v1-v4 (pose owner 15/23, 02/11) → v5-v8 (capsula, varianti A-C) → v9 (rimbalzo, tenuta, carica) → cp5b: fix montage ok, FAIL gioco.
