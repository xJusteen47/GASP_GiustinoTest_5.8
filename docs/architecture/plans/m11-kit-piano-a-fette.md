# M11 "Kit in gioco": piano a fette (2026-11-11, da approvare)

Piano, niente codice. Base: card [m11-kit.md](m11-kit.md) (gate G0-G8), spec [combat-loadout-mods.md](../combat-loadout-mods.md),
handover M10 [m10-slice7.md](../../notes/handovers/m10-slice7.md) (tip `f9d5e0a3`). Aggiunge le idee dell'owner del 10/11 (prese stile
Smash, nemici sollevabili come oggetti, mosse a distanza mirate). Tutto resta soggetto al playtest.

## 0. Cosa c'è già (non si rifà)

- Kit a terra e scambio: `AGSTN_KitPickup`, `UGSTN_KitSwapSubsystem` (tieni premuto interagisci, swap server fuori banda, nuova
  `LoadoutRevision`), hook `DropKit` non chiamato; 7 kit sulle pedane dell'hub con scatola colorata sulla schiena (impianto segnaposto).
- Prese, pummel, lanci, mash (GSTNLink F12), estensione link per kit (`FGSTN_LinkKitExtension`), tabella azioni link.
- Oggetti tenuti (M10 GSTNHeld): tap = presa, hold = solleva, B = posa, lancio con hit ring e fuoco amico per sorgente.
- Hitbox: priorità tra hitbox sovrapposti (sweet/sour spot, M4), `ReachAxis` sui hitbox (dato pronto per la portata scalabile).
- Bit predetto `bPrecisionAim` (reticolo acceso) nel comando di input: pronto per mosse a distanza mirate.

## 1. ESSENZIALE PRIMA DEL PLAYTEST (proposta)

Obiettivo: l'owner prende kit diversi nell'hub col pad, li sente diversi, prende e lancia nemici. Niente mod, zaini, editor.

| # | Fetta | Macchina, modello | Costo | Dipende da | Verifica |
|---|---|---|---|---|---|
| **K1** | Scambio kit col tasto interagisci vero: il kit a terra diventa candidato `GSTNInteract` (focus su quello inquadrato, scritta a schermo), convive con la raccolta oggetti di M10 (vince la cosa inquadrata, hold del kit separato, decisione 6); swap a metà di un'abilità (taglio al confine di revisione); `bDropKitOnDeath` come impostazione di progetto (M12 la sposta nelle regole di modalità). Assorbe G0 (audit) | portatile headless, Opus medium | $5-8 | M10 fetta 8 su feature | test `GSTNCombat.GAS.Loadout.KitPickupSwap` esteso; headless verde; harness in K4 |
| **K2** | Lanci del kit stile Smash: nella presa lo stick sceglie il lancio (avanti, dietro, su, giù), 4 lanci per kit in `FGSTN_LinkKitExtension` con set di riserva; presa anche con speciale + accovacciato (decisione 2); nemico afferrabile ma non sollevabile → solo questi lanci, sollevabile → flusso oggetto M10 | portatile headless, Opus high | $8-12 | M10 G9 su feature (stessi file GSTNLink/GSTNHeld) | test `GSTNLink.*` nuovi (direzione → lancio, ramo sollevabile); headless verde |
| **K3** | Tre kit di prova davvero diversi (leggero/veloce, medio, pesante/lento: stats, peso, moveset e lanci con montage GASP segnaposto) sulle pedane dell'hub, messi dallo script di avvio | fisso, Sonnet medium | $3-5 | K1, K2 | PIE listen a occhio (Claude), log dello swap |
| **K4** | Controllo pre-playtest: `scenario_kit_pickup` e `scenario_kit_throw` in harness (listen 2 %, dedicated 0 %), SUITE breve, build delle tre config, nota "come provare" nella lista cose a mano | fisso, Sonnet medium | $3-5 | K1-K3 | harness PASS, 0 ensure |

Dipendenza esterna, da tenere nella coda M10 (non M11): **M10 G9 nemici lanciati** deve far sì che un nemico sollevato si comporti come
un oggetto tenuto (B posa, RT tap lancia verso lo stick sinistro, RT tenuto reticolo + lancio verso la camera). L'RT tenuto è il resto di
**M10 G7 (AimPolicy camera/spalla)**, non ancora fatto: va aggiunto alla coda M10 prima del playtest (~$5-8, una fetta).

**Essenziale: 4 fette M11, $19-30** (+ G7 rimasto di M10).

## 2. PUÒ ASPETTARE DOPO IL PLAYTEST (proposta)

| # | Fetta (gate della card) | Macchina, modello | Costo |
|---|---|---|---|
| L1 | G1 cuciture M3: `Extensions` tipizzate nel loadout, registro delle estensioni di compilazione, provider di attacchi sulla skin | portatile, Opus medium | $6-10 |
| L2 | G2 resto: plugin `GSTNPickups` (il pickup kit esce da GSTNCombatGAS con redirect), payload arma di supporto, regola di gara | fisso, Opus medium | $6-10 |
| L3 | G3 armi di supporto (eredità stats del kit) e cambio speciali tra quelli disponibili | portatile, Sonnet medium | $5-8 |
| L4 | G4a portata scalabile su `ReachAxis` e sweet/sour che seguono la lama | portatile, Opus medium | $6-10 |
| L5 | G4b mod di moveset e stats, mod degli speciali, limite del registro | portatile, Opus medium | $6-10 |
| L6 | G5 impianto sulla schiena con connettore, impianti arma su mani/piedi (segnaposto), mod cosmetiche | fisso, Sonnet medium | $4-7 |
| L7 | G6 zaini sul connettore (esempio jet-pack) | portatile, Sonnet medium | $6-10 |
| L8 | G7 editor delle mod (anteprima nella timeline, validatori) | portatile, Sonnet medium | $4-7 |
| L9 | Mosse a distanza mirate: attacchi forti su RT col reticolo (`bPrecisionAim`) | portatile, Opus medium | $5-8 |
| L10 | G8 sweep: scenari harness, pedana mod/zaini nell'hub, doc | fisso, Sonnet medium | $4-7 |

Ordine: L1 → L2 → (L3, L4, L6, L9 in qualsiasi ordine) → L5 → L7 → L8 → L10. **Dopo playtest: 10 fette, $52-87.**
Totale M11 con le aggiunte del 10/11: **$71-117** (card originale $53-92). L1 e L4/L5 toccano il loadout di GSTNCombat: mai in parallelo
con un'altra fetta che tocca `GSTNCombatCore`/`GSTNCombatNP`.

## 3. Ci sta entro giovedì 12 / venerdì 13?

Conti: fisso ~04-18 Roma, massimo 2 thread di codice (uno sul fisso, uno sul portatile headless), una fetta reale ~30-60 min con build e
harness, più attese di merge. Coda M10 prima del playtest: fetta 8, G9, G7 rimasto, G12-G13 = 4 fette sul fisso.

- **Mercoledì 11**: fisso finisce M10 (fetta 8, G9, G7 rimasto); portatile fa K1 in parallelo (non tocca GSTNHeld).
- **Giovedì 12**: portatile K2 (dopo G9 su feature); fisso G12-G13, poi K3; K4 nel pomeriggio.
- **Venerdì 13**: margine per 1-2 tentativi falliti o un merge che tarda.

**Essenziale: sì, probabile giovedì sera, sicuro venerdì**, se G9 non sfora e le decisioni arrivano entro mercoledì sera.
**M11 intero: no.** Restano 10 fette (~2-3 giorni con 2 thread): realistico la settimana dopo, ed è meglio farle dopo il playtest perché
mod, zaini e armi di supporto dipendono da cosa piace in mano.

## 4. Decisioni dell'owner (opzione consigliata in grassetto)

1. Divisione essenziale/dopo: **come §1-§2** (alternativa: armi di supporto L3 dentro l'essenziale, +1 fetta, ci sta ancora venerdì).
2. Ingresso dei lanci del kit: **presa con speciale + accovacciato oppure tap interagisci (entrambi), poi lo stick da solo sceglie il
   lancio come in Smash**; alternativa: stick + speciale. Entrambi come impostazione `ThrowInputMode`.
3. Lanci per kit: **ogni kit ha i suoi 4 lanci, con set di riserva comune** (alternativa: un set unico per tutti).
4. Chi è sollevabile: **peso ≤ capacità del kit (regola M10)**, più un tag `Combat.Grab.NotLiftable` che lo vieta per nemico. Entrambi attivi.
5. Kit lasciato alla morte nel playtest: **sì** (`bDropKitOnDeath` acceso; poi regola di modalità in M12).
6. Stesso hold per raccogliere oggetti e cambiare kit: **vince la cosa inquadrata; il kit vuole un hold più lungo (0,5 s) per non
   cambiarlo per sbaglio**. Durata come impostazione.
7. RT tenuto con reticolo per i lanci (resto M10 G7): **prima del playtest**; attacchi a distanza mirati (L9): dopo.
