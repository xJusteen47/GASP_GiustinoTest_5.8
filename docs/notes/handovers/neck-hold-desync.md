# Neck hold desync + 3 rossi vecchi (2026-11-23, cloud, Opus medium)
OBIETTIVO: causa e fix di `link_spike_client_grabs_host` (22-25 rollback con gstn.Grab.HoldByNeck), poi i 3 rossi vecchi; aggiornare `grab_throw`.
Branch `claude/project-thread-5kb8vz` da `claude/net-grab-test-2026-11-23` c9b28b9a. Log letti in sola lettura dal fisso (`GASP_Giustino2_netgrab/Saved/Logs`), editor NON toccato.

## Lettura del codice (prima delle run)
- Chi legge l'anchor del preso (AnchorOverride): solo FinalizeSnapshot (posa hurtbox del preso), presentazione, watchdog (server) e ShouldReconcile del preso. Nessun passo della sim del grabber lo legge: pushbox esclude le entry piazzate (`bPlacedByContributor`, GSTN_CombatSimSnapshot.cpp:320/335), il capsule del preso resta fermo, il probe interact gira solo su chi è libero.
- Dato nuovo dai log (`ci/netgrab-A-listen.log.ue.log`, marks di client_grabs_host): anche l'OSSERVATORE (client 2, fermo) fa 18 rollback col collo e 0 senza (`ci/netgrab-A4-noneck.log.ue.log`). Quindi è qualcosa che ogni client predice e che dipende dalla posizione del preso ospite, non solo il grabber. Sospetti in ordine: (1) probe interact dei client liberi (GSTN_LinkTickExtension.cpp:1107, tag Interact.Target nel GAS; il commento F10 dice già che non è stabile in predizione), (2) Mover del grabber (il suo ShouldReconcile non traccia), (3) altro.
- I log non dicono il campo: `np.LogReconcileField` non era attivo e il combat traccia solo "Fragments:".

## Fatto (non compilato: nessun editor in cloud)
1. `GSTN_SyncFragment.cpp`: cvar `gstn.Combat.LogFragmentReconcile 1` = logga quale fragment chiede il reconcile, con stato locale e autorità (no Shipping).
2. `Tools/GASNPPTest/reconcile_fields.py`: conta i campi per scenario da un .ue.log (max 5 righe a schermo, tabella in `<log>.fields.txt`). Provato su un log finto.
3. `net_grab_driver.py`: commento con le cvar diagnostiche.
4. `grab_throw` riscritto per Notation: Attack neutro = N. (danno 7), niente pummel; controlla prima che ThrowInputMode sia Notation.
5. Harness: `body/shown_break/watchdog/numbers` ignorano out None (grab_immunity e link_rollback morivano su int(None) e nascondevano il verdetto).

## Run sul fisso (RC, worktree GASP_Giustino2_netgrab, branch + content-reorg-apply 50c8fc56 merge locale, build OK 73 s)
- E-diag (4 scenari, `ci/netgrab-E-diag.log.ue.log`): link_spike_client_grabs_host FAIL con 2 rollback non forzati (era 22); link_rollback_host_grabs_client PASS; grab_immunity FAIL (la presa non si riforma dopo l'immunità, target (0, 2, 0)); grab_throw morto su `throw_input_mode` (enum non BlueprintType) -> corretto in 9e085cd9.
- F-neck x3 (`ci/netgrab-F-neck.log.ue.log`): rollback non forzati nella camminata 29 / 1 / 1, osservatore 26 / 0 / 0, p95 grabber 33.6 / 0 / 0 cm. grab_throw PASS 12. F-noneck x1 (`ci/netgrab-F-noneck.log.ue.log`, HoldByNeck 2): camminata 0 non forzati, osservatore 0 (fallisce solo la presentazione dell'osservatore, p95 67 cm, come in tutte le run F).
- Campi (reconcile_fields.py): `Effect.PeriodTimeMS` 333-515 righe e `History:` ~10 in OGNI scenario, anche nei PASS: rumore GAS, non sono i rollback contati. Fragment combat: solo 2 righe per run (la correzione di fine link, attesa). Nessun campo combat tracciato spiega i rollback della camminata: per esclusione è il Mover del grabber (il suo ShouldReconcile, tolleranza 5 cm, non traccia).
- Conclusione: col collo il Mover del grabber diverge a tratti (1-29 rollback per run, non deterministico); senza collo no. Correzione al primo indizio: il "tipo 2" è il grab (nella riga del fragment il grabber ha type 2 role 1), non un latch.
- Prossimo passo (thread nuovo, regola 5: qui fatti 2 run): log di riconciliazione Mover sotto cvar (delta posizione e base) e una run x3; ipotesi da provare per prima: qualcosa di presentazione del preso (mesh attaccata alla mano viva, a 60 cm col collo) che entra nelle query di movimento del grabber.

## Rossi vecchi
- grab_immunity resta rosso (nuovo dato sopra). link_rollback_host_grabs_client PASS in E-diag: probabilmente instabile. Bisezione dal G12b (2026-11-09) ancora da fare.
Testato: build OK, run E-diag e F sul fisso (sopra). Non fatto: fix del desync, bisezione. INDICE-PROGETTO.md non è su questo branch: voce da aggiornare quando arriva in main.
SUBAGENTI: 0
COSTO: ~9 $ | ~30 min desktop (build 1 min, 3 batch 9 min) | 0 token review
