# Integrazione playtest 1116 — 2026-11-16

Thread desktop, branch `claude/integ-playtest-1116` da `claude/debug-hitbox-hub` bb8fabdc. _ue59 compilata e allineata.

## Dentro
- debug-hitbox-hub (pad debug, hitbox) + pkfl8v (yaw offset: già contenuto).
- pcq7jx (Landing track, ground/air rules, coyote, dropkick air timeout), ubv88e (air crouch), 4n8gx9 (tumble recovery + balloon, contiene ujco9r ledge check), wz5g4d (gravità relativa, porta anche ci-runner-setup da main).
- Merge senza conflitti. Fix di compilazione dopo wz5g4d: `GSTN_BodyCapsuleModifier.cpp:245` ternario FVector4/FVector ambiguo (commit e97e0a64).

## Prove
- Build: `Saved/Logs/ci/integ1116_build3.log` (ESITO OK). Test: `Run-Timed.ps1 -Preset Test -Tests "GSTNCombat+GSTNDefence+GSTNHeld"`, 734/734 Success, 0 Fail
  (`grep -c 'Result={Success}' Saved/Logs/ci/integ1116_test.log.ue.log`). Inclusi GravityFrame, Balloon, TumbleRecovery, NP.Ledge, AirTimeout, Landing, AirCrouch.
- Setup headless (`Saved/Logs/ci/integ1116_setup.txt`): kit dropkick (AM e TL KEPT dal guard), air crouch OK, hub_debug_pad OK (L_Test_Hub salvata, pad a 300,-800).

## Scelte applicate
- Dropkick: hold/capsule script NON forzati (guard = hitbox a mano dell'owner, coordinatore). TL/GA/WPN/KIT dropkick riportati al commit;
  poi solo `MaxAirFrames = 125` scritto sulla traccia Hold esistente (start 21, end 26, land 59 invariati, 4 tracce) via `Saved/Logs/ci/integ1116_airtimeout.py`.
- KIT_Test_Hits riportato al commit (regola dei dirty): non ha i dati air crouch. AM_OverlordDAirV9 era già sporco prima: non committato.
- Air crouch: clip PLACEHOLDER (copia del crouch idle), da sostituire via pipeline animazioni.

## Da fare
- PIE del owner (lista nel thread). Balloon resta OFF (CombatRules_Standard non salvato).
