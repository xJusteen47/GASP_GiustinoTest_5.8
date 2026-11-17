# Brief M0: fondamenta di presentazione (polish movimento + VFX)
Macchina: CLOUD (niente editor). Modello: Opus high. Budget: 13 $. _ue59: NO. Base: branch claude/playtest-fixes-1117 (o il più recente della catena playtest; verificare in INDICE/memoria).
Piano: handoffs/polish-movimento-vfx-piano.md §2 M0, §5. Piani collegati: anim-layer-skid-wallslide-piano.md (fase L1).
## Obiettivo
1. **Slot di override a int (L1)**: IGSTN_OverrideLayerSource dà uno slot; il bool di override nell'ABP base diventa Blend Poses by int (ActiveOverrideSlot), priorità wall > skid > start. Script di setup (setup_anim_layers.py) rigenera il grafo; nessun cablaggio BP a mano.
2. **Tabella eventi di presentazione per kit/skin** (DataAsset, BP-usabile, plugin "nativo"): evento (tag Presentation.Event.*) → Niagara, suono, camera shake, additivo breve opzionale, scala da un parametro (velocità d'impatto, forza del colpo). Eventi: Jump(tipo), Land(velocità), RunStart, StageUp, Skid enter/loop/exit, Turn180, StopRelease, Hit(forza), Splat, WallBounce, GroundBounce, Knockdown, GetUp, Roll, Dodge, ShieldOn/Hit/Break.
3. Eventi derivati dallo stato sincronizzato lato presentazione (mai nella sim); rollback: dedupe per (evento, frame), nessun doppio effetto dopo una resim. Evento BP per ogni tipo (online: arriva anche sui client).
## Vincoli
Niente hardcode di input, niente Project Settings per gameplay; ABP base: un solo thread alla volta (verificare che stance-offset sia entrato). Test sulla logica pura (dedupe, scala). Assorbe "hit cue kit data".
## Fine
Handover ≤30 righe docs/notes/handovers/polish-m0.md + copia in handoffs/; build/test sul fisso o portatile = passo successivo scritto nell'handover.
