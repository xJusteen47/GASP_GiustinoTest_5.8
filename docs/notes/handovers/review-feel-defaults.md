# Review dei default di feel/tuning (cloud, Fable 5.2 high, sola lettura, 2026-11-17)
Richiesta owner 19:56Z. Nessun codice o asset toccato. Report completo: project files `handoffs/review-feel-defaults.md`.
## Valori letti da
ini `Plugins/GSTNCombatGAS/Config/DefaultGSTNCombatGAS.ini` (cocsel 3fd73243), `Tools/GAS/setup_playtest_fixes_1117.py` r.23-29 (jprs4t a41f5bac),
`GSTN_RunSkidTypes.h`, `GSTN_RunStageTypes.h`, `GSTN_ImpactTypes.h:100-124`, `GSTN_KnockbackModel.cpp:96-99`, handover dodge-fallback-1117,
wall-ceiling-bounce-1, launch-light-kit-1, playtest-fixes-1117c, qa-movement-1117.
## Esito
- Tieni: squat 3 f, salto pieno/tap, air control, air turn 3600, stage 470/610/850 e rampa, release brake, skid (120°), L3, dead zone 0,08,
  soglie di lancio, pesi, tech bounce e bump, auto get-up 90 f, hurtbox Simple.
- Cambia (solo dati): MaxFallSpeed 1800 -> 1500; FastFall 2600 -> 2400 quando torna; camera playtest extra 300 -> 550 (cvar).
- Misura prima: altezza del doppio salto (AirJumpVelocity 900: con o senza hold?), quanti rimbalzi duri a 1200 cm/s su lanci normali
  (gravità 4,1: bastano ~180 cm di caduta), minimo a terra prima del get-up.
## Decisioni portate all'owner (13d)
1. Dodge in catena: blocco ~10 f dopo l'atterraggio (consigliata) / penalità alla Ultimate / lasciare.
2. Crescita del lancio col % per mossa: moltiplicatore per mossa (consigliata) / modello Smash già nel codice / lasciare.
3. Stage 2->3: 60 f (consigliata) / 94 f / 60 f + L3 allo stage 3 dallo sprint 1.
## Limiti
Numeri Smash controllati su ssbwiki solo per gravità, run, air speed, squat, tumble; il resto a memoria. Confronti con Odyssey/Astro Bot a occhio.
Camera: quota di schermo stimata con FOV 90 non verificato. Niente build, niente test (nessun file di gioco cambiato).
Costo stimato ~6 $.
