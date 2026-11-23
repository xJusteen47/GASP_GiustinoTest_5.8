# Gravità A: sim corretto a gravità inclinata (2026-11-16, cloud, NON compilato)
Branch claude/project-thread-wz5g4d (merge di claude/project-thread-4n8gx9 8d4b21e5 + audit gravita-audit-1.md). Owner 18:40 "A), mmm... B) penso": coordinatore = A ora, B (input/camera) col plugin dei volumi.
Regola: con gravità sul Z del mondo ogni percorso usa il vecchio codice identico (fast path IsWorldUp), quindi il gioco attuale non deve cambiare.

## Fatto
- Nuovo `GSTNCombatCore/Movement/GSTN_GravityFrame.h/.cpp`: yaw attorno all'Up locale nel frame di Mover (FindBetweenNormals(Z, Up)), WithYaw, UprightRotation, Flatten, FollowGravity (dato scritto a 980 -> gravità dell'entità).
- Facing: GSTN_SetFacingEffect (MakeOrientation con Up), steer di startup (CurrentYaw), HoldOrientationIntent + ProduceInput/GetSteerView (yaw e intent sull'Up dell'attore), girata del colpito (ResolveHitFacing, TurnToAttacker senza `.Z = 0`), girata indietro del knockback (ExecuteReactionMove).
- Capsula: shift di GSTN_BodyCapsuleShiftEffect (air crouch, capsula d'atterraggio) e centro pushbox/core nello snapshot nel frame upright.
- 980: GSTN_TumbleLanding mini rimbalzo usa la gravità base (UGSTN_CombatSimComponent::GetBaseGravity, nuovo: base senza fasi di salto), tetto 10 s a gravità zero; lanci held/nemici (UGSTN_GA_HeldThrow::FollowThrowerGravity).
- Test: `GSTNCombat.Core.GravityFrame.*` (WorldUpUnchanged, Tilted: parete/soffitto/45°/pianeta, FollowGravity) + muro in `GSTNCombat.NP.Weakpoint.SetFacingEffect`.

## Non fatto (motivo)
- Dodge a terra: GroundDodgeParams usa NominalGravity 980 nei dati di default della classe (GSTN_GA_Dash.cpp:174); in bassa gravità la timeline può finire prima dell'atterraggio. Serve una timeline per gravità o fine "on land" a runtime: thread a parte (~2 $).
- B input/camera (CameraYaw dal ControlRotation, classifier con facing): rimandato dall'owner.
- AttackerYaw nelle entry di hit resta yaw del mondo (solo fallback, appiattito sul piano della vittima).
- Presentazione/ABP, minion lightweight, depth marker: invariati.

## Fisso: build e controlli
1. _ue59 libero (IN-USO.txt), merge del branch, build col solito script a tempo (45 min).
2. Test: `GSTNCombat.Core.GravityFrame`, `GSTNCombat.NP.Weakpoint.SetFacingEffect`, poi regressione `GSTNCombat.Core` + `GSTNCombat.NP` + `GSTNDefence` + `GSTNHeld`: nessun test esistente deve cambiare (fast path).
3. PIE L_Test_Hub a gravità normale: steer, girata da colpito, air crouch, rimbalzo knockdown, lancio oggetto uguali a prima.
4. Facoltativo: harness con SetTestGravity laterale, colpire il dummy: si gira sul muro senza ribaltarsi.
Rischio compilazione: firme con parametro Up di default (MakeOrientation, HoldOrientationIntent), nuovo metodo FollowThrowerGravity nell'header held. Costo stimato thread: ~5 $.
