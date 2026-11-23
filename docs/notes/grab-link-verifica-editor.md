# grab-link: verifiche sul fisso e animazioni da fare (2026-11-23)

Branch `claude/grab-link`, NON compilato (cloud). Da integrare nel branch di integrazione e compilare sul PC (regola 15h).

## Build e test (subagente Haiku, script a tempo)
1. `Tools/CI/Run-Timed.ps1 -Preset Build -WaitMutex -NoHotReload` (45 min max).
2. Test: `GSTNLink.Core.KitThrows`, `GSTNLink.Core.GrabHold`, `GSTNLink.Core.Tags`, `GSTNLink.NP`, `GSTNExtTest.Link`, `GSTNHeld.Core.KitThrowBlockers`; parsing con `Tools/CI/Check-TestLog.ps1`.
3. Contenuto: `Tools/GAS/setup_defence_link_content.py` (idempotente) mette `UGSTN_GE_GrabHold` come LinkEffect del ruolo Grabber di `/GSTNLink/Test/DA_LinkType_Grab_Test`; poi `Tools/GAS/validate_defence_content.py`. Editor chiuso dopo se non l'ha aperto l'owner.
4. Harness: `SUITE_LINK` e `kit_scenarios`. ATTENZIONE: `scenario_kit_throw_stick_*` presuppongono `ThrowInputMode=Stick`; con il nuovo default `Notation` vanno lanciati con Stick in memoria o aggiornati (voce di coda codice, non un fallimento della presa).

## Da guardare in PIE (KIT_Test_Overlord contro manichino, L_Test_Hub)
1. Presa (tap Interact): il collo del manichino sta circa sulla mano destra davanti, sollevato (`gstn.Combat.Debug.Links 1` mostra la capsula parcheggiata). Se il manichino non ha `neck_01` nel mesh, la presa resta all'offset del tipo (90 cm davanti).
2. Camminata con lo stick: si cammina più lenti (0.75); A salta più basso (0.7); il nemico segue. Crouch: nessun cambio di animazione (voluto).
3. X neutro = N., X + stick in qualunque direzione = F. con girata istantanea verso lo stick, LT + X = Cr., LT + stick + X = Cr.F.. Niente pummel (tolto dall'owner): RT neutro non fa nulla di link.
4. RB (schivata) = mette giù senza lancio. DA CONFERMARE con l'owner che il tasto è giusto.
5. Senza mash il nemico si libera entro MaxFrames del kit; `gstn.Grab.MaxHoldFrames 300` lo forza a 4,8 s. `gstn.Grab.EscapeHitForAll 1`: alla liberazione il grabber prende il colpo (log `GrabEscapeHit`).
6. Menu GSTN Debug: voci `Grab.*` presenti, un controllo ciascuna (TurnMode e HoldByNeck come numero, EscapeHitForAll come casella).
7. `gstn.Grab.TurnMode 2` + cambio kit: X + stick indietro = B. (classe di fallback Back).

## Animazioni necessarie (brief delle pose, NON prodotte qui)
Notazione di progetto; tratti d'identità da confermare dall'owner nel brief di ogni posa.
1. Grab (presa): Overlord afferra per il collo con una mano (stile Ganondorf).
2. Grab idle (hold): braccio teso davanti, nemico sollevato per il collo; il punto mano = `HoldPointCm` (60, 15, 55) finché la posa non lo sposta.
3. Hold walk (camminata con presa, layer braccio sopra la locomotion).
4. Hold jump (salto più basso con presa).
5. Put down (mettere giù senza lancio, su RB).
6. N. throw.
7. F. throw: ricalca il F throw di Zetterburn (Rivals of Aether 2): entrambe le mani per un momento, poi testata. Aspetta le reference dell'owner.
8. Cr. throw.
9. Cr.F. throw.
10. Liberazione: reazione del grabber (se colpito, PVE) e del nemico che si libera.
11. Nemico tenuto per il collo (posa del bersaglio parcheggiato: grabbed idle) e la sua uscita.
Frame dei lanci: oggi placeholder (Forward/Up/Down del set M11 K2); i veri da Smash Ultimate (Ganondorf) con le pose. Hitbox/hurtbox: le imposta l'owner.

## Limiti noti
- Il nemico è posizionato dalle ossa del reference pose (deterministico), non dalla mano animata: la posa deve portare la mano sul punto (regola 13e). Durante un lancio con il nemico che segue la mano (F. a due mani) servirà un'àncora per-frame o una partner move: decidere con le pose.
- Primo frame della presa: un frame all'offset del tipo prima dell'àncora al collo.
- Il colpo di liberazione arriva dal server (come la liberazione stessa): sul client del grabber arriva per correzione.
