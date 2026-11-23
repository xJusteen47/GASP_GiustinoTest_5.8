# Handover loco-base-1 (2026-11-20, fisso, Opus high) - fase 1 rebuild locomotion: base GASP pulita

ESITO: fatto. Base pulita in `/Game/GSTN/Locomotion/StockBase/`, interruttore `gstn.Loco.Base` (0 = attuale, default; 1 = pulita).

1. Sorgenti (nessuna copia 5.9 pristine sul disco; GameAnimationSample e' 5.7, GaspMoverProject e' editato):
   - ABP = `Content/Blueprints/SandboxCharacter_Mover_ABP.uasset` al primo commit f1a280d5 (UE 5.8, LFS oid c4d37422...):
     riferimenti solo a pacchetti del sample, zero GSTN/Wall. Copia del file in `Saved/LocoBase/pristine_src/`.
   - Chooser = `CHT_MoverCharacterAnimations_PoseMatch` e `CHT_PoseSearchDatabases_Relaxed` del progetto: sample non versionato,
     data 2026-08-17, mai toccati. Il 5.8 ABP punta proprio a questi due.
2. Asset nuovi: `ABP_GSTN_StockMover`, `CHT_GSTN_StockMover_PoseMatch` (DB PoseSearch interni copiati con lui),
   `CHT_GSTN_StockMover_PSDRelaxed`. Condivisi (non copiati, come consiglia l'assistente): BPI, CR_, schemi PSS, PSD esterni, clip.
3. Unreal Assistant (Saved/UE59/assistant_locobase.json): AdvancedCopyPackages dei soli 3 package, poi retarget del context class,
   Reference Viewer senza dipendenze verso le versioni vecchie, ricompilare. Fatto cosi'.
4. Tool: `GSTN_ChooserTableToolset::CopyPackagesRemapped` (AdvancedCopyPackages non e' esposto a Python).
   Script: `Tools/GAS/locobase/make_stock_base.py` (copia + retarget, salva via Save-Assets), `verify_stock_base.py` (sola lettura),
   `pie_smoke_stockbase.py` (PIE, env LOCO_BASE).
5. Verifica (log in Saved/LocoBase/, Saved/Logs/ci/):
   - verify_stock_base.txt: ESITO OK; deps proibite 0 su 53/396/79; ABP BS_UP_TO_DATE; 50 + 9 tabelle con context = copia.
   - pie_smoke.txt base 1 (pie_stockbase2.log): i pawn girano ABP_GSTN_StockMover_C, nessun errore chooser/context;
     10 warning "AsyncBuildIndex in progress" al primo avvio (DB interni nuovi che si indicizzano). Base 0: ABP_GSTN_Locomotion_C.
6. Scoperta: in gioco la base "attuale" NON e' SandboxCharacter_Mover_ABP ma ABP_GSTN_Locomotion, messa dallo skin del kit
   (UGSTN_SkinComponent -> SetAnimInstanceClass). Per questo il subsystem ricontrolla ogni frame (UTickableWorldSubsystem) e
   scambia entrambe le classi; a 0 rimette la classe originale di ogni mesh.
7. Limiti: nessun input/movimento provato (solo idle in PIE, nessun video): il confronto visivo e' il prossimo thread.
   La base pulita non ha stomp/hitstop/dash/hit additive/layer nostri (3A: da riportare in fase successiva).
   La cvar e' per processo: in rete ogni processo deve avere lo stesso valore.
8. Divergenze: riga 6 in docs/notes/gasp-divergenze.md (copiata da claude/project-thread-k3vjuh, + decisione 1A).
9. Checkout _ue59: lasciato su claude/loco-base-1 (era claude/anim-setup-3, stesso commit base; file non committati di altri
   thread lasciati intatti). Lock liberato.

COSTO: ~9 $ | 25 min desktop | 0 token review
