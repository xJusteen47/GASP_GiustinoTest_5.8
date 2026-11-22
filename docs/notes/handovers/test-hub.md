# Handover TH-2: crash del Play, kit a terra, cubo per kit (portatile, 2026-11-06)

- Branch `claude/project-thread-kizkcl` = `claude/project-thread-f6jper` (TH-1 + M7) + merge di `claude/project-thread-0t6tfg` (TH-1b) + questo lavoro.
  I commit SD locali di `feature` (SD-11..16) NON sono qui. TH-1 precedente: `git show 3b00d12:docs/notes/handovers/test-hub.md`.
- **Crash del Play risolto**: riprodotto sul portatile (Play listen 3 giocatori in L_Test_Hub, crash ~16 s dopo i manichini; stack in
  `FGSTN_LinkSnapshotContributor::Contribute` ← `ReadSnapshotEntry`, lo stesso dei due crash del fisso). Causa: bug di NP stock.
  `TFixed/TIndependentInterpolateService::RegisterInstance` punta la presentation view dentro una `TSparseArray` di stati inline; quando un
  manichino nuovo la fa crescere, le view degli altri sim proxy restano su memoria liberata (Epic lo aveva già corretto solo nel servizio
  Smooth). Fix (tentativo 1 di 2) in `NetworkPredictionService_Interpolate.inl`: se l'array si rialloca, ripunta tutte le view. Dopo: Play
  60 s, 13 pawn e 7 kit su host e 2 client, nessun crash. Il vecchio aggiramento "niente cambio kit ai manichini" in `play_setup` si può togliere.
- **Kit a terra**: `test_hub.spawn_kits()` al Play spawna un `AGSTN_KitPickup` per pedana (`KIT_PADS`) via `ServerSpawnKitPickup` (nuovo,
  test component). Nessun attore nella mappa: niente Ctrl+S. Le pedane Python non cambiano più kit (`KIT_PICKUPS = True`); la rossa resta.
  Scambio: tieni premuto interagisci vicino a un kit (raggio 150 cm). Visti replicati; lo scambio con l'input vero NON l'ho provato.
- **Cubo per kit** (richiesta owner): `UGSTN_KitPlaceholderLookSubsystem` (GSTNCombatGAS/Pickups), solo presentazione, 4 Hz, non sul server
  dedicato: box colorato dietro la schiena se il kit non ha skin; stesso colore sul kit a terra senza mesh. Cvar `gstn.Combat.KitPlaceholderLook`.
  Non ancora guardato a schermo.
- Test: build ok, headless 996/0, 0 ensure, anche dopo il fix NP. `KitPickupSwap` fallisce se si aggiunge un campo
  a `AGSTN_KitPickup` prima di `Loadout` (layout stantio in build non-unity): ho tolto il campo, il segnaposto usa un component tag.
- Tasti tastiera oggi (KIT_Test_Hits): tasto sx mouse attacco (14 danni, knockback 500 a 30°), accovacciato + sx attacco basso, centrale
  speciale, Y salto del kit, U schivata, O interagisci (tieni = cambio kit), L scatto. Dash, Buff, Scudo, Spin: tasti da leggere nell'IMC.

**Prossimo (thread nuovi):**
1. Mappa pad Xbox dell'owner su `IMC_GASNPP_Test` (in memoria al Play come il resto): X leggero, A salto, Y speciale, B grab, RT forte,
   LT scudo/schivata ("RL" nel messaggio, da confermare), RB abbassarsi. Manca un'azione "forte" nell'IC. (Sonnet, ~$2-3)
2. Mosse placeholder IN BLUEPRINT (leggero/forte/aereo con knockback diversi, hitbox visibili col debug) come riferimento per l'owner;
   le mosse complesse (riflettore, side special, proiettili) in un thread dedicato dopo. (Opus medium, ~$5-8)
3. Guardare in Play cubo e scambio kit con l'input vero; togliere l'aggiramento sui manichini.

Costo stimato: ~170k token di contesto, ~$12-14 (build non-unity lente, due riproduzioni del crash).
