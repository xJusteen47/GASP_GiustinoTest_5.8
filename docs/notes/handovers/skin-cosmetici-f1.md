# Skin cosmetiche, fase 1 (codice cloud) — handover 2026-11-23

Branch `claude/project-thread-oclodp` (base `claude/integ-2026-11-23`). Cloud, Opus medium. Brief: nota del coordinatore + `handoffs/skin-prova-brief.md`, `skin-cosmetici-modulari.md`, `skin-proporzioni-analisi.md`, `skin-lyra-confronto.md`. Riuso dichiarato (INDICE 5.13, 5.15): `UGSTN_SkinDefinition`, `UGSTN_SkinComponent`, riga Kit del pannello GSTN Debug.

**Fatto**
- Proporzioni del corpo (GSTNCombatCore): `Kits/GSTN_SkinProportions.h` con limiti dell'owner (altezza 0,95-1,07, gambe ±4%, braccia ±5%, spalle 0,95-1,20, testa ≤1,25), misura dalla posa di riferimento (ossa UE5 mannequin) + bounds, impostazioni "GSTN Skins" (mesh base vuota = controllo spento). Il validatore di `UGSTN_SkinDefinition` dà errore fuori limite e se la mesh non è sullo scheletro del base.
- Plugin nuovo `GSTNCosmetics` (Core, spento di default, attivo nel .uproject): dati tema/oggetto, slot con involucro, spline, stili, tag `Cosmetic.*`, piano deterministico (fallback al tag padre, override per oggetto, scala nell'involucro, specchio, segmenti spline, limite di silhouette, budget 12/4/8), tag raccolti dai pezzi, chiave di cache, validatore con campo collisione per pezzo (solo NoCollision).
- Debug: `UGSTN_SkinComponent::SetForcedSkin/ClearForcedSkin`, comando `gstn.Debug.Skin <nome|base|off> [all]`, riga Skin nel pannello GSTN Debug (Force, Force on all, Release). Solo locale e solo presentazione.
- Test: `GSTNCombat.Core.SkinProportions.*` (3), `GSTNCosmetics.Core.*` (7).

**Non verificato**: niente è compilato né eseguito (cloud senza UE). Va compilato sul portatile con `Run-Timed.ps1 -Preset Build` e test `-Tests GSTN`. API UE usate a rischio: `USkeletalMesh::GetImportedBounds`, `FBox::TransformBy`, payload di `SButton.OnClicked`.

**Da sapere per la fase 2**
- `UGSTN_SkinComponent` oggi mette la mesh della skin sulla mesh BERSAGLIO (quella che guida la posa), non su quella visibile del VisualOverride: va controllato in editor se questo rompe "uno scheletro invisibile fisso".
- Settings: impostare `BaseMesh` (BirdKnight) in `Config/DefaultGSTNCombat.ini` dopo il riordino di Content (i percorsi cambiano).
- Mancano: tipi primari dei temi in `DefaultGame.ini`, estensione del loadout con tema/stile, assemblatore, cache armadietto, coda asincrona con placeholder amorfo, strumento editor che copia socket/bounds. Niente VFX.
- INDICE-PROGETTO 5.13 (branch `claude/indice-progetto`): aggiungere "GSTNCosmetics fase 1 + proporzioni + gstn.Debug.Skin" quando il branch dell'indice entra in main.

SUBAGENTI: 0
COSTO: ~5 $ | 0 min desktop | 0 token review
