# Handover polish M0: fondamenta di presentazione (2026-11-17, cloud, Opus high, budget 13 $, costo stimato ~10 $)
Branch `claude/project-thread-kup1kk` da `claude/playtest-fixes-1117c` 23fb94a4 (stance-offset già dentro: `git merge-base --is-ancestor origin/claude/stance-offset HEAD`). NON compilato (cloud): build + test = passo successivo.
## Fatto
1. **Slot di override (L1)**: `IGSTN_OverrideLayerSource::GetOverrideSlot()`, slot in `GSTNOverrideSlot` (1 wall, 2 skid, 3 run start; priorità di default 30/20/10), scelta pura `FGSTN_OverridePick::Pick`; `UGSTN_CharacterAnimInstance::ActiveOverrideSlot` (il bool resta). Editor: `InsertOverrideLayerSlots` (Blend Poses by int; sostituisce IN PLACE il vecchio seam a bool, tiene base e Inertialization). Script `Tools/GAS/setup_anim_layers.py` (crea ALI_GSTN_Skid/RunStart vuote, rigenera il seam); `setup_wall_anim_layer.run()` ora delega a lui.
2. **Nuovo plugin `GSTNPresentation`** (abilitato in uproject): tag `Presentation.Event.*` (tutti quelli del brief), `UGSTN_PresentationTable` (DataAsset: evento -> Niagara, suono, camera shake, additivo breve via nodo additivo direzionale; soglia + scala magnitudo->intensità; riga di un tag padre vale per i figli; tabella `Parent` = fallback skin->kit->default), `UGSTN_PresentationComponent` (stack di tabelle per priorità, effetti non sul dedicated server, `OnPresentationEvent` + `BindEventByTag` = evento BP per tipo).
3. **Derivazione + rollback**: `FGSTN_PresentationDeriver` (puro, fronti dello stato presentato) e `FGSTN_PresentationDedupe` (una volta per (evento, frame), finestra ±2 per i fronti, frame esatto per hit e inizio mossa). Online: ogni macchina deriva dal proprio stato, nessuna RPC.
4. **Adattatore** `UGSTN_CombatPresentationEvents` (GSTNCombatNP, aggiunto da CombatSim a BeginPlay, anche sul server): Mover + combat state view + tag ASC + ultimo hit confermato; layer "Kit" = `FGSTN_PresentationKitExtension` del kit (assorbe "hit cue kit data" lato vittima).
## Scelte applicate (regola 6, da confermare 13d)
- Hit: magnitudo = frame di hitstun (non velocità di lancio). Turn180 = corsa opposta entro 30 frame dalla fine dello skid.
- Bounce muro/terra/soffitto riconosciuti dal riflesso della velocità in reazione (non serve l'evento sim); funziona anche con M5.
- Roll e Dodge: nessuna regola di default (servono i tag delle abilità di v94blx): `DeriveRules.TagRules` con `bMoveTag`.
## Test (scritti, NON eseguiti)
`GSTNPresentation.Core.{Scale,Lookup,Dedupe,Derive.Air,Derive.Run,Derive.Tags,Resimulation}`, `GSTNAnimation.Core.OverrideSlot.Pick`.
## Passo successivo (fisso o portatile, Haiku low/Sonnet)
1. Build (script a tempo, 45 min). Punti a rischio: `UAnimGraphNode_BlendListByInt::AddPinToBlendList` (export), `UMoverComponent::GetSyncState() const`.
2. Test sopra + suite ridotta. 3. Editor chiuso: `setup_anim_layers.py` headless, poi `--check`; commit di SandboxCharacter_Mover_ABP + 2 ALI nuove (ABP base: un thread alla volta).
4. PIE: wall slide ancora visibile (slot 1). M2 crea le tabelle (DA_Presentation_Default + kit) e i Niagara.
