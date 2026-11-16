# Brief: corsa Mario P1 (profilo, sync state, 3 stadi, L3)

- **Macchina**: cloud per il codice; build e test sul fisso solo alla fine (Haiku, script a tempo). Non aggiorna `_ue59`.
- **Modello**: Opus medium. **Budget**: 8 $ (+20% coperto dal piano approvato).
- **Base**: branch nuovo `claude/*` da `feature/gas-npp-5.8` (d258c41c o successivo). Piano: `docs/notes/corsa-mario-piano.md` (branch `claude/project-thread-r78ylj`), decisioni owner 1C 2A 3C.

## Da fare
1. `UGSTN_LocomotionProfile` (DataAsset) referenziato dal kit: tabella 3 stadi (MaxSpeed, Acceleration, TurningStrength, ReleaseBrake cm/s², FramesToNextStage, soglia di velocità), finestra L3 (frame) + modalità `InitialWindow | Anytime` (default InitialWindow), finestra K per stadio (decisione 2A, solo i campi). Velocità scalate dalla stat WalkSpeed come oggi.
2. Struct di stato nello sync state di Mover: Stage, FramesInStage, FramesSinceMoveStart, FramesSinceStickRelease, PreReleaseSpeed. Rollback-safe, NetSerialize, niente stato sul componente.
3. Logica stadi nel walking mode (modo C++ o modifier che ASSEGNA i valori come `FGSTN_StatsMovementModifier`; scegli e scrivi perché). Sale dopo N frame sopra soglia con stick avanti; scende con svolta stretta, stick rilasciato oltre N frame, urto.
4. Comando `Command.Movement.RunBoost` su tag bottone nuovo `Input.Button.RunBoost` in `Config/Tags/GSTNCommands.ini`; L3 solo nei binding di default del kit (dati). Niente tasti nel codice, niente Project Settings.
5. Test headless: salita stadi, L3 dentro/fuori finestra, rollback che ripristina lo stadio.

## Fuori ambito (P2/P3)
Aria, skid, hook tracce, momento negli attacchi e ritirata.

## Regole
Max 2 tentativi di fix; build 45 min, test 30 con lo script a tempo. Handover ≤30 righe in `docs/notes/handovers/corsa-mario-p1.md` con numeri citati da log e comando.
