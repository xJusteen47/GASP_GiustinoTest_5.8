# Skin con proporzioni diverse su uno scheletro di gioco unico (analisi, 2026-11-23)

Cloud, Opus medium, sola lettura del repo, nessuna modifica al codice. Legenda: **[letto]** = verificato nel repo (file citato), **[dedotto]** = mia inferenza, da verificare sul fisso.

## 1. Come funziona oggi
- **La simulazione è già "a scheletro invisibile".** Nessun codice di simulazione legge una mesh: hitbox `BakedSocket` e ossa seguite dalle hurtbox sono campionate **offline** dal montage di **riferimento** al tick NPP e salvate nella timeline (`BakedSockets`); a runtime nessuna animazione viene valutata [letto: combat.md §12.1, anim-layers.md §5]. Il bake registra la mesh usata (`FGSTN_BakeInfo::Mesh`, solo editor) [letto: `GSTN_TimelineTypes.h`].
- **Hurtbox**: `UGSTN_HurtboxSetDefinition`, forme root-relative nella posa di riferimento; con `bFollowBone` seguono l'osso cotto nella mossa (`Shape.Local * BoneReference^-1 * osso cotto`). Il set è del componente combat / kit, **non della skin** [letto: `GSTN_HurtboxTypes.h`, handover `hurtbox-follow-bones.md`, anim-layers §5.4].
- **Capsula / pushbox**: dalla capsula di movimento (Mover) e dalle tracce Body Capsule / Pushbox della timeline, numeri in cm, indipendenti dalla mesh [letto: `GSTN_Pushbox.h`, `GSTN_GroundCrouch.h`].
- **Skin** (`UGSTN_SkinDefinition`): mesh + materiali + AnimClass + footwear, solo presentazione; una skin può cambiare l'animazione visiva ma il validatore impone stesse sezioni, durate e root motion [letto]. Regola già decisa: *la hitbox resta la stessa per ogni skin*.
- **Scheletro**: tutto è autorato su `SK_UEFN_Mannequin`; il rig Cascadeur del proprietario È lo scheletro UE (stesse ossa) [letto: `jab-fk-1.md`, `polish-m1.md`]. Non esiste ancora una mesh Overlord/BirdKnight/Voyager/FlameFury nel repo [dedotto: nessun riferimento in docs/Tools]. GASP prevede già un "VisualOverride" (attore figlio che copia/retargeta la posa del manichino nascosto) [letto: `backpack-back.md`, `ground-contact-fix-1.md`].

Quindi: **sì, si usa il baking**, e la logica è già separata dalla skin. Il modello Lyra/Fortnite c'è per metà: manca solo la regola su *quanto* una skin può discostarsi dal manichino.

## 2. Cosa cambia se le skin hanno proporzioni diverse
- **Resta uguale**: frame data, hitbox, hurtbox, capsula, rete, rollback, ledge check, wall query (tutti in cm dal manichino). Ogni skin con lo stesso kit è identica in gioco.
- **Si rompe (solo visivamente)**: (a) mano/piede visibile non coincide più con la hitbox: con un braccio +6% lo scarto alla portata massima (~70 cm) è ~4 cm [dedotto]; (b) piedi che galleggiano o affondano se cambia la lunghezza delle gambe e il bacino resta all'altezza dell'animazione; (c) mani/piedi sul muro nella wall slide, prese (grab/link usano socket cotti del partner), oggetti tenuti in mano; (d) compenetrazioni: Overlord largo + pose del manichino (braccia conserte del dropkick, pugni davanti al petto) = braccia dentro il torace; (e) pose approvate e bloccate (regola 13b) rivalidate su ogni corpo.

## 3. Opzioni
**A. Proporzioni nella mesh, scheletro condiviso, tolleranza fissa.** Ogni skin è skinnata su `SK_UEFN_Mannequin` con la sua posa di riferimento; le ossa prendono la lunghezza dalla mesh (traslazione "Skeleton"), il bacino "AnimationScaled" così l'altezza del bacino scala con le gambe [dedotto: impostazioni UE standard; il modo attuale delle ossa va letto sullo scheletro]. Costo runtime zero. Limiti proposti, rispetto al manichino base (= 100%):

| Misura | Limite | Perché |
|---|---|---|
| Altezza totale (testa compresa) | **−5% / +7%** | owner: niente scarti esagerati; la testa grande recupera altezza |
| Lunghezza gambe (anca-caviglia) | **±4%** | contatto piedi, ledge, caviglie ≥10 cm (regola 15b) |
| Lunghezza braccia (spalla-polso) | **±5%** | scarto mano/hitbox ≤ ~4 cm, sotto il raggio delle hitbox |
| Testa (scala) | **fino a +25%** | non tocca superfici, la hurtbox testa resta |
| Larghezza spalle/torace | **−5% / +20%** | oltre, le pose di petto compenetrano |
| Mani e piedi (dimensione, non posizione) | **fino a +20%** | look "cartoon" senza spostare il contatto |

Proposta di corpi: **BirdKnight ≈ base** (100% altezza, spalle +10%); **Overlord** +6-7% altezza, spalle +20%, mani +20%; **Voyager / FlameFury** gambe −4%, torso −3%, testa +20-25% → altezza finale ~96-97% (la testa compensa, come chiede l'owner).

**B. A + scala ossa per skin a runtime.** Solo per ossa che non toccano niente (testa, mani come dimensione, petto), con clamp ai limiti sopra; mai lunghezze di braccia/gambe (spostano contatti e invalidano la revisione). Utile per ritocchi in playtest (regola 15c: parametro invece di clip nuova). Costo basso, rischio: la scala delle mani ingrandisce anche gli oggetti attaccati ai socket.

**C. Retargeting per skin (IK Retargeter, mesh visibile che copia il manichino nascosto).** Qualsiasi proporzione e anche scheletri diversi. Costo: un IK Rig + retargeter per skin (~0,5 giornata di setup + revisione), CPU per ogni personaggio visibile (da misurare con 40+ nemici), e soprattutto **le pose approvate cambiano**: braccia conserte, pugni, mani sul muro vanno ricontrollati e spesso corretti per skin. Il retargeting già usato (skid) ha fatto affondare il corpo [letto: `merge-skid-camera-jump-1.md`].

**Consiglio: A, con B limitato alle ossa "cosmetiche".** C solo come uscita di sicurezza per una skin fuori tolleranza (es. una creatura). Motivi: la simulazione è già indipendente dalla mesh, A costa zero a runtime, una sola revisione d'animazione vale per tutti i corpi entro la tolleranza, e i limiti rendono misurabile ("dentro/fuori") una cosa oggi a occhio.

**Rischi da controllare (A+B)**: IK/contatto piedi (nostro `GroundContact`, presentazione) va tarato sulle gambe ±4%; foot planting: scivolamento se il passo della clip non scala con la gamba (±4% = ~1-2 cm per passo [dedotto]); muro/ledge: mani a pochi cm dal muro, si nasconde con un IK di presentazione sulle mani se serve, mai spostando la capsula; portata: la hitbox è del kit, quindi Overlord *sembra* arrivare più lontano di Voyager col pugno ma colpisce uguale (coerente con la regola "hitbox uguale per ogni skin"); compenetrazioni sul torace largo: si risolvono con una variante visiva della mossa per tratto `Anim.Trait.Heavy` (sistema già pronto, anim-layers §4).

## 4. Migrazione (non avviata)
| # | Cosa | Dove / modello | Stima |
|---|---|---|---|
| 1 | Leggere il modo di traslazione delle ossa di `SK_UEFN_Mannequin`, fissare bacino AnimationScaled (asset condiviso: avvisare l'owner) | fisso, Haiku | 1-2 $ |
| 2 | Due mesh di prova sullo stesso scheletro: "Overlord" +7%/spalle +20%, "ragazzo" gambe −4%/testa +25% (mesh stand-in, PROTO) | fisso, Sonnet | 3-5 $ |
| 3 | Cattura (dietro + lato) idle, corsa, jab, dropkick, wall slide, grab, sulle due mesh e sul base; debug draw hitbox/hurtbox acceso | fisso, Haiku + review cloud | 4-6 $ |
| 4 | Validatore `UGSTN_SkinDefinition`: misura la posa di riferimento della mesh contro il base e dà errore fuori tolleranza; il bake usa sempre il manichino base (`DefaultPreviewSkin`), errore se `BakeInfo.Mesh` è una skin | cloud, Opus medium + build fisso | 5-7 $ |
| 5 | Parametri B (scala testa/mani/petto, clamp) nella skin, cvar nel menu GSTN Debug | cloud, Sonnet + build | 3-4 $ |

Totale ~16-24 $. **Da testare sul fisso**: caviglie ≥10 cm e piedi non scivolanti (regola 15b), mani sul muro nella wall slide, braccia conserte del dropkick e pugni davanti al petto senza compenetrazioni, scarto mano/hitbox all'attivo del jab ≤ raggio hitbox, oggetti/zaino ai socket, camera (usa la capsula, deve restare uguale).

**Decisioni per l'owner**: (1) i limiti della tabella (A consigliata); (2) quale corpo è il "manichino base": consiglio il BirdKnight medio, così Overlord sta sopra e i ragazzi poco sotto.

COSTO: ~2 $ | 0 min desktop | 0 token review
