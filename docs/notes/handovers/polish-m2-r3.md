# Handover polish M2 r3: kit VFX, giro 3 (ultimo) (2026-11-17, portatile + editor UE 5.9, Opus)
Branch `claude/project-thread-6web8v` da `claude/project-thread-lrogpg` 69ddff1e + merge di `claude/project-thread-cocsel` 3fd73243 (rimbalzi). Review Fable 5.2 r3: **FAIL di misura** (`docs/notes/reviews/polish-m2/review-fable-r3.md`). Era l'ultimo giro: decide l'owner.
## Cause trovate (non viste nei giri 1-2)
1. Il drag non ha mai agito: `FindModule` (VfxAuthoringLibrary) seguiva la catena in avanti e non trovava Drag/GravityForce del template, quindi `set Drag` falliva (errori r3 `no module Drag`, in r2 nascosti dal Drag aggiunto in coda, dopo il solver). Ora appartenenza allo stack dalla catena all'indietro, forze inserite prima di SolveForcesAndVelocity.
2. Spawn Probability legata a un parametro utente non filtrava mai: sempre 8 schegge e le 3 stelle rosse Accent su ogni colpo (era il "rosso su S"). Ora conteggi int dal cue (`CountMax`/`StrongCount`, `User.Count`/`User.StrongCount`).
3. Colori: misurati a schermo con `NS_GSTN_Swatch` (`PIE_SWATCH=1`, `vfx_swatch.py`, `r3/swatch.json`); valori calibrati in `setup_presentation_vfx_materials.py`.
## Fatto
Ventaglio schegge 3000 cm/s drag 14 (~±45°), partenza corsa in 2 coppie a 2 velocità, skid e scia per distanza (37 / 60 cm), anello a terra spesso 18 frame, knockdown che aspetta il pelvis disegnato (cue `GroundedBone`), ombra atlas 28 %, sonda particelle in PIE (`PIE_PROBE=1`, `DescribeParticles`). Fix C4459 nel test del ramo rimbalzi.
## Prove (regola 11)
- Build `Saved/Logs/ci/m2r3-build-p2.log` ESITO OK. Test `Saved/Logs/ci/m2r3-test1.log.ue.log` (GSTNPresentation + GSTNCombat.NP.StateEvents/Hits/Impact): 95 `Result={Success}`, 0 Fail (`grep -c`), prima dei conteggi int (solo presentazione cambiata dopo).
- Setup `Saved/Logs/ci/m2r3-setup.txt`: 9 sistemi, 0 errori. Sonda `Saved/PolishM2/r3pr2/vfx1/capture.log` (`grep "probe f"`): colpo a 0,56 n=4, Accent n=0, schegge ~65 cm a f3 in scala 1.
- Catture ON `Saved/PolishM2/r3{a2,b2}/vfx1`, OFF `.../vfx0`, 16 griglie off/on in `docs/notes/reviews/polish-m2/r3/`, eventi `events_r3*.txt`.
## Esito Fable (dettagli nella review)
PASS: partenza corsa, skid, salto, atterraggio, sprint, colpo S/M/L, scia tumble; splat e rimbalzo muro PASS con nota. FAIL lievi di forma: palline di knockdown, rimbalzo terra e soffitto (a grumo sopra il corpo 6-10 frame). Bump muro non giudicabile (evento a sim 2969, fuori dalla sequenza di 110 frame). Scostamenti dal builder: stelle visibili già sul frame del lampo (non disturbano), ombra a schermo ~10-15 %.
## Da decidere (owner) e coda
Decisioni proposte da Fable: accettare il nucleo; impatti così per la v1 o un giro mirato sulle palline; taglia/spessore anello atterraggio (~190 cm, più grande dell'anello del colpo L); bump 0,4 da confermare dopo una ricattura (sequenza wall_bump più lunga). Coda codice (gioco): rimbalzi muro doppi a 6 frame, bump e WallBounce nello stesso frame, soffitto segnalato come WallBounce.
Costo stimato thread: ~45 $ Opus + review Fable ~8 $.
