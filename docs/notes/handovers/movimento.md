# Handover MOV-1: salto, rimbalzo, pad Xbox, slide (portatile, 2026-11-06)

- Ramo `claude/project-thread-kizkcl` (da 63f8450). Build ok (Saved/MOV/build-1.out, build-2.out); test GSTNCombat.NP+GSTNSurface+GSTNLevelObjects 163/0.
- **Esisteva già (nessuna modifica)**: salto ad altezza variabile (rilascio anticipato = salto corto: `MinJumpHoldFrames`/`MaxJumpHoldFrames`,
  `JumpCutGravityScale`), fast fall (`UGSTN_GA_PlatformFastFall`, giù in aria), gravità diversa salita/discesa (`JumpRiseGravityScale`,
  `FallGravityScale`, `FastFallGravityScale`, `MaxFallSpeed`, `FastFallSpeed`), velocità a terra/aria (`WalkSpeed`, `Acceleration`, `AirControl`),
  salti in aria. Tutto in `UGSTN_CombatAttributeSet`; valori iniziali in Project Settings > GSTN Combat GAS > Attribute Defaults; per kit sul
  Blueprint dell'abilità di salto (coyote, squat). Funziona col salto del kit (`Input.Button.Jump`), non con `GA_Jump` di GASP.
- **Nuovo, rimbalzo più alto tenendo salto**: `FGSTN_SurfaceLaunchParams::HeldJumpSpeedScale` (default 1.4) su ogni superficie che lancia;
  la sim legge il tasto salto dall'input sincronizzato (`StepSurfaceLaunch`), quindi è deterministico. Test `...Launch.FromSurfaceParams` esteso.
- **Pedana placeholder**: `/Game/GASNPPTest/BP_Test_BouncePad` (figlio di GSTN Trampoline; Class Defaults > Launch: velocità 1300, x1.5 tenendo
  salto, 250x250x40). `test_hub.spawn_bounce_pads()` la mette al Play in piazza (-700, 0) in ogni mondo (nuova
  `SpawnTestTrampolineOfClass`); nessun salvataggio mappa. Non provata in Play.
- **Pad Xbox** (asset salvati da `Tools/GASNPPTest/movement_setup.py`, già eseguito una volta; Giustino non deve lanciarlo):

  | Pad | Azione | Tastiera |
  |---|---|---|
  | X | leggero (IA_GASNPP_Attack) | tasto sx mouse |
  | A | salto del kit (IA_GASNPP_Jump) | Y (spazio resta il salto GASP) |
  | Y | speciale | tasto centrale mouse |
  | B | grab = interagisci (tocco prende, tieni = raccogli/cambia kit) | O |
  | RT | forte (IA_GASNPP_Strong, nuovo) | K |
  | LT | scudo (IA_GASNPP_Shield, nuovo) | I |
  | LB | schivata (IA_GASNPP_Dodge): LT fa solo scudo, sullo stesso grilletto partirebbero tutte e due | U |
  | RB | accovacciati (IA_Crouch) | C |

  IMC_GASNPP_Test ha priorità 5 > IMC_Sandbox, quindi copre i tasti pad di GASP (X ragdoll, RT takedown, LT mira...). Tolto A da IA_Jump in
  IMC_GAS_Abilities (avrebbe fatto partire due salti). IC_GASNPP_Test: aggiunti i bottoni Strong (bit 9) e Shield (bit 10) in coda.
  Il kit di test non ha ancora mosse sul forte: il tasto c'è, le mosse arrivano col thread delle mosse Blueprint.
  NOTA: l'IMC committato aveva solo Dash/Buff (1, 2): i tasti tastiera del vecchio handover non erano salvati, ora sì.
- **Slide: NON fatto (2 tentativi falliti)**. Lo slide GASP è una transizione Mover (`BP_MovementTransition_ToSlide` sul modo Walking) del
  componente ereditato da SandboxCharacter_Mover; toglierla dal solo BP_GASNPP_TestCharacter via Python non viene salvato (l'override del
  componente ereditato si perde). A mano sono 30 secondi: BP_GASNPP_TestCharacter > CharacterMover > Movement Modes > Walking > Transitions,
  togli ToSlide, Compile, Save. Senza slide, accovacciarsi correndo resta accovacciati (il "si rialza" sparisce). Un vero GA_Slide (abilità NP
  con layered move, resta accovacciato a fine slide) è C++ deterministico: thread dedicato Opus, ~$5-8.
- Costo stimato: ~120k token, ~$9-10.
