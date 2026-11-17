# Brief M5: rimbalzo di default su muri e soffitti
Macchina: CLOUD. Modello: Opus high. Budget: 10 $. _ue59: NO. Base: claude/playtest-fixes-1117 (o più recente della catena playtest).
Piano: handoffs/polish-movimento-vfx-piano.md §3. Modello di riferimento: handoffs/ground-bounce-1.md (GroundImpact + tech bounce).
## Regola owner (2026-11-17 12:30Z, ambito: vittima che tocca muro o soffitto)
Il rimbalzo su muri/soffitti c'è DI DEFAULT; se l'hitstun è poco, rimbalzo piccolo piccolo con un'animazione valida per muri e soffitti. Splat (tabella superfici / Force Splat) e rimbalzo forzato dalla hitbox (Force Bounce) sono altre cose e vincono sul default.
## Scelte applicate (da confermare)
- Solo in hitstun/tumble; fuori hitstun collisione normale.
- Ampiezza scalata dall'HITSTUN RIMASTO (min in cm/velocità > 0 se in hitstun, max); alternativa per velocità d'impatto come impostazione. Dati in CombatRules > Wall Impact (FGSTN_WallImpactRules: restituzione, min/max, MaxBounces, finestra), stile GroundImpact. Override per mossa nel profilo Impact (bit dove serve, rispettare il packing esistente).
- Evento di presentazione WallBounce (normale della superficie) per M0; GA/clip: una sola clip orientata alla normale (placeholder finché M6).
## Test
Logica pura (scala, limiti, precedenza splat/forced) + NP (rimbalzo su muro e soffitto, nessun rimbalzo fuori hitstun, server = client).
## Fine
Handover docs/notes/handovers/polish-m5.md + copia handoffs/. Non compilato in cloud: build/test/PIE come passo successivo.
