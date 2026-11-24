# Review livello 2: fermate della dair v9 approvata (AS/AM_OverlordDAirV9, 916338ad) - 2026-11-24

Domanda (owner, scelta B): la v9 ha fermate vere o la metrica `stop_sync` (0,76, max 0,5; taratura T4 su claude/anim-skill-t3,
handover skill-anim-unica-t4.md) sbaglia? Clip e metrica NON toccate.

## Materiale
- Cattura Play reale in _ue59 (asset identici a 916338ad, `git diff --quiet 916338ad`), 1 tentativo, 4,6 min, `assert.txt` = ASSERT: OK
  (AM_OverlordDAirV9 peso >= 0,8 sul 100% di f32-62 whiff e f30-58 hit_rise; spec `assert_spec.json`).
- Mappa whiff: a = (f - 30) x 1,04 (sim 62,5 Hz). Il montage in gioco finisce a ~a34 (f63), poi locomozione: a35-42 mai a schermo.
- Pacchetto `final/review_pack.json` (complete: true, `spec2.json`). Deroga: INV-06 vuole base + variante; la v9 E' la base, quindi
  la stessa cattura e' registrata due volte (bande base e variante identiche). Keypose e fogli contatto convertiti in jpg q88.
- Video real + slow4x per dair_whiff e dair_hit_rise in az90, az180, game; fogli contatto `sheets/`; overlay a cipolla del critico `critic/`.

## Critico cieco (Opus high; prima senza metrica ne' frame sospetti, poi con) - FAIL vita e fermate
Identita': palla con ginocchia al petto SI'; piedi giu' dritti SI' di lato, da dietro gambe ad A poi a X (caviglie incrociate f48-58);
braccia davanti al viso, non sopra la testa (tratto 2 a meta'); conserte allo stomp e dopo SI'.
1. a15-17 (f45-47) statua in whiff (nessun hitstop la copre), poi pop a18 (ginocchia e bacino in un tick): stop-and-go [posa].
2. a19-27, quasi statua a23-26 (f53-56): l'affondo progettato (bacino -9 cm su 9 frame, ~1 cm/frame) non si vede [posa].
3. In gioco il corpo sale nella carica e resta sospeso all'apice f45-~77: lo stomp legge verso l'alto [gioco]; la caduta generica
   apre le braccia dopo a34 [dati montage]. In hit_rise la vittima e' metri sotto: colpo non giudicabile, cattura da rifare [gioco].
Impressione: "palla di cannone che sale, poi soldatino rigido sull'attenti, appeso".

## Le tre finestre della metrica
| anim | verdetto | perche' |
|---|---|---|
| a3-5 | tenuta VIVA da dichiarare | 1 frame quasi fermo, poi deriva e ribaltamento (f37-38), capsula che sale. Motivo: carica a4-a12 |
| a17-26 | DIFETTO vero (come jab v12) | statua 15-17 + pop 18, poi quasi statua 23-26 |
| a34-36 | falso allarme | mai a schermo (montage finisce ~a34, fusione continua f62-66) |
Controllo in sola lettura (regola 11) sul dump `docs/notes/life-calib-run/dumps/joints_AS_OverlordDAirV9.json`, passo per frame:
bacino a15-18 = 1,0 / 1,0 / 0,2 / 0,6 cm, a24-27 = 0,6 / 0,2 / 0,2 / 0,8 cm; testa a15-18 = 1,3 / 1,3 / 0,1 / 0,5 cm. Picco
dell'affondo 2,0 cm/frame (a21). Conferma il critico.
Correzione proposta alla metrica (piu' precisa, non piu' morbida): contare EVENTI (minimi di 8 parti sullo stesso frame = 1),
escludere i frame fuori dal montage in gioco; resta da tarare (regola 17).

## Vista dell'owner, data al critico DOPO il verdetto
"Sembra qualcuno che fa il limbo a mezz'aria." Coincide: busto inclinato indietro, testa dietro il bacino, ginocchia avanti, corpo
sospeso (f46-57). Il critico aveva pesato di piu' fermate e sospensione; l'owner nomina la causa principale, l'inclinazione indietro.

## Proposte per una VARIANTE (v9 bloccata), da studio Doom K8 e proposte stomp 23/11
1. a15-27 busto verticale o appena avanti (+12-18 gradi bacino/colonna), testa avanti-giu' 5-8 cm, sguardo sul bersaglio [posa].
2. a15-20 ginocchia tese, piedi sotto il bacino; cedimento a18-26 ginocchia avanti 10-15 gradi, bacino giu' sui talloni [posa].
3. a15-17 tesa ma viva (piedi giu' 2-3 cm, bacino -1-2 cm); rimbalzo a18-20 da 9 a 3-4 cm con ease, piedi sfalsati 1 frame [posa].
4. a17-24 affondo leggibile: bacino -15-20 cm veloce poi lento, spalle +1-2 frame, testa ultima; a23-27 respiro 2-3 cm [posa].
5. a15-28 da dietro gambe unite e parallele, mai caviglie a X [posa].
6. a13-34 sagoma a goccia: gomiti 10-15 cm fuori dal busto, spalle su 5 gradi [posa].
7. a4-12 avambracci incrociati 10-15 cm piu' su (sopra/davanti alla fronte), braccia +1-2 frame dopo ginocchia [posa].
   DA DECIDERE OWNER: aprire la palla "a porta da calcio" (proposta 1A del 23/11) tocca il tratto 1.
8. DA DECIDERE OWNER: spinta in basso al rilascio (gioco; contrasta "niente stallo" 3A). Montage fino a posa compatta o fusione in
   caduta a braccia conserte [dati montage]. Cattura colpo con attaccante sopra la vittima [gioco].
