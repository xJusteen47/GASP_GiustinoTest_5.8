# Valutazione ChiR24/Unreal_mcp (EngineWire) per AnimBP — 2026-11-18

Esito: NON installato. Solo lettura del sorgente (clone depth 1, commit 2792ba1, 10/11/2026). Nessuna modifica a _ue59.

## Cosa sa fare (plugin C++ Domains/AnimationAuthoring, ~1200 righe)
- add_state_machine, add_state, add_transition (from/to, bidirectional, automaticRule), set_transition_rules,
  animazioni in uno stato, add_blend_node, add_slot_node, add_cached_pose, add_layered_blend_per_bone, set_anim_graph_node_value.
- Regola di transizione: UNA sola variabile dell'AnimBP (bool true/false oppure double >,<,>=,<= valore). Niente AND/OR, funzioni, Time Remaining, enum.

## Cosa NON sa fare
- Chooser Table: zero codice (grep "Chooser" vuoto, nessuna dipendenza nel Build.cs).
- Grafi annidati: GetAnimGraphFromBlueprint/FindStateMachineNode cercano solo nel grafo "AnimGraph" top-level.
  In ABP_GSTN_Locomotion lo Skid sta in SkidLayer (layer di ALI_GSTN_Skid) -> state machine "Skid States"
  (stringhe uasset: SkidLayer, Skid States, Skid Loop, Skid Transition). Non raggiungibile senza patch.
- UE 5.9: dichiarato 5.0-5.8 (uplugin 0.6.1-beta-b). Plugin enorme (1490 .cpp, ~50 moduli): build lunga e rischio rotture API.

## Raccomandazione
Non installare. Fallback consigliato: toolset C++ per l'MCP di Epic (skill create-toolset) che riusa (licenza MIT)
la logica di McpAnimBPStateTransitions/TransitionRules, aggiungendo: ricerca state machine in qualsiasi grafo
(layer, sotto-grafi) e regole composte. Fattibilità: alta, ~400-600 righe in un modulo editor; Chooser via API
ChooserEditor da verificare a parte. Non costruito (fuori brief).

Prove: grep sul clone in scratchpad (comandi: grep -rl Chooser plugins; sed McpAnimSupport.cpp 81-140;
strings ABP_GSTN_Locomotion.uasset | grep -i skid).
