# Preset movimento "Mario 3D World" (base di partenza)

Thread cloud 2026-11-16/17, Opus. Solo dati: nessun codice cambiato. Valori da applicare sul FISSO (asset binari + ini).
Codice letto: branch `claude/kit-playtest-1116` 9658b271 (`GSTN_RunStageLogic.cpp` MakeDefault, `GSTN_RunSkidTypes.h`,
`GSTN_AnalogWalkTypes.h`, `GSTN_JumpLogic.cpp`, `Plugins/GSTNCombatGAS/Config/DefaultGSTNCombatGAS.ini`, `Tools/GAS/setup_test_kit_archetypes.py`).

## Da dove vengono i numeri
- Fonte: parametri interni di SM3DW (Mario wiki, "Super Mario 3D World playable characters' statistics"): unità/frame a 60 fps.
  Mario: DashMaxSpeed 9, JumpPow 18,395, JumpPowLow 16,37, SuperDashTimer 90 f, TurnJumpPow 26,93 (salto dopo skid), DashBrakeFrame 20,
  BrakeFrame 14 (Luigi), BrakeFrameOnIce 55, JumpPowCountMax 10-11 (Peach 10, Mario invincibile 11), Gravity 1,0 (Rosalina; Mario = default).
- Scala (STIMA, da confermare a video): Mario ≈ 160 unità (come Odyssey, stesso motore) → manichino 180 cm = ×1,125.
  Velocità: u/f × 67,5 = cm/s. Accelerazione/gravità: u/f² × 4050 = cm/s² (= 4,13 × gravità 980).
- Il salto di 3DW tiene la velocità COSTANTE finché tieni premuto (max ~11 f), poi gravità piena. Da noi: RiseGravityScale ≈ 0 + MaxHoldFrames.
- Risultato salto pieno: apice ≈ 408 cm (2,3 altezze) in 30 f, aria totale ≈ 58 f (0,93 s). Oggi: ≈ 250 cm in 45 f, aria ≈ 84 f (1,35 s) = fluttuante.
  Salto corto (tap): ≈ 230 cm (1,3 altezze).

## Tabella (default del progetto; i kit scalano sopra, vedi sotto)
| Campo (dove) | Attuale | Proposto | Perché |
|---|---|---|---|
| Run Stage 1 MaxSpeed (DA_Locomotion_Default > Run Stages) | 450 | 470 | Normal ~7 u/f × 67,5 |
| Stage 2 MaxSpeed | 650 | 610 | DashMaxSpeed 9 × 67,5 |
| Stage 3 MaxSpeed | 850 | 850 | SuperDash ~12,5 (Peach 12, Toad 16) |
| Stage 1/2/3 Acceleration | 1800/1400/1200 | 2000/1400/1200 | velocità piena in ~0,24 s (STIMA). La rampa d'avvio resta al piano run-start |
| Stage 1/2/3 TurningStrength | 12/8/5 | 16/10/6 | curve strette a corsa, più larghe a sprint |
| Stage 1/2/3 ReleaseBrake | 4000/2500/1800 | 3200/1900/1700 | stop in 9 / 20 / 31 f: poco scivolo a stage 1 (regola owner), DashBrakeFrame 20 a stage 2 |
| Stage 1 FramesToNextStage | 40 | 40 | invariato (in 3DW lo sprint è col tasto; da noi tempo + L3) |
| Stage 2 FramesToNextStage | 60 | 94 | SuperDashTimer 90 f a 60 Hz = 1,5 s |
| Stage 1/2/3 AirControlScale | 1/0,8/0,6 | 1/0,9/0,75 | 3DW tiene molto controllo anche a sprint |
| Skid Brake | 2600 | 2600 | da 610 cm/s stop in 15 f ≈ BrakeFrame 14 ✓ |
| Skid MinSpeed / ReverseStickCos | 350 / -0,5 | invariati | già in linea |
| Air MinAirSpeed | 300 | 470 | da fermo in 3DW ti muovi in aria a velocità di corsa |
| AnalogWalk AirTurnRateDegPerSec | 540 | 3600 | rotazione in aria "quasi istantanea": 180° in 3 f |
| AnalogWalk StartFacingTime / RunFacingTime | 0,03 / 0,08 | 0,02 / 0,05 | pivot a terra più secco |
| AirControl (ini, stat) | 0,35 | 0,85 | controllo in aria forte, inversione a metà salto |
| JumpVelocity (ini) | 700 | 1240 | JumpPow 18,395 × 67,5 |
| JumpRiseGravityScale | 1,0 | 0,0 | velocità costante finché tieni (3DW). 0,15 se lo vuoi meno lineare |
| MaxJumpHoldFrames | 12 | 11 | ~11 f a 60 Hz |
| MinJumpHoldFrames | 4 | 2 | tap ≈ 1,3 altezze |
| JumpCutGravityScale | 2,0 | 4,1 | gravità 3DW 1,0 u/f² = 4,13 × 980 |
| PlatformJump CutRampFrames (abilità salto, kit) | 0 | 3 | tuo desiderio: al rilascio la gravità sale in 3 f fino a quella di caduta |
| FallGravityScale | 1,25 | 4,1 | stessa gravità di 3DW (4,5 se vuoi cadute più secche) |
| MaxFallSpeed | 1500 | 1800 | STIMA: raggiunta a fine caduta da un salto pieno |
| FastFallGravityScale / FastFallSpeed | 2,0 / 2200 | 6,0 / 2600 | va alzata, sennò il fast fall sarebbe PIÙ LENTO della caduta normale |
| AirJumpVelocity | 600 | 900 | Mario non ha doppio salto; scalata per restare ~60% del salto |
| PlatformJump CoyoteFrames | 4 | 6 | ~0,1 s, perdonante come i Mario 3D |
| JumpSquatFrames (ini) | 3 | 0 (Mario) | 3DW non ha squat; 3 è da picchiaduro → da confermare (vedi sotto) |
| Superfici ghiaccio: Locomotion.FrictionScale | — | 0,25 | BrakeFrameOnIce 55 / 14 |

Invariati: Weight, GroundFriction/Braking (le stage li sovrascrivono), parametri di skid ledge/jump window.

## Kit (stessa scala, altrimenti il preset si perde)
I kit hanno valori ASSOLUTI di salto e gravità: vanno scalati come il default. Proposta (Light / Medium / Heavy, `setup_test_kit_archetypes.py` + asset kit):
JumpVelocity 820/720/620 → 1450/1270/1100; FallGravityScale 1,1/1,25/1,6 → 3,6/4,1/5,3 (e stesso valore su JumpCut);
AirControl 0,55/0,35/0,2 → 0,95/0,85/0,65. WalkSpeed resta (fa da fattore sulle stage: Light 760/600 → sprint 2 ≈ 1080 cm/s).

## Campi che mancano nel codice (non fatti, solo segnalati)
1. Salto più alto se corri (JumpPowLow 16,37 da fermo vs JumpPow 18,395 in corsa: ~+12%). Serve "JumpVelocity per velocità/stage". ~2-3 $.
2. Salto dopo skid (side flip): TurnJumpPow 26,93 → 1820 cm/s, gravità 0,85 (≈3,5), spinta orizz. 5 → 340 cm/s. Oggi la finestra di salto dello skid fa un salto normale. ~3-4 $.
3. Backflip da accovacciato (28,9 → 1950 cm/s) e salto lungo (orizz. 15,74 → 1060 cm/s): mosse nuove, solo idea.
Nessun altro campo manca: rampa di gravità al rilascio, hold massimo e gravità salita/taglio/caduta esistono già.

## Da confermare (owner)
- Scala ×1,125 (Mario 160 u): se a video sembra troppo/poco veloce, cambia solo questo fattore (tutto scala insieme).
- JumpSquatFrames 0 (Mario) o 3 (picchiaduro): il campo c'è già, è una scelta di gioco.
- Altezza salto pieno 2,3 altezze: alta per un'arena PvP; alternativa più bassa = JumpVelocity 1100 e hold 9 (≈ 320 cm).

## Non duplicare
Dead zone unica, rampa d'accelerazione per stage, MinSpeedFraction/StartAccelerationMultiplier: restano al piano `handoffs/run-start-skid-import-piano.md`.
Qui solo i valori bersaglio di velocità/accelerazione delle stage.

## Applicazione (fisso, ~1-2 $ Haiku)
1. ini `DefaultGSTNCombatGAS.ini` (testo) + abilità salto + DA_Locomotion_Default + kit, via script/MCP editor.
2. Test che controllano 450/650/850 (`GSTN_CombatTests_RunStages`) leggono i valori: cambiare l'ASSET, non MakeDefault, li lascia verdi.
3. Playtest: salto pieno, tap, rotazione in aria, stop da stage 1.

Fonte: https://www.mariowiki.com/Super_Mario_3D_World_playable_characters'_statistics
