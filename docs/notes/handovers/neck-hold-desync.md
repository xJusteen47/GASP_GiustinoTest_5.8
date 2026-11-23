# Neck hold desync + 3 rossi vecchi (2026-11-23, cloud, Opus medium)
OBIETTIVO: causa e fix di `link_spike_client_grabs_host` (22-25 rollback con gstn.Grab.HoldByNeck), poi i 3 rossi vecchi; aggiornare `grab_throw`.
Branch `claude/project-thread-5kb8vz` da `claude/net-grab-test-2026-11-23` c9b28b9a. Log letti in sola lettura dal fisso (`GASP_Giustino2_netgrab/Saved/Logs`), editor NON toccato.

## Esito: causa NON trovata leggendo il codice, rerun diagnostico pronto (nessun fix C++ indovinato, regola 5)
- Chi legge l'anchor del preso (AnchorOverride): solo FinalizeSnapshot (posa hurtbox del preso), presentazione, watchdog (server) e ShouldReconcile del preso. Nessun passo della sim del grabber lo legge: pushbox esclude le entry piazzate (`bPlacedByContributor`, GSTN_CombatSimSnapshot.cpp:320/335), il capsule del preso resta fermo, il probe interact gira solo su chi è libero.
- Dato nuovo dai log (`ci/netgrab-A-listen.log.ue.log`, marks di client_grabs_host): anche l'OSSERVATORE (client 2, fermo) fa 18 rollback col collo e 0 senza (`ci/netgrab-A4-noneck.log.ue.log`). Quindi è qualcosa che ogni client predice e che dipende dalla posizione del preso ospite, non solo il grabber. Sospetti in ordine: (1) probe interact dei client liberi (GSTN_LinkTickExtension.cpp:1107, tag Interact.Target nel GAS; il commento F10 dice già che non è stabile in predizione), (2) Mover del grabber (il suo ShouldReconcile non traccia), (3) altro.
- I log non dicono il campo: `np.LogReconcileField` non era attivo e il combat traccia solo "Fragments:".

## Fatto (non compilato: nessun editor in cloud)
1. `GSTN_SyncFragment.cpp`: cvar `gstn.Combat.LogFragmentReconcile 1` = logga quale fragment chiede il reconcile, con stato locale e autorità (no Shipping).
2. `Tools/GASNPPTest/reconcile_fields.py`: conta i campi per scenario da un .ue.log (max 5 righe a schermo, tabella in `<log>.fields.txt`). Provato su un log finto.
3. `net_grab_driver.py`: commento con le cvar diagnostiche.
4. `grab_throw` riscritto per Notation: Attack neutro = N. (danno 7), niente pummel; controlla prima che ThrowInputMode sia Notation.
5. Harness: `body/shown_break/watchdog/numbers` ignorano out None (grab_immunity e link_rollback morivano su int(None) e nascondevano il verdetto).

## Rossi vecchi (rossi anche su 1b0c59bd): primo indizio
- `link_rollback_host_grabs_client`: il log mostra `Modifier(GSTN Linked) started on BP_GASNPP_TestCharacter_C_0` (l'host, cioè chi attacca) e `Link break ... entity 3 and 6, type 2`: il colpo forma un tipo di link DIVERSO dal grab (indice 2, ruolo parcheggiato = iniziatore, tipo latch) tramite le Alternatives. Probabile stessa causa in grab_immunity (prima presa non è un grab). Da confermare con `GetShownLinkText` sul desktop.
- Bisezione: la storia qui è shallow; ultimo verde noto = G12b 2026-11-09 (SUITE_LINK 18/19, net 60/15). Sul fisso: `git bisect` tra un commit main del 09-11 e 1b0c59bd, solo `link_rollback_host_grabs_client` e `grab_immunity`, harness di ogni commit.

## Per il fisso (DOPO il riordino contenuti, lock _ue59/IN-USO.txt; ~15 min, Haiku/Sonnet)
1. Build di questo branch (Run-Timed -Preset Build). 2. `NETGRAB_CVARS='np.LogReconcileField 1;gstn.Combat.LogFragmentReconcile 1'`, batch con `link_spike_client_grabs_host` + `link_rollback_host_grabs_client` + `grab_immunity` + `grab_throw` (listen). 3. `python Tools/GASNPPTest/reconcile_fields.py <ue.log> link_spike` e incollare le 5 righe al thread di codice. Poi fix (Opus, ~3 $) e rerun `link_spike_*` x3.

Non testato: tutto (C++ non compilato, harness non eseguito). INDICE-PROGETTO.md non è su questo branch: voce da aggiornare quando arriva in main.
SUBAGENTI: 0
COSTO: ~6 $ | 0 min desktop | 0 token review
