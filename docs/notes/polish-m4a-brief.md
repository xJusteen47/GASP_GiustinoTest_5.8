# Brief M4a: regole corsa dall'owner (L3 stadio intermedio, ripartenza dopo skid)
Macchina: CLOUD. Modello: Sonnet medium (Opus medium se tocca la sync state). Budget: 5 $. Base: catena playtest più recente.
Fonte: owner 2026-11-17 12:52Z (polish-movimento-vfx-piano.md §5). Codice: GSTN_RunStageLogic.cpp (boost a :198), GSTN_RunSkidLogic.cpp, GSTN_RunStageTypes.h.
## Obiettivo
1. L3 (boost) porta allo stadio INTERMEDIO: nuovo dato `BoostTargetStage` nel tuning (default stadio 2 = "sprint 1"), clamp ai stadi esistenti. Cambia la regola precedente "L3 = stadio 3"; nessuna costante.
2. Opzione per kit `bResumeStageAfterSkid` (default ON, da confermare): finita l'inversione dello skid si riparte dall'ultimo stadio raggiunto invece che da fermo/cold start; stato serializzato, rollback-safe.
3. Verifica (test): stick LASCIATO = solo ReleaseBrake, mai skid; stick indietro oltre ReverseStickCos = skid. Se oggi non è così, fix.
## Fine
Test di logica pura + NP; handover docs/notes/handovers/polish-m4a.md + copia handoffs/; build/test come passo successivo.
