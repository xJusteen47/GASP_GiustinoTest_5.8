# Kit di playtest, L3, stick analogico, partenza/rotazioni — 2026-11-16 (fisso, Opus, _ue59)

Branch `claude/kit-playtest-1116` da `claude/integ-playtest-1116` b528f9a8. Uniti: 3j0pgk (attacchi/roll verso lo stick), 33ixjz (corsa P2/P3),
brrdze (query di stato BP), xe8mcz (crouch a terra). Build: `Saved/Logs/ci/kit1116_build6.log` ESITO OK (2 fix: AirControl P2/P3, InputCore). Test `Run-Timed.ps1 -Preset Test -Tests "GSTNCombat+GSTNDefence+GSTNHeld"`:
755 Success / 0 Fail (`grep -c 'Result={Success}' Saved/Logs/ci/kit1116_test.log.ue.log`).

## Fatto
- **L3 / volo GASP**: non era un mapping ma un nodo tasto L3 in `SandboxCharacter_Mover` (debug flying). `IA_GASNPP_RunBoost` ora consuma
  il tasto dai legacy key event (`bConsumesActionAndAxisMappings`, eventi Started|Ongoing|Triggered). Nessun BP toccato; Z da tastiera resta.
- **Stick**: deadzone `IMC_GSTN_Locomotion` 0.3 → 0.12. Nuovo `FGSTN_AnalogWalkTuning` nel profilo di locomozione (`AnalogWalk`):
  curva inclinazione→velocità (MinSpeedFraction 0.12, esponente 1.4), boost di accelerazione ×2.5 e TurningStrength 30 a bassa velocità,
  AccelerationSmoothingTime 0.03, facing a terra 0.03 s da fermo / 0.08 s in corsa (prima 0.25+ del GASP), WalkSpeed = velocità dello stadio
  (il gait walk non limita più), rotazione in aria 540°/s (bloccata durante le mosse, `bAirTurnDuringMoves`). Tutto input/proprietà
  scritte ogni frame dallo stato del tracker: rollback-safe, 62.5 Hz invariati.
- **Menu loadout a inizio PIE**: tolto (`DefaultGSTNLoadoutMenu.ini`, anche il pad). Finestra GSTN Debug Options: riga **Kit** + Apply.
  `gstn.Debug.Kit <nome>`; kit iniziale `gstn.Debug.KitAtStart=KIT_Test_Playtest` solo in `gstn.Debug.KitAtStartLevels=L_Test_Hub`
  (`Config/DefaultEngine.ini` [ConsoleVariables], tooling di debug).
- **KIT_Test_Playtest** (`Tools/GAS/setup_playtest_kit.py`): copia del kit dropkick (= Hits + dropkick con timeout aria) + estensioni
  di Hits/Light (difesa get-up, spin, locomozione, presa, air crouch). Log `Saved/PlaytestKit/setup_playtest_kit.txt`.
- **Lanci dal ~20%**: `GSTN_CombatRules_Standard` LaunchThreshold 1080 → 750, PercentGrowth 1 → 1.8 (550 lancia da ~20%, 900 subito).
- Fix sul branch P2/P3: `AirControlPercentage` sta su `UFallingMode` in questo Mover (non nelle shared settings).

## Scelte applicate (regola 6)
Valori di default sopra; regole standard cambiate (non solo il kit: la soglia di lancio è delle regole). Back tilt: nessun binding
"indietro" aggiunto; gli slot Directional escono verso lo stick (3j0pgk).

## Non fatto / da fare
- PIE non eseguito in questo thread (budget): da provare dall'owner (lista nel thread). Verificare che l'ABP GASP si abbassi col crouch.
- Esempi dati di Landing track, Ledge check su un dash, finestra di take-off: il codice c'è, nessuna mossa li usa ancora → thread dati.
- Livello separato `L_Test_Lab` con i kit strani e pedane del hub ridotte al kit unico: non fatto (le pedane sono in `build_test_hub_map.py`).
- `AM_OverlordDAirV9` era sporco prima: non committato.
