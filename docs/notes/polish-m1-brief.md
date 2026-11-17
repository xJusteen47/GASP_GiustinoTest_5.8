# Brief M1: import asset per il polish (skid, 180, wall slide) + diagnosi ossa reazioni
Macchina: FISSO (editor 5.8/5.9 libero; GaspMoverProject 5.7 solo in lettura, MAI salvato). Modello: Sonnet medium; review ABP in cloud Opus medium. Budget: 8 $. _ue59: segnare IN-USO.txt se serve.
Piani: handoffs/run-start-skid-import-piano.md R3 + R4, anim-layer-skid-wallslide-piano.md L0, polish-movimento-vfx-piano.md M1.
## Obiettivo
1. R3: copie (originali intatti) in Content/Blueprints/Animation/Ported57/: Skid/{Skid_Revised, Driving_Skid_Revised, SS_180_Turn x3}, WallSlide/{WallSlideIdle, Driving_WallSlideIdle, Left/RightPose, CR_WallIkControlRig (quello usato da ABP_WallSliding)}. Scheletro diverso → IK Retargeter, mai modificare le pose. Root motion off, curva Distance solo sulle copie skid. Commit LFS su branch claude/*.
2. R4: export testuale di ABP_WallSliding (e simili) → tabella tieni/scarta/perché (thread cloud o subagente).
3. Diagnosi clip reazione base (ossa staccate/stirate oltre Manny): retarget sbagliato? Griglia prima/dopo di una clip, una vista. Fix solo se è un retarget ovvio, altrimenti scrivere cosa serve (va in M6).
## Vincoli
Niente mappe salvate; editor startup sporca PM_BoostJump & co. → git restore. Regola 4 output corto.
## Fine
Handover docs/notes/handovers/polish-m1.md + copia handoffs/.
