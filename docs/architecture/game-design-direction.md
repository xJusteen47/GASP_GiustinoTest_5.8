# Game design direction (owner input)

Status: **owner input, 2026-10-22**. This file records the owner's intended gameplay, so that every combat
specification can be checked against it. Where a specification disagrees with this file, this file wins and the
specification must be updated (or the owner asked). The owner said more answers will follow in
[open-questions-m4-m10.md](open-questions-m4-m10.md); fold them in here.

Section 9 keeps the owner's original message (Italian) verbatim; sections 1–8 are its reading in English, with the
consequences for the design. Items marked *Reading* are interpretations to confirm with the owner.

## 1. Destructible body parts (Helldivers 2 model)

- Enemies have several **parts**, each affecting health differently.
- Each part has a **hardness** (armour value). A hitbox has a **penetration** value; the hit's penetration against the
  part's hardness decides whether the hit damages the part (and whether the part can be destroyed).
- The entity's health depends on the damage its parts take. **Weak points** have low hardness and transfer high
  damage to the main health ("durable damage" in the Helldivers sense: the share of damage that goes to the main
  pool). *Reading:* each part has its own health and a transfer factor to the main pool, like Helldivers 2's
  per-part health plus "durable/main" damage split.
- Breaking a part changes the entity: e.g. an enemy too big to grab becomes grabbable once parts are broken off.
- Parts apply to **players too** (section 5): players also lose pieces as they lose health.

## 2. Health as a capped percentage, and weight

- Health works like a platform-fighter **damage percent** (knockback grows with it), but with a **cap**: when the
  percent passes the cap, the part (or the whole entity) is destroyed. So "percent and HP together".
- Enemies have a **weight** linked to their remaining health, which feeds knockback. *Reading:* effective weight
  falls as damage rises (and as parts break), so a battered enemy flies further.
- Enemies have a **stagger** factor and a **juggle** factor (the weight for knockback): **hitstun and hitstop depend
  on what hits them and where** (which part, which hit).

## 2b. Hitstun per part, stagger force, hitstop, weight (owner input, 2026-10-22 morning)

- Whether an enemy or boss enters **hitstun depends on the part that is hit**. Each part has its own **hitstun
  condition**: generally, the attack's **stagger force** parameter must exceed a threshold set on the part.
- **Stagger force** generally **scales with the attack's power**, but the designer can set it independently.
  *Reading:* by default derived from the hit's damage/knockback, with a per-hit override.
- **Hitstop** can be **computed automatically from the attack's damage and knockback**, or **set by the designer**
  per attack if they want.
- **Weight** decides what happens when an entity enters hitstun: how far it **flies**, or whether it does not fly at
  all and is only **staggered in place**, or is **pushed back**. *Reading:* three reaction tiers (launch, push back,
  stagger in place) chosen by comparing the knockback with the victim's effective weight (which also depends on
  remaining health, section 2).

## 2c. DI, SDI and dodging in hitstun (owner input, 2026-10-22 late morning)

- **No dodging during hitstun or hitstop**, "generally" (so a kit may still have a special escape).
- **SDI**: holding the direction you want to drift, during hitstop (not mashing).
- **DI** (owner examples, in terms of the launch direction's dot with the up vector):
  - launched diagonally forward with dot `0.4`: stick **towards the attacker (DI in)** → dot `0.6`; stick **along the
    launch (DI out)** → the message says `0.4` (*Reading*: probably `0.2`, see M4.29);
  - launched horizontally (dot `0`): DI in → dot `0.2`; DI out → no change;
  - **mirrored for downward launches** (dot with the down vector > 0): DI in → steeper down, DI out → flatter;
  - launches with dot above about `−0.1` behave like upward ones;
  - **lateral stick rotates the launch around the up axis**.
- Formula derived from these examples: combat-m4-hits.md §3 (DI).

## 2d. Arbitrary gravity and trampolines (owner input, 2026-10-22 late morning)

- **Gravity can change arbitrarily.** "Up" is always the opposite of the local gravity of the entity at that frame,
  never world Z: DI, launch angles, push back, the movement plane, projectiles and minion movement all follow it.
- **Trampolines may exist** in levels: everyone landing on one is launched along its normal; a tumbling victim bounces.
- **Trampolines** (2026-10-22 18:27): a box (parallelepiped) with adjustable height, depth and width; mesh, size, launch direction,
  tilt etc. can differ per instance (M4 H11b).
  *Proposal* in combat-m4-hits.md §7b (own small plugin, not in the combat core).

## 2e. Super armor and ricochet (owner input, 2026-10-22 late morning)

- **Super armor** is the special case of **infinite stagger resistance** (enemies or players): no hit staggers.
- Attacking a **part too hard for the attack** gives **no hitstop** but a **ricochet**: the character pulls the weapon
  back with a **montage that depends on the attack**. Spec: combat-parts.md §3.4 (ricochet band), M4 §3b (attacker
  outcome and per-attack ricochet response).

## 2f. Clarifications (owner input, 2026-10-22, before noon)

- **DI values were examples**: DI in makes the knockback more vertical, DI out more horizontal; amounts are tuning.
- **Armor and poise** are special cases of **stagger resistance** and **remaining weight**: no separate poise system.
- **Shield**, where present (**PvP, not PvE**), works **like Smash**. Some **special moves** may effectively **parry**:
  keep the parry mechanism available to any move.
- **Death**: a **KO zone** (e.g. ravines) or **health at zero**.
- **Many enemies**: research the best replication techniques for performance (docs/architecture/many-enemies-replication.md).
- **Some enemies work as solid platforms** (players can stand on them).
- Flakes coming off a part's material are **only aesthetic**; players have one part, so for them it is all aesthetic.

## 2g. Interact button, dodge windows, designer tags, Blueprint, replaceable content (owner input, 2026-10-22, 10:34 and 10:36)

- **Interact button, for now: tap = grab, hold = pick up objects** (answer to the M3 question on tap/hold). The heavy grab on
  the hold is dropped (M6 §8, M10 §8).
- **Ground dodge: the feet are invincible in the initial part** of the dodge (owner's correction at 11:41: the 10:34 message
  said "final part" by mistake). Dodges, jumps and the like are **pluggable gameplay abilities**, so changing this later is easy. In general, **which portions of the
  hurtboxes are invincible (and similar windows) is the designer's choice** (M4 §1b: `HurtboxModifier` data, never code).
- **Designers must be able to create tags for the names of weapons, kits, specials and so on** (modularity-and-tags.md §4.2b).
- **Most gameplay logic must be easy for designers to extend or change in Blueprint** (modularity-and-tags.md §3.8).
- **Extreme modularity must allow replacing content as needed** (modularity-and-tags.md §3.9).

## 2h. Hardness, stagger resistance, weight: three dials (owner input, 2026-10-22, 11:18–11:21)

- **Hardness** is the Helldivers armour value: against the attack's penetration it decides **whether damage applies** (too hard =
  no damage, ricochet).
- **Stagger resistance** decides **whether the victim enters hitstun**; hitstop can apply anyway.
- **Weight** decides **how strong the knockback is**, and so whether the victim is launched. Weight **drops as damage grows** (the
  owner's reading of Smash). This logic is **standard** and generally does not change.
- Example: hit 1 staggers without launching and does damage; hit 2's knockback, against the weight left after hit 1, is still not
  enough to launch, so the enemy is only staggered again.
- A hit's own damage does not lighten the weight that decides its own knockback; only damage taken before it counts (owner
  confirmed, 11:32).
- Consequences (M4 §3): one dial per job; the standard knockback model is weight-driven (damage makes knockback stronger only
  through the weight, never twice); the default penetration bands act on damage only (combat-parts.md §3.4).

## 3. Grabs, interaction and throwing enemies

- Example flow: a large enemy attacks; it is too big to grab. The player damages it until parts break off. Now it can
  be grabbed and thrown, e.g. **thrown at other enemies** (the thrown enemy is a projectile with hitboxes).
- **The grab button is also the general interact button.** Different meanings can come from input variations (e.g.
  holding the button), as long as the input system is flexible enough (it is: conditions such as hold/tap in
  `GSTNInput`). Decided for now (§2g): **tap = grab, hold = pick up objects**.

## 4. Impacts with surfaces

- Being launched into a surface can cause a **wall splat**, a **bounce**, or **destruction**, as the designer decides
  (per hit, per entity, per surface: *Reading*, to confirm which of these owns the choice).

## 5. Death and damage presentation: crystal people

- Players and enemies are **crystal inside, covered in human "skin"**. As health drops, **pieces fly off**: small
  cosmetic chips, or whole limbs (tied to the part-breaking system of section 1).
- Dead players and enemies **shatter into pieces**.

## 5b. Damage inside a part, detachment, players (owner input, 2026-10-22)

- The "breaking" look **inside a part** is a diegetic display of that part's health: **just a material** (shader), no
  spawned pieces.
- When a part **detaches**, it is no longer a material effect: it is a real separation (mesh, chunk, physics).
- **Players are a single part**: when it breaks, they die.
- **Parts do not regenerate by themselves** (18:22): they stay broken by default, but some cases can **restore** them,
  for example an enemy that heals others. *Reading:* restoring is an explicit gameplay action (ability/effect), never a
  timer inside the parts system.

## 6. Dodge and shield

- **No standard implementation**: dodge and shield may depend on the **kit**.
- Base PvE **ground dodge**: almost a Super Mario **long jump** or a Helldivers **dive**, but the character does not
  end lying down on landing. **Only the feet are invincible**, and **only in the initial part** of the dodge (§2g, corrected at 11:41).
  *Reading:* the feet hurtboxes are intangible (default) or invincible in that window; the rest of the body can be hit. The
  windows are designer data.
- **Air dodge**: like the **dive roll** of Super Mario 3D World.
- A **rolling movement mode** may come later: **do not implement it now**.

## 6b. Network topology: listen server (owner input, 2026-10-22)

- No dedicated servers for now: **a friend hosts (listen server), the others are clients.**
- Consequences: the host is both the authority and a player with zero latency (host advantage, to be accepted or
  mitigated); the host's machine also renders, so server-side budgets (minions, lag compensation, hit resolution)
  share the frame with the host's own game; the host's upload bandwidth (home connection) is the bandwidth limit; if
  the host leaves, the session ends (no host migration unless asked). The PIE harness should also cover a
  listen-server configuration (host + 2 clients), not only a dedicated server.
- Dedicated server support is not removed from the design, but it is no longer the reference configuration.

## 6c. Cancellable animation tails (owner input, 2026-10-22)

- Most moves and animations have an **automatic cancel window towards the end**: the final part of the return to idle can
  be **cancelled by anything**, for responsiveness. Which portion is only cosmetic is **decided by the designer**.
- **Stagger follows the same pattern**: about **half a second unskippable**, then the rest is cancellable.
- Spec: M3 `FirstActionableFrame` for timelines; M4 §7 (hitstun = locked part, reaction tail cancellable; ricochet recoil too).

## 6d. Game modes, kits, weapons, mods, implants, backpacks, depth marker (owner input, 2026-10-23)

For later implementation. Specs: [combat-loadout-mods.md](combat-loadout-mods.md) (kits in play, mods, backpacks),
[game-modes.md](game-modes.md), [depth-marker.md](depth-marker.md).

- **Two PvE modes**: a mode of **designer-made 3D levels** (Super Mario 3D World style), and a **Helldivers-style mode** where
  a **large level is generated randomly** and **missions** must be completed. **No shield in PvE** (as §2f).
- **Everything is a pluggable gameplay ability** ("in generale praticamente ogni cosa dovrebbe essere un'abilità pluggabile"):
  it makes design extremely easy and it is what games like Fortnite do. Dodges and jumps already are (§6).
- **Kit = stats + moveset.** As a technical term, the **kit** is the **stats**, which stay in force even while a support weapon
  is in use; the **weapon** ("arma") is the **moveset of the primary kit** without the support weapon. A "weapon" may be fists
  and kicks and use a sword mesh only in some animations.
- **The kit can be changed mid-match by picking one up.** Changing the primary weapon separately from the kit mid-match is
  **not planned for now**.
- **Support weapons** (Helldivers style: a big extra weapon to switch to when needed) **inherit the kit's stats**.
- **Kits can be modded**: mods change the look (cosmetic changes by the mods chosen) and can affect **special moves and the base
  moveset**. Example: a mod that makes the sword longer but some attacks slower. **Special moves can be swapped** among the
  available ones.
- **Kit visuals: an "implant"**, an extra mesh on the character's back (or anything that shows which kit is in use). A weapon can
  be a sword plus implants reaching the hands or the feet.
- **Backpacks** exist, as in Helldivers 2. They are **modular and attach on top of the kit implant** through a standard
  connector, like the standard port of an electronic device.
- **Depth-perception marker**: as in the Smash Bros Melee 3D mod, a **decal or marker pointing at the platform under the
  character plus a dashed line**; **some enemies** have it too. Which surface it should point to **while airborne is ambiguous**
  and needs design work. The final look (e.g. a shadow cone, something more diegetic) is the designer's choice. Reference
  screenshots: a filled disc on the floor under a character above the stage; off stage, over the void and below the stage's
  level, a hollow ring at about the stage's height joined to the character by a vertical dashed line.

## 7. Consequences for the specifications

| Topic | Affects |
|---|---|
| Parts, hardness, penetration, per-part health, transfer to main pool, part break events, capped percent | new spec [combat-parts.md](combat-parts.md) (own plugin); M4 resolution pipeline; M3 attributes; M8 minions |
| Weight from remaining health, stagger and juggle factors, part-dependent hitstun/hitstop | M4 knockback model and victim side; M8 |
| §2b: hitstun only if the hit's **stagger force** reaches the threshold of the part hit; stagger force derived from the attack's power or set by the designer; hitstop automatic (damage and knockback) or per attack; weight decides the reaction tier (launch, push back, stagger in place) | M4 hit data, knockback model, hurtbox thresholds and victim side ([combat-m4-hits.md](combat-m4-hits.md) §3, §7); per-part `StaggerThreshold` and `MaxReaction` ([combat-parts.md](combat-parts.md) §3.1, §4.4); M8 minions (same rules, tier encoded in `ActionState`, [combat-m8-minions.md](combat-m8-minions.md) §4d.2, §8.2) |
| Grabbability depending on size and broken parts; grab = interact; throwing enemies at enemies | M6 grabs; M10 thrown objects (enemies as projectiles); M8 |
| Wall splat / bounce / destruction on impact | M4 (launch and tumble), M6 tech interaction |
| Crystal shattering, flying chips and limbs | new spec combat-parts.md (presentation side), M4 hit presentation |
| Kit-dependent dodge and shield; base dodge = long jump / dive with foot-only invincibility; air dive roll | M3 movement abilities (kit data), M4 dodges, M6 shield |
| §6d: two PvE modes (designer levels, procedural expedition with missions); kit = stats, weapon = moveset; mid-match kit pickup; support weapons inherit kit stats; mods (cosmetic + moveset/specials); special swapping; implants; backpacks on the implant connector; depth marker; everything a pluggable ability | new specs [combat-loadout-mods.md](combat-loadout-mods.md) (M11), [game-modes.md](game-modes.md) (M12–M13), [depth-marker.md](depth-marker.md); M3 seams (loadout extensions, compile extension, mid-match apply); M4 hit data scalable by mods; M5 mod preview and validation; M6 interact probe (possible `GSTNInteract` plugin); [modularity-and-tags.md](modularity-and-tags.md) §1.1 goal 9, §3.6, §3.10, §4.2 |
| §2h: hardness → damage, stagger resistance → hitstun, weight → knockback strength; weight drops with damage; standard logic | M4 §3 (`WeightDriven` model, three-dials table, standard `HealthWeight`, standard rules asset thresholds), [combat-parts.md](combat-parts.md) §3.4 (bands on damage only), §4.1, M8 §3 |
| §2g: tap = grab, hold = pickup; feet invincible in the dodge's initial part (corrected at 11:41), invulnerability windows designer-owned; designer identity tags; Blueprint extension; replaceable content | M6 §8, M7 §7, M10 §8; M4 §1b, §3b; [modularity-and-tags.md](modularity-and-tags.md) §1.1, §3.8, §3.9, §4.2b, §5b, §6.1 rule 12, §7.1 |
| §2f, §6b: many enemies on a listen host (replication, presentation and host budgets) and enemies as solid platforms (players stand on them) | research [many-enemies-replication.md](many-enemies-replication.md); M8 minions ([combat-m8-minions.md](combat-m8-minions.md) §4d.1, §4d.5, §8, §11: packs with push model and dormancy, per-item tier rates, 150 as a population with about 60 awake, `MaxVisiblePuppets`, platform seams); new optional plugins `GSTNPlatform` and bridge `GSTNPlatformMinion` ([modularity-and-tags.md](modularity-and-tags.md) §3.6, tag root `Platform.*` §4.2) |

## 8. Out of scope for now

- Rolling movement mode (section 6).

## 9. Original message (verbatim)

> Ti darò alcuni insights sul mio ragionamento per alcuni comportamenti previsti nel gioco, per darti alcune idee.
> Risponderò poi, per bene al resto delle domande:
>
> 1. Nemici possono avere diversi punti che influiscono sulla vita in modo diverso. Cosa vuol dire? In pratica il
> funzionamento sarebbe molto simile a helldivers 2. Quindi durezza delle parti (e "penetrazione" dell'hitbox
> corrispondente per vedere se la parte può essere distrutta). Vita che dipende in base al dano delle parti, punti
> deboli che sono a durezza bassa ma che hanno alto danno durabile etc…
>
> Considera che i nemici dovrebbero anche avere un "peso" associati ai loro HP rimanenti (per questo ti ho detto HP è
> percentuale insieme, in pratica è come una percentuale, ma dopo un tetto massimo, causa la distruzione della parte,
> o del nemico).
>
> Esempio: Nemico 1 attacca. È grande, non posso grabbarlo. Riesco a danneggiarlo così tanto da far saltare via delle
> parti. Adesso posso grabbarlo e tirarlo. Ad esempio, posso tirarlo addosso agli altri nemici.
>
> Per quanto riguarda gli impatti: essere lanciato contro una superficie potrebbe causare wall splat, o bounce, o
> distruzione, a seconda di cosa decide il designer.
>
> Per quanto riguarda la morte: I giocatori e i nemici morti andranno in pezzi. (L'idea era renderli uomini di
> cristallo all'interno, rivestiti di "pelle" umana. Quindi sia ai nemici, che ai giocatori, al diminuire della vita,
> dovranno saltare via dei "pezzi". Piccoli pezzi estetici, oppure interi arti (di riagganciamo al "rompere le
> parti")).
>
> Per quanto riguarda la schivata e lo scudo. Non ho un'implementazione standard, potrebbe dipendere dal Kit.
> L'implementazione base della schivata nella modalità PVE è quasi un "long jump" di super Mario o un tuffo di
> helldivers, senza andare sdraiato, però, quando si atterra. Solo i piedi saranno invincibili). La schivata in aria
> sarà alla stregua, un dive roll alla super Mario 3d world. (La modalità di movimento rolling potrebbe essere
> implementata in seguito, ma non implementarla per ora)
>
> Per quanto riguarda L'interazione: il tasto della Grab sarà anche il tasto d'interazione, in generale. Non dovrebbe
> essere un problema giocare con i valori di input (come ad esempio, tenere premuto il tasto), nel caso in cui il
> sistema di input sia progettato in modo abbastanza flsssibile.
>
> Nemici, come in helldivers, dovrebbero avere un fattore di stagger e juggle (il peso per il knockback di cui parlavo
> prima) dove hitstun e hitsop dipendono da cosa e dove vengono colpiti.

### Second message (2026-10-22, verbatim)

> Aggiungo delle cose: Se un nemico o un boss entra in hitsuno no, dipende dalla parte colpita. Ogni parte dovrebbe
> avere una condizione di hitsun(generalmente se l'attacco ha un parametro, chiamato forza di stagger sopra un certo
> totale. Generalmente scala con la potenza dell'attacco, ma potrebbe essere settato in modo diverso). Hitstop puo
> venire calcolato dinamicamente in base al danno e al knockback dell'attacco, ma puo anche essere scelto a parte dal
> designer, se vuole.
>
> Ricordiamo che il peso è il valore che decide quanto un nemico vola via quando entra in hitstun, oppure se non vola
> via affatto ma viene solo staggerato sul posto, oppure mandato all'indietro.

### Third and fourth messages (2026-10-22, verbatim)

> Inoltre in genere no, non si può schivare durante l'hitstun o l'hitstop. L'SDI avviene tenendo premuta la direzione in
> cui si vuole driftare.
>
> Per quanto riguarda la DI, ti mando delle ipotesi di funzionamento, trova una formula valida a partire da queste:
>
> Se vengo lanciato in diagonale avanti diciamo con un dot si 0.4 rispetto all'up vector, e tengo premuto lo stick lo
> stick verso di lui, andrò lanciato con in diagonale verso l'alto, con un dot di 0.6. Se invece premo verso la direzione
> in cui vengo lanciato (DI out), il dot rispetto all'up vector potrebbe diventare 0.4.
>
> Se vengo lanciato direttamente in orizzontale (dot 0.0) premere in direzione interna (DI In) dovrebbe risultare in un
> vettore di lancio con dot di 0.2 ad esempio con l'up vector, mentre premere in avanti non risulta in nessun cambiamento
> di dot.
>
> La situazione è speculare nel caso in cui il dot con il down vector sia >0. DI in, dot ancora più alto (con il down
> vector), di out, dot ancora più basso (con il down vector).
>
> Nel caso dot> -0.1 ad esempio, ci si comporta come nel primo caso (dot con up vector).
>
> Premere lo stick lateralmente rispetto alla direzione del lancio, causa l'angolo di lancio di ruotarne attorno all'
> asse dell' up vector

> Ricorda che la gravità potrebbe cambiare arbitrariamente.
>
> Unrelated, ma tieni conto della presenza di eventuali trampolini

> Per quanto riguarda la "super armor" avrai capito che si tratta del caso speciale in cui la resistenza allo stagger da
> parte del nemico o giocatore è infinita. Per quanto riguarda la durezza delle parti, se si prova ad attaccare una
> parte troppo dura rispetto all'attacco, non si avrà un hitstop, ma un effetto di "ricochet" del colpo, ossia il
> personaggio ritira indietro l'arma con un montage che cambia a seconda del di attacco

> Per quanto riguarda l'effetto visivo di "rottura". All'interno di una parte, è un modo diegetico di far vedere la sua
> vita, un semplice materiale. Se una parte si stacca, quello non è più un semplice materiale/shader. I giocatori sono
> semplicemente una parte, quindi, quando si rompono muoiono.
>
> Per quanto riguarda la domanda che mi hai appena fatto, tieni a mente che la maggior parte delle mosse e animazioni
> dovrebbe avere una finestra di cancel automatico verso la fine (ossia la parte finale dell'animazione di ritorno a
> "idle" dovrebbe poter essere cancellata da qualsiasi cosa, in modo da dare un senso di reattività. Ovviamente, quale
> porzione di animazione è solo estetica, è decisa dal designer). Lo stagger segue questo pattern, ossia per circa mezzo
> secondo l'animazione è unskippable, mentre la parte finale è cancellabile.

> Per quanto riguarda I valori della DI, erano esempi, cioè DI IN rende il knockback più verticale, DI OUT più orizzontale.
>
> Per quanto riguarda armor e poise, abbiamo visto che sono casi speciali della resistenza allo stagger e del peso
> rimanente, no?
>
> Per quanto riguarda lo scudo, ove presente (in PVP, non in PVE), funziona come in smash. Potrebbero essere presenti delle
> mosse speciali che effettuano parry in pratica, ricordati questo aspetto.
>
> Condizione di morte: zona di KO (E.g. burroni) oppure vita a zero.
>
> Inoltre ricerca le migliori tecniche per gestire la replicazione etc… di molti nemici in modo da essere performanti.
>
> Alcuni nemici funzionano come piattaforme solide.
>
> I pezzi del material che si staccano di una parte sono solo estetici, i giocatori hanno solo una "parte" quindi, puoi
> dedurre che sia estetico.

### Fifth and sixth messages (2026-10-22, 10:34 and 10:36, verbatim)

> Ti do la risposta alla domanda 6: per ora (raccogliere oggetti richiede il tenere premuto, grabbare il tap). 5. Per ora
> lascia i piedi invincibili nella parte finale, ma le porzioni delle hurtbox invincibili e quant altro dovrebbero poter
> essere lasciate al designer, in generale. Per quanto riguarda le gameplay tag, beh devono poter essere create tag
> appropriate per i nomi delle armi, dei kit, delle speciali, etc… Ricorda che la maggior parte delle logiche gameplay
> devono poter essere estese o modificate dal designer in modo agevole tramite BP.

> La modularità estrema deve permettere di rimpiazzare contenuti a seconda delle esigenze.

### Seventh to ninth messages (2026-10-22, 11:18–11:21, verbatim; the 11:20 one as edited at 11:21)

> Aggiungo dei dettagli: la possibilità di lancio dei nemici dipende dal knockback e dal loro peso. Abbiamo detto che è possibile
> che un nemico perda peso all'aumentare del danno preso (comportamento simile a smash). La logica è standard e in generale non
> cambia. Se il primo colpo staggera (manda in hitstun senza lanciare), il nemico perde vita, ma il knockback del secondo colpo
> non è sufficiente a lanciarlo in base al nuovo peso dopo il danno del primo colpo, allora il nemico sarà solo staggerato.

> In pratica: la durezza regola la possibilità di applicare il danno o no.
> La resistenza allo stagger determina la possibilità di entrare in hitstun o no, (potrebbe entrare in hitstop ugualmente),
> Il peso determina il la forza del knockback.
>
> In smash è il peso che diminuisce all'aumentare del danno, ad esempio.

> La durezza sarebbe il valore di armatura di helldivers, in pratica

### Tenth message (2026-10-22, 11:41, verbatim)

> Comunque, solo la parte iniziale della schivata ha i piedi invincibili, mi sono sbagliato prima. Ricorda che schivate, salti
> etc… sono abilità di gameplay pluggabili, come avevamo già concordato. Quindi cambiare idea dopo dovrebbe essere facile.

### Messages of 2026-10-23 (verbatim)

> Aggiungo altri dettagli di cui fare l'implementazione poi. Ci dovrà essere una modalità a livelli scelti dal designer (livelli in
> 3d, stile Mario 3d world), e una modalità alla helldivers dove si genera casualmente un grande livello e bisogna terminare delle
> missioni.
>
> Nella modalità PVE non c'è lo scudo.
>
> I kit, come ti ho già detto sono un'unione di statistiche e moveset. Si può cambiare kit a metà partita raccogliendone uno.
>
> Se si usa un'arma di supporto (alla helldivers, un'arma grossa aggiuntiva, a cui si può passare quando serve), vengono ereditate
> dal kit.
>
> Per ora non prevedo il supporto al cambiamento di arma primaria separata dal kit a metà partita.
>
> Aggiungo che dovrebbe essere possibile moddare il kit (con cambiamenti estetici in base alle mod scelte) e cambiare mosse
> speciali tra quelle disponibili. Cambiamenti alle mod potrebbero influire su alcune mosse speciali come sul moveset base. Per
> esempio, una mod che aumenta la lunghezza della spada, ma rende alcuni attacchi più lenti.
>
> Quando mi riferisco al "kit" come termine tecnico mi riferisco a statistiche che vengono mantenute in generale anche se si
> adopera un'arma di supporto.
> Quando parlo di "arma" come termine tecnico mi riferisco al moveset del kit primario senza arma di support.
>
> Anche se lo chiamo "arma" in realtà potrebbe benissimo avere pugni e calci, e usare magari una spada (la mesh nelle animazioni)
> solo qualche volta.
>
> Il kit dovrebbe essere indicato da una mesh aggiuntiva posta nella schiena del personaggio (o insomma, qualcosa che fa vedere che
> si sta usando un determinato kit), una sorta di "impianto". L'arma può essere una spada + degli impianti che vanno fino alle mani
> o ai piedi.
>
> Inoltre: per evitare problemi con la percezione di profondità, sarebbe buono adottare un sistema alla smash bros melee 3d (la
> nuova mod uscita), dove c'è un decal, o qualcosa che punta alla piattaforma sotto il personaggio, più una linea tratteggiata.
> Anche alcuni nemici dovrebbero averla. Il modo in cui calcolare esattamente quale è la superficie a cui dovrebbe puntare il
> decal quando si è a mezz'aria però è ambiguo, quindi ci si dovrebbe pensare un po. Ovviamente, l'effetto estetico finale
> potrebbe anche essere, ad esempio un cono d'ombra, o qualcosa di più diegetico che il designer scioglierà alla fine.

> Ah, inoltre, è prevista la presenza di Zaino come in helldivers 2.

> Potrebbero essere semplicemente modulari, attaccandosi diciamo, come un attacco standard di qualche dispositivo elettronico

> Sopra il kit.

> Sì, in generale praticamente ogni cosa dovrebbe essere un'abilità pluggabile.

> Rende il design estremamente facile, in più è anche quello che avviene in giochi come Fortnite
