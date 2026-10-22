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

## 3. Grabs, interaction and throwing enemies

- Example flow: a large enemy attacks; it is too big to grab. The player damages it until parts break off. Now it can
  be grabbed and thrown, e.g. **thrown at other enemies** (the thrown enemy is a projectile with hitboxes).
- **The grab button is also the general interact button.** Different meanings can come from input variations (e.g.
  holding the button), as long as the input system is flexible enough (it is: conditions such as hold/tap in
  `GSTNInput`).

## 4. Impacts with surfaces

- Being launched into a surface can cause a **wall splat**, a **bounce**, or **destruction**, as the designer decides
  (per hit, per entity, per surface: *Reading*, to confirm which of these owns the choice).

## 5. Death and damage presentation: crystal people

- Players and enemies are **crystal inside, covered in human "skin"**. As health drops, **pieces fly off**: small
  cosmetic chips, or whole limbs (tied to the part-breaking system of section 1).
- Dead players and enemies **shatter into pieces**.

## 6. Dodge and shield

- **No standard implementation**: dodge and shield may depend on the **kit**.
- Base PvE **ground dodge**: almost a Super Mario **long jump** or a Helldivers **dive**, but the character does not
  end lying down on landing. **Only the feet are invincible.** *Reading:* the feet hurtboxes are intangible or
  invincible during the dodge; the rest of the body can be hit.
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
