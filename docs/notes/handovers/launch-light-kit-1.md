# Lancio delle mosse del kit LEGGERO a ~20% — 2026-11-16 (cloud, Opus, ~3 $)

Branch `claude/project-thread-hewki6` da `claude/kit-playtest-1116` 9658b271. Solo diagnosi + script dati, niente C++, niente asset.

## Verificato
- `GSTN_CombatRules_Standard.uasset` in `_ue59` (lettura binaria, sola lettura): modello `GSTN_KnockbackModel_WeightDriven`,
  LaunchThreshold 750, PushBackThreshold 540, PercentGrowth 1.8, TumbleThreshold default 1800, DefaultWeightCurve 1.0 → 0.6 (su %/MaxHealth,
  a 20% quasi 1). Il fix del 11-16 è caricato: regole e formula NON sono il problema.
- Formula (`GSTN_KnockbackModel.cpp`): K = (Base + Growth·Danno) · (1 + 1.8·%/100) · 100/Peso. Il % è quello PRIMA del colpo.
  < 540 resta sul posto, 540-750 push back, ≥ 750 lancio.

## Perché il LEGGERO non lancia (manichino peso 100, 20%: fattore 1.36)
| Mossa (fonte `setup_test_kit_archetypes.py`) | Base | K a 20% | Esito | Lancia da |
|---|---|---|---|---|
| X neutro, GA_Light_Jab (3%, 30°) | 300 | 408 | sul posto | ~86% |
| X con stick, mossa di WPN_Test_Hits (non sovrascritta) | 500 | 680 | push back | ~28% |
| Y special, GA_Light_Special (5%, 80°) | 700 | 952 | lancio | ~4% |
| RT, GA_Example_Strong (14%) | 1100 | 1496 | lancio | 0% |
Causa: il fix era tarato su una mossa da 550; il LEGGERO non ha una mossa X da lancio (jab 300, X direzionale ereditato 500).

## Fatto (script, da eseguire sul fisso)
`Tools/GAS/setup_test_kit_archetypes.py`: nuova mossa `tilt` (X + stick, `Command.Attack.Directional` sull'arma) per i tre archetipi:
leggero 5%/560/40° → lancia da ~19%; medio 8%/620 → ~11%; pesante 12%/700 → ~4%. Jab invariati (come Smash: il jab non lancia).
Placeholder montage (shove/takedown F), frame 20/26/36.

## To-do desktop (editor chiuso)
1. `UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<proj>/Tools/GAS/setup_test_kit_archetypes.py" -unattended -nullrhi`
   (attenzione: riscrive jab/special/throws/stats dei tre kit con i valori dello script; tuning a mano su quei kit andrebbe perso).
2. Log `Saved/Logs/k3/setup_archetypes.log`: nessun WARNING "no Command.Attack.Directional binding".
3. PIE: LEGGERO, X+stick su manichino a 20% → deve volare.

## Da confermare (owner)
- Il jab che non lancia fino a ~86% va bene (stile Smash)? Alternativa globale: LaunchThreshold 600 / PushBack 400 (tutti i kit lanciano prima).
- Non verificato sull'asset quale abilità sta oggi su X direzionale del LEGGERO (stimato da `SOURCE_WEAPON = WPN_Test_Hits`, base 500).
