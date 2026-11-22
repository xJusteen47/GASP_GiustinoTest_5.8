# Toolset MCP per AnimBP e Chooser (GSTNAnimToolsets) — 2026-11-18

Esito: FATTO. Nuovo plugin editor `Plugins/GSTNAnimToolsets` (modulo `GSTNAnimToolsets`), registrato nel ToolsetRegistry: compare in
`list_toolsets` come `GSTNAnimToolsets.GSTN_AnimBlueprintToolset` e `GSTNAnimToolsets.GSTN_ChooserTableToolset`. Dettagli: README del plugin.

## Cosa sa fare
- AnimBP: ListAnimGraphs (tutti i grafi con tipo e percorso, layer compresi), FindStateMachines (ovunque), GetStateMachine (stati, animazioni,
  entry, transizioni con regola leggibile), AddStateMachine, AddState/Conduit, FindState, RenameState, RemoveState, SetEntryState,
  SetStateAnimation, AddTransition/GetTransition/RemoveTransition, SetTransitionRule (OR di AND: variabili, funzioni pure con argomenti,
  getter di state machine come Time Remaining (ratio), confronti bool/numeri/enum/name/string, regola automatica, conduit), AddAnimNode.
  Pin, collegamenti e compile restano a BlueprintToolset di Epic; crossfade/priority con ObjectToolset.SetProperties (provato).
- Chooser: wrapper tipizzato di `UChooserTableEditorLibrary` (5.9): GetChooserTable in una chiamata, colonne (add/remove/move/input binding/
  proprietà), righe (add/remove/move/disable/result, fallback = riga -1), celle. L'errore riporta il warning della libreria.
- Logica transizioni ispirata a EngineWire (MIT, attribuzione in ThirdPartyNotices.md), estesa a qualsiasi profondità e regole composte.

## Prove
- Build: `Tools\CI\Run-Timed.ps1 -Preset Build -Log Saved\Logs\ci\build_animtoolsets5.log` → ESITO OK.
- Test: `Run-Timed.ps1 -Preset Test -Log Saved\Logs\ci\test_animtoolsets5.log -Tests GSTN.Toolsets` → ESITO OK, 35/35
  (`grep -c "Result={Success}" Saved/Logs/ci/test_animtoolsets5.log` = 35, Fail = 0).
- Demo live via MCP (editor sul desktop 2, script in scratchpad mcp_core.py/demo2-4.py) su copie scratch `/Game/ScratchToolsets/`
  (non versionato): in SkidLayer di ABP_ToolsetDemo creati "Skid States" (Skid Loop → SS_180_Turn2, Skid Exit → SS_180_Turn_Root180),
  regola `((Time Remaining (ratio) (SS_180_Turn2) < 0.15) || (!IsMoving() && (Current State Time > 0.2)))`, ritorno automatico,
  macchina collegata all'Output Pose, compile senza errori, asset salvato. Funzione impura rifiutata prima di toccare il grafo.
  CHT_ToolsetDemo: letto (1 colonna Name, 10 righe, 49 nested), riga aggiunta/disabilitata/rimossa, cella invalida → errore chiaro.

## Da sapere (da portare all'owner)
- L'ABP_GSTN_Locomotion salvato (HEAD e02924b6) ha SkidLayer con il solo Output Pose: niente "Skid States" né funzioni Is Skidding /
  Get Skid Clip Time. Le stringhe nel .uasset sono residui. Il cablaggio Skid dell'owner non è salvato in questo checkout.
- Decisione applicata: plugin separato (non dentro GSTNAnimation) per non far dipendere un plugin runtime dal ToolsetRegistry sperimentale.
- Originali GASP e asset dell'owner non toccati; i test lavorano in sandbox /Automation, la copia Skid è in sandbox.
