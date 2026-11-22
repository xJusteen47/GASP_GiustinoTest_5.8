# Gravità: audit di tutte le meccaniche (thread cloud, sola lettura, 2026-11-16)
Branch letti: feature/gas-npp-5.8, claude/project-thread-4n8gx9 8d4b21e5 (pila pcq7jx/ujco9r), claude/mario-run-int, claude/project-thread-ubv88e, claude/hit-react-int. Comando: grep di UpVector/.Z/.Yaw/980/GetGravityZ sui .cpp/.h GSTN non test (script /tmp, non salvato). Niente compilato, niente cambiato.
Lettura scelta: (a) FORZA diversa (zone a bassa gravità) e (b) DIREZIONE diversa (Galaxy). Già decisione owner: design-direction §2d e §6l, spec docs/architecture/gravity-volumes.md (GSTNGravity, NON costruito).

## Funziona con gravità di forza e direzione qualsiasi (usa Up = -gravità locale)
Salto/fast fall/fasi di gravità (GSTN_JumpLogic, GSTN_CombatSimJump.cpp:173), coyote, knockback (GSTN_LayeredMove_Knockback.cpp:38-45), balloon, DI/stick (StickToPlane), momentum slide e pushback con sonde bordo, Ledge Check (GSTN_CombatSimAttacker.cpp:63-85), pushbox/core (GSTN_Pushbox.cpp:82-136), trampolini (GSTN_SurfaceLaunch.cpp), tumble contact, run stages (GroundVelocity sul piano, RunStageSyncState.cpp:149), landing track/air timeout/hitstun (frame), hitbox/hurtbox (spazio osso/attore), camera (Up da gravità + trasporto parallelo), minion (GravitySource).

## Assume Z del mondo o 980 fisso
1. Facing salvato come yaw del mondo: GSTN_SetFacingEffect.cpp:13 (cambia lo yaw Euler tenendo pitch/roll), CombatSimVictim.cpp:239/629, CombatSimComponent.cpp ~821-886 (GetActorRotation().Yaw). Con capsula inclinata (Mover bShouldRemainVertical la allinea all'Up) gira sull'asse sbagliato.
2. Girarsi verso l'attaccante: GSTN_CombatTick_Victim.cpp:702 (ToAttacker.Z = 0, FRotator(0,yaw,0)).
3. Capsula che si accorcia (air crouch, capsula d'atterraggio): GSTN_BodyCapsuleModifier.cpp:240 (shift nel frame yaw).
4. Centro pushbox/core nello snapshot: GSTN_CombatSimSnapshot.cpp:276 (frame yaw).
5. Input: CameraYaw = ControlRotation.Yaw (GSTN_InputSource.cpp:86) + riferimento "avanti del mondo" proiettato (CombatTick_Victim.cpp:113): su un pianeta lo stick deriva rispetto alla camera e ha una singolarità ai poli.
6. Forza fissa 980: GSTN_TumbleLanding.cpp:318 (durata del mini rimbalzo), GSTN_MoveTimelines.h:42 (archi di dodge/dash e finestra intangibile fatti a 980), GSTN_HeldTypes.cpp:17/246 (lanci a -980 su Z).
7. Presentazione: GSTN_ReactionAnimComponent.cpp:218 (hit-react, Push.Z=0), GSTN_AnimNode_DirectionalHitAdditive.cpp:174, GSTN_AnimNode_BlendOutFootLock.cpp:225 (UpVector), GSTN_DepthSubject.cpp:131 (World GravityZ), minion lightweight :195/:581. ABP GASP (uasset): non leggibile da qui, quasi certamente Z.
8. Sorgente della gravità: non esiste. Oggi solo override del Mover (game thread, non va in rollback) o physics volume (solo Z).
Non verificato: wall slide (non portato), skid (piano corsa §5, non portato).

## Raccomandazione (non implementata)
A. "Up-correct" del sim (punti 1-4, 6 + test harness con gravità inclinata): Opus medium, ~5-6 $.
B. Input/camera nello stesso frame (punto 5): ~3 $, dopo A.
C. Plugin GSTNGravity da spec (campi Galaxy, gravità funzione pura della posizione): milestone, stima grezza 15-25 $, decisione owner.
D. Presentazione/ABP (punto 7): solo quando C esiste; ABP serve test sul fisso.
Solo bassa gravità per zona funziona già quasi tutto: basta il punto 6 (~1 $, dentro A). Nota design: dropkick MaxAirFrames 2 s scade più spesso in bassa gravità.
