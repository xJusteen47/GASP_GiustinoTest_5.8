# Depth marker: come si calcola la posizione dei marker (pseudocodice)

Specchio fedele di `Plugins/GSTNDepthMarker/Source/GSTNDepthMarkerCore/Private/GSTN_DepthMarkerMath.cpp`
(`Evaluate`, `ApplyHysteresis`, `ApplyReferenceLayers`, `EvaluateApproach`). Tutto è solo presentazione: locale, mai simulato.
`Up` = opposto della gravità del personaggio (qualsiasi direzione, anche diagonale o capovolta). "Altezza" = distanza lungo `-Up`.

```text
OGNI FRAME, per ogni personaggio con il marker:

0. CAMBIO DI GRAVITÀ
   se Up è ruotato più di UpResetDegrees in un solo passo: dimentica la superficie scelta e l'ultimo punto a terra

1. ULTIMO PIANO A TERRA
   se il personaggio sta a terra da almeno GroundedMinMs su una base ferma: LastGroundedPoint = piedi

2. MARKER PRINCIPALE "SOTTO" (lungo la gravità)
   candidati = colpi di un trace dai piedi lungo -Up (più uno sweep a "impronta" largo come la capsula)
   scarta: chi non blocca, chi parte compenetrato, chi è escluso (trigger, pawn, proiettili),
           chi sta sopra i piedi oltre PenetrationTolerance (piattaforma sottile appena superata salendo),
           chi ha la normale che non guarda verso Up (lati e sottofacce),
           chi non soddisfa la query Surface.Behaviour (se impostata)
   scelto = il candidato più vicino sotto i piedi (altezza minima)

   isteresi rispetto alla superficie del frame prima (Previous, riproiettata sulla colonna di adesso):
     R1 stessa superficie o complanare (entro CoplanarTolerance e CoplanarAngleDegrees) -> resta, aggiorna il punto
     R2 il nuovo è più vicino                            -> passa subito
          (tranne se ho appena cambiato verso una più lontana e sono ancora nel MinDwell: anti ping-pong)
     R3 sto uscendo dal bordo ma l'impronta tocca ancora la vecchia superficie entro EdgeForgivenessCm
                                                         -> resto sul bordo ("EdgeHold"), alfa che cala verso il limite
     R4 il nuovo è più lontano                           -> passa dopo il MinDwell, con dissolvenza SwitchBlendMs
     R5 sotto non c'è niente                             -> nessuna superficie: decide il piano di riferimento (passo 3)

   punto del marker = intersezione della colonna (piedi lungo -Up) con il piano della superficie scelta
   inclinazione del marker limitata a MaxTiltDegrees; raggio allargato sui pendii (max x2)

3. PIANO DI RIFERIMENTO ("sopra" e "nessun terreno")
   piano = override di un volume del designer se presente, altrimenti piano per LastGroundedPoint con normale Up
   D = altezza firmata dei piedi sopra quel piano
   marker "SOPRA" (verso l'alto, sul piano dell'ultima piattaforma):
       in aria e |D| <= ReferenceMaxDistance e D < -AboveShowDepth (sparisce solo sopra -AboveHideDepth: isteresi)
   anello "NESSUN TERRENO": se il passo 2 non ha trovato niente e D >= 0, anello sul piano di riferimento

4. MARKER DI AVVICINAMENTO (il tuo disegno: il secondo marker sul piano della piattaforma più alta vicina)
   solo in aria
   candidati espliciti = volumi/componenti piano posizionati dal designer che contengono il personaggio
   candidati sondati   = cime colpite da sonde verso il basso: un anello di 6 attorno ai piedi (raggio fisso)
                         + 2 lungo la velocità orizzontale; poche per frame, a rotazione, ultime nella coda dei trace
   per ogni candidato:
       proietta la colonna dei piedi sul suo piano -> S (quanto il piano è sopra/sotto i piedi)
       scarta se: cima sondata che non guarda verso Up; stesso piano del marker principale (o complanare);
                  piano più di ApproachNearAboveFeet sopra i piedi (quello è compito del marker "sopra");
                  sondato e dislivello sul principale fuori da [ApproachMinRise, ApproachMaxRise]
       Edge   = distanza orizzontale dai piedi al punto colpito (stima per eccesso del bordo vero)
       Toward = velocità orizzontale verso quel punto
       sondato valido se Edge <= ApproachDistance (o <= ApproachDistance + HideMargin se è già quello mostrato)
                     e (Edge <= ApproachCloseDistance oppure Toward >= ApproachMinTowardSpeed)
   scelta: i piani del designer vincono sempre (priorità, poi chiave);
           altrimenti il bordo più vicino, a parità la velocità di avvicinamento maggiore
   isteresi: uno sfidante sostituisce il piano mostrato solo dopo ApproachMinDwell e se più vicino di ApproachSwitchMargin
             (o se è un piano del designer); se il piano mostrato non è stato sondato in questo frame lo tengo per ApproachHoldMs
   punto del marker = colonna dei piedi proiettata sul piano scelto (quindi anche nel vuoto oltre il bordo)
   alfa = 1 vicino al bordo, cala fino a 0 alla distanza di sparizione

5. VISIBILITÀ E DISSOLVENZE
   visibile solo in aria, sopra ShowHeight (sparisce sotto HideHeight: isteresi)
   alfa e raggio interpolati con l'altezza fino a FadeEndHeight
```

Valori predefiniti dell'avvicinamento (da confermare): ApproachDistance 500 cm, HideMargin 100 cm, CloseDistance 150 cm,
MinTowardSpeed 60 cm/s, dislivello 30-1500 cm sopra il principale, NearAboveFeet 60 cm.
