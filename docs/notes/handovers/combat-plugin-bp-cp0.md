# COMBAT-PLUGIN-BP CP0 handover (cloud, solo piano, 2026-11-14)
Branch `claude/project-thread-qfjwsu` da `d27c77f5` (wkds3r). Nessun codice, nessuna build, desktop non usato.
**Fatto**
- Piano: `docs/notes/handovers/combat-plugin-bp-plan.md` (1 pagina): ganci BP di simulazione su `UGSTN_GA_BaseCombat`,
  azioni "richiesta" (proiettile, evento, movimento, finestra tag), query input pure, validatore, ganci visivi, factory "Combat Move";
  cosa resta C++; special a doppia pressione (variante A dati come jab1/jab2, variante B grafo BP); CP1-CP5 ~$95-140.
- Letti: doc architettura (`Artifacts/Combattimento`, ramo 0tmywc-docs), header `GSTN_GA_Base/BaseCombat`, `GSTN_GA_FireProjectile`,
  `GSTNProjectile/README`, tracce timeline, README fork AbilitySystemSimulation (BP + FSync ok), `setup_overlord_jab_kit.py` (catena).
**In attesa**: l'owner approva il piano e sceglie A oppure A+B. Nessun lavoro finché il coordinatore non conferma.
**Prossimo thread (CP1)**: Opus high; parte da questo file + il piano; legge solo `GSTN_GA_BaseCombat.h/.cpp`, `GSTN_GA_Base.h`,
  la simulazione input per le query, `GSTNCombatEditor` validatori. Build e test su una macchina libera.
**Rischi**: grafi BP generati via UE_MCP_Bridge richiedono l'editor vivo; BP per frame costoso (opt-in); nuova chiave timeline tocca l'editor timeline.
**Non verificato**: che la variante A funzioni già oggi con chiave Event → `GA_FireProjectile` attivata da evento.
**Costo**: cloud Opus, ~70k contesto, nessun subagente; stima ~$4.
