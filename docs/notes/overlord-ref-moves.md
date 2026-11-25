# Mosse di riferimento dell'Overlord (Ganon e Ike) e bersagli fissi

Richiesta owner 2026-11-25: "Forward Strong metti all'overlord il forward smash di Ganon. Strong neutrale l'ftilt di Ike. Neutral light
Aerial il nair di Ganon. Neutral strong aerial il nair di Ike. Che mi servono per fare playtest. Programma qualche superficie tipo cubi a
penetrazione diverse. Fissi che posso attaccare." E: "centra le animazioni e gli swing: se sono storti la precisione ne risente".

Fase greybox: pose da capsula fatte con numeri, nessuna animazione definitiva toccata, nessuna clip approvata toccata.

## Dove

| Cosa | File |
|---|---|
| Numeri, fonti, conversioni, percorsi delle hitbox (Python puro) | `Tools/GAS/overlord_ref_moves_data.py` (`python3` lo stampa) |
| Script editor: clip capsula, montage, timeline, abilità, binding sul kit, set di parti dei bersagli | `Tools/GAS/setup_overlord_ref_moves.py` |
| Cubi e cartelli nell'hub | `Tools/GAS/build_test_hub_map.py` (`TARGETS`, `penetration_targets`) |
| Bersagli in Play (pawn dentro i cubi, parti, regole) | `Tools/GASNPPTest/test_hub.py` (`spawn_targets`, `_targets_tick`) |

Riuso: abilità `UGSTN_GA_TestKitStrike` + timeline asset come jab e dropkick; hitbox con `AddHitboxTrack`; finestre di atterraggio
`FGSTN_TimelineTrack_Landing`; knockback con la formula Smash già nelle regole standard (`knockback_smash_data.py`); linguaggio delle pose e
rig della capsula di `greybox/make_capsule_clips.py`; parti, durezza e tabella di penetrazione di GSTNParts; manichini dell'hub.

## Comandi (kit `KIT_Test_Playtest`, quello dato al Play)

| Notazione | Comando | Contesto | Mossa |
|---|---|---|---|
| F.S | `Command.Strong.Directional` (RT + stick avanti) | a terra | forward smash di Ganondorf |
| N.S | `Command.Strong.Neutral` (RT) | a terra | ftilt di Ike (angolato avanti) |
| N.L Aerial | `Command.Attack.Neutral` (X) | in aria | nair di Ganondorf |
| N.S Aerial | `Command.Strong.Neutral` (RT) | in aria | nair di Ike |

Un binding del kit con lo stesso comando e contesto viene sostituito sul posto; se manca, il nuovo va per primo. I nuovi ignorano
`Combat.Link.Linked`, quindi durante una presa vincono i binding della presa. Il kit è salvato in `Saved/Backups/GAS/<ora>/` prima.

## Numeri (Smash Ultimate)

Frame Smash f (da 1) = frame di timeline f-1. BKB cm/s = BKB x 22.5. Hitstop = floor((danno x 0.65 + 6) x moltiplicatore), max 30.

| Mossa | Finestra (frame Smash) | Danno | Angolo | BKB | KBG | Hitstop | Totale / FAF | Atterraggio |
|---|---|---|---|---|---|---|---|---|
| F.S | early 29, clean 30-31 | 24 | 40 | 61 (1372 cm/s) | 75 | 21 | 90 / 69 | - |
| N.S | 12-13 | 13.5 | Sakurai | 30 (675) | 97 | 14 | 60 / 41 | - |
| N.L Aerial | colpo 1: 7-8 (late 9-12), colpo 2: 17-18 (late 19-26) | 7 / 5.25, 12 / 9 | 70, Sakurai | 20, 25 | 30, 106 | 10/9, 16/14 | 55 / 40 | autocancel 1-3 e 37+, lag 10 |
| N.S Aerial | clean 10-14, late 15-22 | 7.5 / 6 | 80 | 35 (787) | 130 | 10 / 9 | 68 / 59 | autocancel 1-2 e 50+, lag 8 |

Fonti (lette 2026-11-25): ultimateframedata.com/ganondorf e /ike; ssbwiki.com Ganondorf_(SSBU)/Forward_smash, Ganondorf_(SSBU)/Neutral_aerial,
Ike_(SSBU)/Forward_tilt, Ike_(SSBU)/Neutral_aerial. Disaccordi risolti così: ftilt di Ike 13.5 % (SmashWiki SSBU; UFD ha il 12.5 di Smash 4);
landing lag dei nair da UFD (Ganon 10, Ike 8).

Semplificazioni dichiarate: fsmash non caricabile; ftilt solo angolato avanti; nair di Ganon colpo 1, hitbox del ginocchio unica (70 gradi,
20/30) perché la timeline non distingue vittima a terra o in aria; raggi del colpo 2 late del nair di Ganon presi dal riassunto SmashWiki, non
letti parola per parola. Scala: 1 unità Smash = 9.5 cm (ASSUNZIONE: Overlord ~190 cm alto come Ganondorf ~20 unità), un solo numero
(`UNIT_CM`) da ritoccare per tutte le portate.

## Centratura (owner 2026-11-25)

Tutti i percorsi delle hitbox stanno sull'asse in cui guarda il personaggio: archi verticali e calci nel piano X-Z (y = 0), lo swing
orizzontale di N.S simmetrico (+60 -> -60 gradi), quindi a metà dei frame attivi la lama è dritta davanti. Le pose della capsula sono
simmetriche (F.S a due mani) o passano dalla direzione di sguardo a metà dei frame attivi (N.S: torso +45 -> 0 -> -45).

## Penetrazione e bersagli

Proposte per il playtest (decide l'owner): F.S 6 ("penetrazione media"), N.S 4, N.L Aerial 2, N.S Aerial 4. Le mosse vecchie non hanno dati
di parte: penetrazione 0.

Bersagli in piazza (riga a y = 15 m davanti alle partenze): durezza 0, 2, 4, 6, 8, 12 (cubo bianco, verde, giallo, arancio, rosso, grigio).
Ognuno è un manichino dentro il cubo con una sola parte indistruttibile su tutte le sue hurtbox: con la tabella di default penetrazione >
durezza = danno pieno, = 65 %, < rimbalzo. Non si muovono mai (stagger sul posto, knockback x0; se un corpo li spinge il server li rimette nel
cubo). A schermo: "COLPITO bersaglio durezza N: +x%" o "RIMBALZO".

## Sul fisso

1. Editor chiuso: `.\Tools\CI\Save-Assets.ps1 -Script Tools\GAS\setup_overlord_ref_moves.py` (ESITO e report in
   `Saved/OverlordRefMoves/setup_overlord_ref_moves.txt`). Modifica il Blueprint condiviso `ABP_GSTN_GreyboxCapsule` (solo MoveOverrides,
   unite) e `KIT_Test_Playtest`: `REF_MOVES_CAPSULE=0` salta l'ABP.
2. Rigenera la mappa dell'hub con `build_test_hub_map.py` per vedere i cubi (senza, i bersagli ci sono lo stesso ma invisibili tranne il manichino).
3. Play in `L_Test_Hub`. Nessuna build C++ serve: solo dati e Python.

Non provato: niente di questo è stato eseguito nell'editor (macchine spente il 25-11).
