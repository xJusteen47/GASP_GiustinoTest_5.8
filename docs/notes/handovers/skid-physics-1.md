# skid-physics-1 (cloud, 2026-11-21, Opus high) - skid con velocità fisica (frena, passa per zero, riparte), NON compilato
Branch claude/project-thread-4adnj1 (da claude/runstart-generic-desktop dd7e843a). Nessun asset toccato, solo C++ + 1 script QA + doc.

## Causa del "bloccato" (owner 2026-11-21)
1. `gstn.Skid.Sim` è 1 di default (GSTN_CombatSimRunSkid.cpp:22): ogni inversione dal playtest avvia il NOSTRO layered move, non lo stock.
   Le misure da 0,26-0,3 s di skid-reversal-1 / pivot-overlord-1 erano con `gstn.Skid.Sim 0` (lo dicono i loro handover, riga 2).
2. Il DA ha RootTrack + TurnCurve cotti (skid-fix-1, divergenza 10): il move faceva OverrideVelocity con la root della clip per 0,83 s.
   Frame 1: la velocità di corsa sostituita da quella della clip (salto), poi stick ignorato fino a fine clip = corpo "incollato".

## Fatto
1. `EGSTN_RunSkidMotion` + `FGSTN_RunSkidPhysicsTuning` in FGSTN_RunSkidTuning (DA > Run Stages > Skid): Motion default **Physics**;
   ClipRoot (= comportamento di ieri) e BrakeExit (prima versione P2) selezionabili. Console `gstn.Skid.Motion` (-1 DA, 0, 1, 2).
2. Physics (GeneratePhysics nel move): frenata DALLA velocità del frame prima lungo la vecchia direzione, BrakeDecel 3600 con onset su 3
   frame e BrakeCurve (X = velocità rimasta / iniziale); a zero, spinta verso lo stick PushAccel 3000 con PushCurve fino a PushSpeed 420,
   poi la walking mode continua dalla stessa velocità (+ boost reversal della riga 16). Rotazione sul PROGRESSO (0 inizio, 1 fermo, 2 fine
   spinta, TurnStart/EndProgress 0/1). Stick di nuovo avanti in frenata (cos >= 0,5) o rilasciato prima della spinta = fine skid, la corsa
   continua dalla velocità che c'è. Tutto deterministico: integrazione da frame 0 a ogni step dai campi serializzati + curve del DA.
   Sulla carta (472 cm/s, valori default): fermo in 10 frame (0,16 s, ~40 cm), 420 nell'altro verso in altri 9 (0,30 s totali, Toad).
3. Presentazione: la clip skid (base 0) segue le fasi della sim; gstn.Skid.PlayAtClipSpeed vale solo per ClipRoot (IsSkidClipSpeed).
   Base 1: il pivot del chooser legge la traiettoria della capsula, quindi segue già la fisica.
4. Debug Options: gstn.Skid.Sim (check), gstn.Skid.Motion (numero), gstn.Skid.PlayAtClipSpeed (check). TurnSide escluso (vedi commento).
5. Test: GSTNCombat.Core.RunSkid.Physics (continuità, riposo, spinta, curve, rotazione), NP.RunSkid.NetSerialize esteso,
   Debug.OptionsWindow controlla i due cvar. move_reversal misura `max_step_cms` (salto massimo per tick), soglia QAM_REV_MAX_STEP.
6. Divergenza 22 in gasp-divergenze.md (riga 10 non più default). Regola 14: risposta Assistant del 2026-11-20 riusata (stock = nessuna
   frenata dedicata, suggerisce boost temporaneo nel sync state); se il fisso vuole, richiedere con il testo della riga 22.

## Sul fisso (in ordine, max 2 tentativi)
1. `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build_skidphys.log` (build vera, non Live Coding).
2. `Tools\CI\Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test_skidphys.log -Tests GSTNCombat.Core.RunSkid` poi `-Tests GSTNCombat.NP.RunSkid`
   e `-Tests GSTNCombat.Debug.OptionsWindow`; esito con `Tools\CI\Check-TestLog.ps1`. Poi la suite `-Tests GSTN` come al solito.
3. Misura A/B, stesso DA: `$env:QAM_REV_CVARS='gstn.Skid.Sim 1;gstn.Skid.Motion 0;gstn.Loco.Base 1'; $env:QAM_REV_MAX_STEP='90';
   Tools\GASNPPTest\run_movement_qa.ps1 -Names move_reversal -Tag skidphys0`, poi Motion 1 (`-Tag skidphys1`, atteso salto grande = il
   bug) e `gstn.Skid.Sim 0` (`-Tag skidstock`). Attesi Motion 0: run_s 0,24-0,36, max_step <= ~60, srv = cli, 0 reconcile.
4. Owner: PIE, inversione di corsa, prova `Skid.Motion` 0/1/2 e `Skid.Sim` dal menu GSTN Debug Options.

## Da sapere
- Non compilato: errori di battitura possibili (2 tentativi). Nessun asset salvato: il DA carica Motion = Physics dal default C++.
- Spinta lungo la direzione dello stick del PRIMO frame (come il lato di rotazione); lo stick prende il controllo al passaggio a 420.
- Al passaggio la walking mode riparte con lo stato molla suo: un piccolo scalino d'accelerazione è possibile, si vede in max_step.

COSTO: ~5 $ | 0 min desktop | 0 token review
