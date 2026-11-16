# Corsa stile Mario: piano (2026-11-16, solo piano, niente codice)

Richiesta owner 2026-11-16 09:58Z (ambito: locomozione a terra e in aria del personaggio giocante, tutti i kit).
Fonti lette in sola lettura: prototipo `Unreal Projects/GaspMoverProject` (UE 5.7, ultimo Mover aperto, 2026-10-12) e `feature/gas-npp-5.8` d258c41c.

## 1. Cosa fa il prototipo 5.7 (letto da C++ e dai nomi nei Blueprint)
- **Stadi di corsa**: non ci sono 3 stadi. C'è il gait GASP Walk/Run/Sprint di `BP_MovementMode_Walking` (RunSpeed, SprintSpeed, SprintAcceleration, SprintTurnStrength, "Dampen Turning Strength", decelerazione forte all'atterraggio), scelto da un tasto (`IA_Sprint`), non dal tempo di corsa.
- **Aria**: `AirControlPercentage` fisso (impostazioni Mover comuni) + gravità extra in salita/discesa in `AC_GiustinoEnhanchedMoves` / `BP_GiustinoFalling`. Il controllo non dipende dalla velocità di stacco: è il difetto che hai notato.
- **Skid**: `FGiustinoSkidLayeredMove` (C++) + logica BP (MinSkidDot, MinSpeedToSkid, SpeedAfterSkid, montage `SS_180_Turn`). Difetti: (a) parte dal game thread e il timer usa il DeltaTime del tick, quindi niente rollback né determinismo; (b) il freno vale `min(v, v*2.75) = v` come velocità additiva, quindi azzera la velocità ogni frame e BrakingFactor non conta; (c) si annulla appena lasci lo stick; (d) la svolta a 180 è un montage BP, non simulazione.

## 2. Cosa c'è già nel 5.8
Stesso walking mode gait GASP (Run/Sprint scalati dalla statistica WalkSpeed via `FGSTN_StatsMovementModifier`); attributo `AirControl` dichiarato ma **non letto da nessun mode**; salto `GSTN_GA_PlatformJump` (coyote, squat, hold); `MomentumSlide` per mossa già su feature (scivolata lungo/contro il momento all'inizio di un attacco a terra). Lo skid del prototipo **non è stato portato** (esiste solo lo skid sui muri).
**Stick sinistro (L3)**: nell'IMC vivo `IMC_GASNPP_Test` è libero (R3 = reticolo automatico). La voce in coda "melee to left stick" la leggo come *direzione del melee dallo stick sinistro*, non come click: nessun conflitto, ma confermalo.

## 3. Proposta (tutto nella simulazione a 62.5 Hz, tutto dati per kit)
- **Dati**: nuovo `UGSTN_LocomotionProfile` (DataAsset) referenziato dal kit. Tabella di 3 stadi: velocità max, accelerazione, turning strength, freno a stick rilasciato, frame per salire allo stadio dopo. Le velocità restano scalate dalla statistica WalkSpeed. Niente Project Settings.
- **Stato**: struct nello sync state di Mover (stadio, frame nello stadio, frame da inizio movimento, frame da rilascio stick, velocità di stacco, frame di skid), così rollback e resim la ripristinano.
- **Stadi**: si sale se la velocità resta sopra la soglia dello stadio per N frame con lo stick in avanti; si scende con svolta stretta, skid, urto o stick rilasciato oltre N frame.
- **Salto allo stadio 3**: comando `Command.Movement.RunBoost` legato a un tag bottone nuovo (`Input.Button.RunBoost`), tasto di default L3 nei binding di default del kit (dati). Valido entro una finestra di frame dall'inizio del movimento, dato per kit.
- **Aria**: velocità orizzontale massima in aria = velocità di stacco (con minimo); accelerazione di sterzata e frenata per stadio di stacco, più una curva "controllo vs velocità di stacco". Usa finalmente l'attributo AirControl come moltiplicatore.
- **Skid e svolte strette**: sotto la soglia (stadio 1, o dot non abbastanza opposto) svolta stretta senza skid (pivot in pochi frame); sopra, skid nel sim con freno vero (cm/s²) e durata in frame; skid non annullato dal rilascio stick, salto dallo skid = finestra dati (salto laterale opzionale, non costruito ora).
- **Momento negli attacchi neutri**: la sim ricorda la velocità di corsa prima del rilascio stick per K frame (K per stadio). Un attacco neutro entro K usa quella velocità come ingresso del `MomentumSlide` già esistente; oltre K vale il freno dello stadio (forte allo stadio 1, quindi poca scivolata).

**Da portare**: struttura del layered move di skid, gravità extra salita/discesa (se il PlatformJump non la copre già), parametri gait come base dei valori. **Da rifare**: tutto quello che gira sul game thread, il freno dello skid, il controllo aereo fisso, il montage di svolta come guida del movimento.

**Fasi (stime, non costi reali)**: P1 profilo + sync state + 3 stadi + L3 (Opus medium, ~8 $) · P2 aria + skid (~6 $) · P3 momento negli attacchi + test headless (~5 $) · build e playtest sul fisso (Haiku, ~2 $). Ogni fase un thread; serve il fisso solo per build e playtest.

## 4. Decisioni per l'owner (risposta tipo "1A 2A 3C")
1. **Controllo in aria**. A) tetto = velocità di stacco, sterzata per stadio (stile 3D World) · B) solo percentuale di controllo da una curva sulla velocità di stacco · C) entrambi, scelta per kit (+1 $). Consigliata **C, default A**.
2. **Velocità portata negli attacchi neutri dopo aver lasciato lo stick**. A) quella di prima del rilascio, entro K frame per stadio · B) quella attuale già frenata (più semplice, meno momento). Consigliata **A**; costo uguale.
3. **L3 verso lo stadio 3**. A) solo entro la finestra iniziale dallo stadio 1 · B) in qualsiasi momento della corsa · C) entrambi come dato per kit (+0.5 $). Consigliata **C, default A**.
Intanto resta fermo solo questo lavoro; nessun'altra voce della coda dipende da qui.
