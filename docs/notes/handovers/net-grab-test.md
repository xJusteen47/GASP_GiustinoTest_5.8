# Test di rete grab-link sotto rollback (2026-11-23, fisso, Opus medium)
OBIETTIVO: presa/lanci di grab-link in sync tra server e 2 client (predizione locale, avversario interpolato). Branch `claude/net-grab-test-2026-11-23` da integ e0fcf520.
Worktree `GASP_Giustino2_netgrab` (junction del contenuto GASP verso _ue59, ignorate da git). Editor -nullrhi, PIE un processo, net(60, 15, 0), una PIE fresca per scenario.
Driver nuovo `Tools/GASNPPTest/net_grab_driver.py` = batch_driver + `UGSTN_GE_GrabHold` come LinkEffect del Grabber IN MEMORIA (contenuto non salvato: Save-Assets gira solo da _ue59) + `NETGRAB_CVARS`.
Report: `Saved/Logs/netgrab/batch-<tag>.log`, log UE `Saved/Logs/ci/netgrab-<tag>.log.ue.log`, esiti `*.esito` (tutti ESITO: OK, editor chiuso dallo script).

## Risultati (conteggi = righe PASS/FAIL dei report)
- A listen (link_spike/rollback/break_distance x3 pairing): 6/9. B listen (grab_immunity, grab_throw, grab_mash, grab_interact, grab_gate, throw_at_enemy): 4/6. C dedicated (client_grabs_client + grab_*): 6/7.
- Rossi riprovati (A2 = stessi 3 rossi). Controlli sui 3 rossi: A3 senza effetto hold = stessi 3 rossi; A4 con `gstn.Grab.HoldByNeck 2` = `link_spike_client_grabs_host` PASS.
- Baseline D sul commit prima del merge grab-link (1b0c59bd, rebuild): `link_spike_client_grabs_host` PASS; `link_spike_host_grabs_client`, `link_rollback_host_grabs_client`, `grab_immunity` FAIL come su integ.

## Conclusione
1. REGRESSIONE grab-link (presa al collo): `link_spike_client_grabs_host` FAIL 3/3 con il collo (22-25 rollback non forzati sul client che prende, 18-22 correzioni sull'osservatore, p95 presentazione 13-15 cm > 10), PASS senza collo e su baseline. Sospetto: `FGSTN_LinkTickExtension::StepNeckHold` (GSTN_LinkTickExtension.cpp:298) scrive l'anchor sul preso dallo snapshot del passo; dove il preso è il pawn dell'host simulato sul client lo snapshot/partner o il frame differiscono. Voce di coda codice (Opus), poi rerun di `link_spike_*` x3.
2. PRE-ESISTENTI (non grab-link, rossi anche su 1b0c59bd): host_grabs_client spike (1-4 rollback, ~20 correzioni osservatore), link_rollback_host_grabs_client (lato bersaglio del link None, preso resta Walking), grab_immunity (stesso sintomo alla prima presa). Ultimo verde noto: m8 SUITE_LINK 18/19. Voce di coda separata (bisezione tra m8 e main).
3. `grab_throw` FAIL listen e dedicated = harness non adeguato: Attack ora lancia N. (pummel tolto); il danno è 7 uguale su tutti i mondi, quindi in sync. Da aggiornare con `kit_throw_stick_*`.
4. In sync e verdi: mash, interact, gate, throw_at_enemy, break_distance x3, rollback client, tutti i client_grabs_client anche in dedicated.

## Non fatto
2 % di perdita, standalone host, kit_scenarios. Salvataggio di `DA_LinkType_Grab_Test` con l'effetto hold (setup_defence_link_content.py via Save-Assets da _ue59). _ue59 non toccata, lock liberato.

SUBAGENTI: 0
COSTO: ~5 $ | ~55 min desktop (build 20+1+1 min, 7 batch 30 min) | 0 token review
