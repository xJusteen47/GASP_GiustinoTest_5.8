# Combat M4 — Hits (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5.3, §12 and §15 there, and the M1–M3 specifications.
Section numbers below are local to this file.

> **Revision 2026-10-18:** hits publish authoritative events (`Combat.Event.HitLanded`, `HitReceived`, `KO`) through
> the outbox of combat.md §5.6 / M2 §7b, tagged with command, move, hit type and relationship.

> **Revision 2026-10-19 (combat abilities):** "move" in this file means a **timeline ability**
> (`UGSTN_GA_BaseCombat` and its timeline, combat.md §11); type names follow M3.

> **Revision 2026-10-22 (owner direction, [game-design-direction.md](game-design-direction.md), wins over this file):**
> (1) seams that [combat-parts.md](combat-parts.md) (M4b, plugin `GSTNParts`) needs, so `GSTNCombat` works without it (§3b);
> (2) knockback weight from remaining health, stagger and juggle factors, hitstun/hitstop by what hits and where (§3);
> (3) surface impacts: wall splat, bounce, destruction, by designer data (§7b); (4) dodges are **kit-dependent**: base PvE
> ground dodge = long-jump dive with feet-only intangibility, air dodge = dive roll; no roll/spot dodge, no rolling movement
> mode (§1b); (5) the reference topology is a **listen server**, not a dedicated one (§9b); (6) SDI is M6's, M4 ships DI only (§7).

## 1. Scope

**Goal:** moves hit. Hitboxes from move tracks overlap hurtboxes; the attacker predicts its own hitstop and
hit-confirm (cancel windows OnHit/OnBlock become live); the victim, on the machine that simulates it, resolves the hit
through the pipeline: damage, meter, knockback with DI, hitstun, tumble, hitstop with a real freeze (also on walls).
Teams and friendly-fire variants. Lag compensation for remote attackers. Hit presentation on interpolated victims.
Knockback that depends on remaining health and on stagger/juggle factors, and the impact of a launched entity on a surface
(§7b).

Also in: the **base dodges** (§1b), now that intangibility exists, and the **generic seams** of §3b.

Out: shield, parry, links and grabs, tech, SDI (all M6: DI is the only victim influence in M4, §7); piggyback (M7); minions
(M8); editor gizmos (M5); destructible body parts, penetration and hardness (M4b, `GSTNParts`: M4 only provides the seams of
§3b); a rolling movement mode (owner: not now).

## 1b. Dodges (kit-dependent)

Owner direction: there is **no standard dodge**; it depends on the kit. M4 therefore ships no dodge system, only the two
**base PvE dodges** as ordinary timeline abilities in the default kit (data, `GSTN_Kit_*`), built on mechanisms that already
exist: a movement track (M3 `FGSTN_LayeredMove_MoveTrack`) and a `HurtboxModifier` window.

| Dodge | Behaviour (base PvE kit) | Intangibility |
|---|---|---|
| Ground dodge | Long jump / dive: a committed forward leap with an arc, **ends standing** (a short landing recovery, no lying-down get-up state) | **Only the feet hurtboxes** (`Combat.Hurtbox.Feet`) are `Intangible` for the window; the rest of the body can be hit (*Reading*: intangible rather than invincible, so a hit on the feet does not even register) |
| Air dodge | Dive roll, as Super Mario 3D World: a forward air burst with a tuck animation, then normal falling | Not specified by the owner: *Proposal*: same rule as the ground dodge (feet only) until the owner says otherwise |

- The "roll" is an **animation and ability**, queued as a normal move through Mover (rule 2 of CLAUDE.md); **no rolling
  movement mode is implemented** (owner, game-design-direction.md §6/§8). Spot dodge and a generic roll are removed from M4.
- The `HurtboxModifier` track selects hurtboxes by name (`AffectedHurtboxes`) **or by tag** (`AffectedTags`, new, so a kit's
  dodge does not depend on another set's names); the foot group is `Combat.Hurtbox.Feet` in the hurtbox set.
- Other kits (heavy: armoured step, caster: blink) are only content: another timeline ability with another `HurtboxModifier`.
  Nothing in `GSTNCombat` assumes a particular dodge. M6's tech roll is a separate thing (tech ability, M6 §6).


## 2. Spikes first (record results in combat.md §17)

1. **Cross-entity cost**: client 1's pawn hits client 2's pawn (server-resolved); measure client 2's rollbacks per hit
   and the visible delay on client 1 (presentation off). Expect about one rollback per hit on client 2.
2. **Freeze**: `FGSTN_LayeredMove_Freeze` (OverrideAll, zero velocity) in Walking, Falling and each project wall mode,
   with forced reconciles inside the freeze. Confirms how a layered move exposes a Mover gameplay tag (check
   `UMoverComponent::HasGameplayTag` sources: modes, layered moves, modifiers); if layered moves cannot carry tags, the
   freeze also queues a `FGSTN_FrozenModifier` that does.
3. **Hitstop with a montage playing**: the M0 pause path with a real montage, forced reconcile in the window.
4. **Interpolation buffer**: lower `FixedTickInterpolationBufferedMS` (100 ms today) step by step under 60/15 ms and
   2 % loss and keep the smallest value without visible stutter of interpolated proxies (spike S-FP showed this buffer
   is most of the delay of remote actions, combat.md §12.7). Record it in combat.md §17.

## 3. Core types (`GSTNCombatCore`)

**Shapes** — `FGSTN_HitShape { EGSTN_ShapeType Type (Sphere/Capsule/Box); FVector Extent (radius / radius +
half-height / half-extents); FTransform Local; }`. `UGSTN_HitShapeUtils` (stateless): `Overlap(A, AWorld, B, BWorld)`
for every pair (sphere–sphere, sphere–capsule, capsule–capsule, sphere–box, capsule–box, box–box via SAT); and
`SweptOverlap` that tests `N` sub-steps between a hitbox's previous and current world transform (`bSweepBetweenFrames`
on the hitbox, N = ceil(distance / smallest extent), capped at 4) so fast hitboxes do not tunnel. All math in doubles,
results identical on every machine for identical inputs.

**Hit data** — `FGSTN_HitData { float Damage; float BaseKnockback; float KnockbackGrowth; float FixedKnockback (≥ 0
overrides); float AngleDeg; EGSTN_SpecialAngle SpecialAngle (None / Sakurai3D / AutoLink); EGSTN_YawSource YawSource
(AttackerFacing / HitboxToVictim / HitboxVelocity); float HitstunMultiplier; float HitstopMultiplier; float
AttackerHitstopMultiplier; float ShieldDamage; int32 Priority; FGameplayTag HitType (Combat.HitType.Normal/Grab/
Projectile/Unblockable); TArray<TSubclassOf<UGameplayEffect>> ExtraEffects (applied to victim); FGameplayTag CueTag;
FInstancedStruct CustomData; float StaggerPower = 1; FGSTN_ImpactProfile Impact (§7b);
TArray<FInstancedStruct> ExtensionData (base struct FGSTN_HitExtension, modularity-and-tags.md §5b; empty by default); }`.
`StaggerPower` is the "what hits" input of the stagger factor (§3 knockback model).

**Hitbox** — `FGSTN_HitboxTrack : FGSTN_TimelineTrack { TArray<FGSTN_HitboxSpec> Hitboxes; }`, `FGSTN_HitboxSpec {
FGSTN_HitShape Shape; EGSTN_HitboxSpace Space (Root/BakedSocket/WorldAtMoveStart/Projectile); FName Socket;
TArray<FGSTN_HitboxKey> Keys (frame, offset, rotation, scale; linear interpolation); uint8 HitGroup; bool
bSweepBetweenFrames; FGSTN_HitData Data; TMap<FGameplayTag, FGSTN_HitVariant> RelationshipVariants; }`.
`FGSTN_HitVariant { bool bIgnore; float ShapeScale = 1; FGSTN_HitDataOverride Override (per-field optional values and
multipliers); }`. `BakedSocket` uses `FGSTN_CombatTimelineData::BakedSockets (TMap<FName, TArray<FTransform>>)` — one
root-relative transform per frame, written by the M5 baking tool; in M4 a Python helper bakes them (section 10).

**Hurtboxes** — `UGSTN_HurtboxSetDefinition { TArray<FGSTN_Hurtbox { FName Name; FName Bone; FGSTN_HitShape Shape
(root-relative, baked from the reference pose); FGameplayTagContainer Tags (e.g. Combat.Hurtbox.Head, .Feet); uint8 Priority = 0;
FGSTN_HitReactionModifiers ReactionScale }> }`. `Priority` and the array index decide which hurtbox a hit lands on when several
overlap (§6 step 4); `ReactionScale` is the "where" input of hitstun/hitstop/knockback scaling for combat without parts (§3).
`FGSTN_HurtboxModifierTrack : FGSTN_TimelineTrack { EGSTN_HurtState State (Normal/Intangible/Invincible/Armor); float
ArmorThreshold; TArray<FName> AffectedHurtboxes (empty = all); FGameplayTagContainer AffectedTags (matches `FGSTN_Hurtbox::Tags`; union
with the names); TArray<FGSTN_HitShape> ShapeOverrides; }`.
Intangible = not hit at all; Invincible = hit registers (attacker hitstop, confirm) but no damage/knockback/hitstun;
Armor = no knockback/hitstun below the threshold, damage applies.

**Knockback model** — `UGSTN_KnockbackModel` (abstract, EditInlineNew, const methods): `Compute(const
FGSTN_KnockbackInput&) → FGSTN_KnockbackResult { FVector LaunchVelocity; int32 HitstunFrames; bool bTumble; int32
HitstopFrames; int32 AttackerHitstopFrames; }`. `FGSTN_KnockbackInput` also carries the victim's `HealthFraction` (derived, §3b) and
`FGSTN_HitReactionModifiers Reaction`. Default `UGSTN_KnockbackModel_PercentScaling`: knockback grows with the
victim's damage percent and the hit's damage, scales inversely with weight, plus base and growth (the commonly
documented platform-fighter formula, every constant exposed as a property); hitstun = knockback × `HitstunFactor`;
tumble above `TumbleThreshold`; hitstop = `floor(Damage × HitstopPerDamage + HitstopBase)` clamped to `MaxHitstop`,
× the hit's multipliers. `UGSTN_KnockbackModel_Fixed` (HP games): knockback = base, no percent.
`UGSTN_KnockbackModel_CappedPercent` (owner direction, [combat-parts.md](combat-parts.md) §4.1): the percent term is
`DamagePercent` as is, used with `DamageMode::PercentCapped` (below).

**Weight, stagger and juggle (owner §2).** Every model applies the modifiers of `Ctx.Reaction` exactly once, in this order:
`Weight × WeightScale`, knockback magnitude, `× KnockbackScale`, hitstun from the knockback `× HitstunScale`, hitstop `×
HitstopScale`; `bNoFlinch` keeps damage and hitstop but gives no hitstun or launch (the Armor semantics). The modifiers are
written by steps, never by the model, so the model stays one pure function:

- `FGSTN_HitReactionModifiers { float WeightScale = 1, KnockbackScale = 1, HitstunScale = 1, HitstopScale = 1; bool bNoFlinch =
  false; }` (`GSTNCombatCore`; defaults are the identity, so unextended content is unchanged).
- **Weight that depends on remaining health**: an optional generic step `HealthWeight` (before `ComputeKnockback`) writes `WeightScale = Curve(Progress)`
  from a baked curve (`FGSTN_BakedCurve`, 64 `uint16` entries, integer interpolation, so every machine reads the same number;
  default 1.0 at 0 % falling to 0.6 at the cap: a battered enemy flies further). `Progress = DamagePercent / MaxHealth`
  under `PercentCapped`, `1 − Health / MaxHealth` under `Health`. With `GSTNParts` the `PartResolve` step **replaces** it (it
  adds lost mass of broken parts): the rules validator reports both in one pipeline.
- **Stagger / juggle by what hits and where**: "what" = `HitstunMultiplier`, `HitstopMultiplier`, `StaggerPower` of the
  hit data; "where" = the `ReactionScale` of the hurtbox the hit landed on (a `HurtboxReaction` step multiplies it into the
  modifiers; **hurtbox reaction and part reaction of GSTNParts must not both be set** on the same hurtbox: validator).
  `UGSTN_CombatRules::StaggerThreshold` (default 0 = off), `MinStagger`, `MaxStagger`: a `StaggerPower × hurtbox factor` below the
  threshold sets `bNoFlinch`; above it `HitstunScale` = that value clamped. The juggle factor of a victim already in
  `Tumble` is `AirborneJuggleScale` on the rules (default 1), the knob that allows or limits long juggles. Direction: yaw from
`YawSource` on the movement plane, pitch from `AngleDeg` (`Sakurai3D`: low angle on grounded victims below a knockback
threshold, higher in the air; `AutoLink`: towards the hitbox's velocity).

**DI** — `UGSTN_HitMathUtils::ApplyDI(Launch, StickOnPlane, MaxDIDegrees)`: rotate the horizontal component of the
launch around world up by `MaxDIDegrees × (component of the stick perpendicular to the launch direction)`; vertical
component unchanged; DI on the plane only, as decided.

**Resolution pipeline** — `UGSTN_HitResolutionStep` (abstract, EditInlineNew, const): `EGSTN_StepResult
Resolve(FGSTN_HitResolutionContext& Ctx) const` (Continue/Stop). `Ctx` holds the incoming hit, the victim's state at
end of F-1 (combat state, attributes snapshot, tags), and the outcome being built `{ bHit, bBlocked, bParried, bClank,
Damage, Knockback result, MeterGains, Events (tags), Effects to apply }`. Steps in M4: `TeamFilter`, `HurtState`
(intangible/invincible/armor), `HurtboxReaction`, optional `HealthWeight`, `ComputeKnockback` (calls the model), `ApplyDamage` (adds
`Outcome.Damage` through the damage GE), `Hitstop`, `MeterGain`. Placeholders for M6 (`Shield`, `Parry`) and M4b
(`PartResolve`, after `Shield` and before `ComputeKnockback`) are documented, not implemented. The context also holds
`FGSTN_HitReactionModifiers Reaction`, `uint8 HurtboxIndex` and `TArray<FInstancedStruct> OutcomeExtensions` (§3b).

## 3b. Generic seams for optional plugins (acceptance items of M4)

`GSTNCombat` must build, run and pass its tests with `GSTNParts` (and every other combat extension) absent. M4 therefore
lands these generic, inert-without-a-provider seams, each reviewed as a combat change in its own commit
([combat-parts.md](combat-parts.md) §1 lists them; they use the extension mechanism of modularity-and-tags.md §5b):

1. **Context modifiers and outcome extensions**: `Ctx.Reaction` (`FGSTN_HitReactionModifiers`) and `Ctx.OutcomeExtensions`
   (`TArray<FInstancedStruct>`); `PercentScaling`, `HealthFraction` (M8), `CappedPercent` and `Fixed` models read the modifiers.
2. **`HurtboxIndex`** (`uint8`, which hurtbox the overlap picked) on the victim inbox entry and on the context. Rule (*Proposal*,
   the same as combat-parts.md §3.4): among overlapped, non-intangible hurtboxes pick the highest `Priority`, ties by lowest
   index. The attacker stores it, the victim never re-derives it; lag compensation already rewinds the transforms.
3. **Outcome appliers**: `IGSTN_OutcomeExtensionApplier` (registered by type), called when the combat tick applies the outcome
   (§7) for each `OutcomeExtensions` entry whose struct type it owns. State written there is state at F+1, so rollback restores
   it with the combat state.
4. **`DamageMode::PercentCapped`** and `UGSTN_KnockbackModel_CappedPercent` (§3).
5. **`FGSTN_HitData::ExtensionData`** and its per-field override variant (`FGSTN_HitDataOverride` extension entry), so a
   relationship variant can change an extension's fields.
6. **Tick-extension point "before candidates"** may return a `HitGroupMask` that masks the entity's own active hitbox
   groups (§6 step 1).
7. `FGSTN_HitVictimView` and the snapshot-extension POD of M8 §3 (if M8 lands first they exist already).

Without a provider every seam is the identity: no extension, modifiers at 1, mask empty. Automation: a pipeline with fake
steps that write modifiers and an applier that records calls, run with no plugin present.

**Combat rules** — `UGSTN_CombatRules` (DataAsset): `Pipeline (TArray<UGSTN_HitResolutionStep*> Instanced)`,
`KnockbackModel (Instanced)`, `DamageMode (Percent/Health/PercentCapped)` (`PercentCapped`: `MaxHealth` is the cap in percent points; `DamagePercent` is
clamped at it and reaching the cap raises `Combat.Event.KO`; `Health` unused; owner §2), `bFriendlyFire`, `HittableRelationships`
(`FGameplayTagContainer`), `RelationshipResolver (Instanced)`, `bUseLagCompensation`, `MaxRewindMs`, DI tuning (SDI: M6), `StaggerThreshold`/`MinStagger`/`MaxStagger`/`AirborneJuggleScale`, `RulesExtensions`
(`TArray<FInstancedStruct>` of base `FGSTN_RulesExtension`, §5b).
Selected per game mode (`AGameModeBase` subclass property or a GameState component `UGSTN_CombatRulesComponent`,
replicated by asset reference).

## 4. GAS side (`GSTNCombatGAS`)

- **`UGSTN_CombatAttributeSet`**: `Health`, `MaxHealth`, `DamagePercent`, `Poise`, `MaxPoise`,
  `OutgoingDamageMultiplier`, `IncomingDamageMultiplier`, `KnockbackResistance`, `HitstunResistance`.
- **`UGSTN_GE_Damage`** (instant) with **`UGSTN_DamageExecution`**: reads SetByCaller `Combat.SetByCaller.Damage`, the
  instigator's outgoing and the target's incoming multipliers; writes `DamagePercent` (+) or `Health` (−) by
  `DamageMode`.
- **`FGSTN_HitEffectContext`** (`FGameplayEffectContext` subclass with `NetSerialize`): hit data id (move + hitbox
  index), hit location, relationship; used by cues.
- Meter gain GEs (`UGSTN_GE_MeterGain`, SetByCaller per meter).

## 5. World subsystem and snapshot (`GSTNCombatNP`)

- **`UGSTN_CombatWorldSubsystem`**: registry of hittable entities `{ int32 EntityId (NP proxy id); TWeakObjectPtr
  component; FGenericTeamId Team; const UGSTN_HurtboxSetDefinition*; }` (registration on BeginPlay, removal on EndPlay,
  sorted by id).
- **Per-step snapshot**: `{ int32 Frame; per entity: transform and velocity at end of F-1 (tracker sync state), facing,
  hurt state and active hurtbox overrides from its combat sync state at end of F-1, team, `bInHitstop`, action state }`
  plus a uniform-grid broadphase (cell size = 2 × the largest hurtbox extent, default 400 cm).
- **Built by the first combat tick of a step**: a combat tick at frame F rebuilds the snapshot when `Snapshot.Frame !=
  F` or the snapshot was invalidated. Every combat component's `RestoreFrame` (called on rollback for every instance)
  invalidates it, so a re-simulated frame never reuses a snapshot built before the rollback. At the first tick of a step
  no combat instance has ticked F yet, so every entity's state read is end of F-1 (fact 8 of combat.md §2).
- Entities that do not tick on this machine (interpolated proxies) contribute their interpolated presentation state:
  on a client this is the attacker's view of the world, which is what it predicts against.

## 6. Attacker side (combat tick at F)

1. Active hitboxes of the current move frame (paused frames excluded; groups in the `HitGroupMask` given by the tick-extension
   point "before candidates" are skipped: no hit, no hitstop, no confirm) → world shapes: `Root` = end-of-F-1 actor
   transform × local key; `BakedSocket` = transform × baked frame; `WorldAtMoveStart` = `MoveOriginTransform` (new sync
   field, quantized, set when the move starts); `Projectile` = owned by the projectile system (later).
2. Broadphase → candidates (excluding self unless the variant for `Self` exists).
3. **Relationship** first (resolver: self / same team = Ally / `FGenericTeamId::NoTeam` = Neutral / else Enemy), then
   `HittableRelationships` and `bFriendlyFire`, then the variant (`bIgnore`, `ShapeScale`).
4. Overlap (swept if flagged) against the target's hurtboxes with its overrides; `Intangible` skipped; the hurtbox that wins
   the `Priority`/index rule of §3b becomes `HurtboxIndex`.
5. **Hit groups**: `HitTargets` ring in the combat sync state (8 entries `{ EntityId, TimelineStartFrame, HitGroup }`) — a
   target is hit once per group per move execution.
6. For each new hit: record it; **predict own consequences**: hit-confirm flags (`OnHit`, or `OnBlock` when the
   snapshot says the victim is shielding — always false until M6), attacker hitstop window from the knockback model's
   `AttackerHitstopFrames` (computed with the snapshot's victim data), attacker meter gain request; deliver the hit to
   the victim (7).

## 7. Victim side

- **Inbox** on the victim component: `{ int32 Frame; int32 AttackerId; resolved FGSTN_HitData (variant applied);
  FVector HitLocation; FVector HitboxVelocity; float AttackerYaw; FGameplayTag Relationship; uint8 HurtboxIndex; }`, written only when the
  victim instance ticks on this machine (authority, or locally controlled). Drained at the victim's combat tick of
  `Frame + 1` (entries with `Frame < F`), in (AttackerId, hitbox order) order. On rollback the inbox drops entries with
  `Frame ≥` the restored frame (they are re-delivered by the attackers' re-simulation).
- **Resolution** (combat tick at F+1): run the pipeline per entry. Outcome applied as state and requests:
  `ActionState = Hitstun` (or `Tumble`), `HitstopStartFrame = F`, `HitstopEndFrame = F + HitstopFrames`,
  `HitstunEndFrame = HitstopEndFrame + HitstunFrames`, `PendingLaunch` (quantized velocity), cancel of the current move
  (`ActiveTableIndex = INDEX_NONE`; GA_Move ends itself), the registered outcome appliers run for each `OutcomeExtensions` entry (§3b), requests: damage GE, extra GEs, cue, freeze move (duration =
  hitstop), buffer event `Input.BufferEvent.HitReceived`, gameplay event `Combat.Event.Hit.Received` (reaction
  abilities may listen; they must not set combat state).
- **Hitstop end** (frame `HitstopEndFrame`): DI from this frame's stick → `ApplyDI` → request knockback move
  (`FGSTN_LayeredMove_Knockback { Velocity; float DecayPerFrame; float GravityScale; int32 ElapsedFrames; }`, forcing
  Falling, applied through a GE + move creator) and `KnockbackResistance` applied.
- **SDI is not in M4.** It is specified once, by M6 §5.5 (`FGSTN_SDIRules`, new direction events during hitstop). M4 ships DI
  only; the sync field `SDICount` is reserved here and written by M6. (Earlier drafts described a basic SDI here while
  §1 put it out of scope: M6 owns it.)
- **Hitstun end**: `ActionState = Actionable` (or `Tumble` until landing when tumbling); M6 adds tech.
- Tags projected: `Combat.State.Hitstun`, `Combat.State.Tumble`, `Combat.State.Hitstop`.

## 7b. Surface impacts: wall splat, bounce, destruction

Owner direction (§4): a launched entity that hits a surface can **splat** on a wall, **bounce**, or cause **destruction**, "as
the designer decides". M4 supplies the mechanism and the data; the *who decides* question is a *Proposal*.

**Outcomes** (`EGSTN_ImpactOutcome`): `None` (slide or land as M4 does today), `Splat` (pinned to the surface for
`SplatFrames`, velocity zero, extra damage and stun), `Bounce` (velocity reflected about the contact normal × `Restitution`,
stays `Tumble`), `BreakSurface` (the surface object is destroyed, the victim continues with `ResidualSpeedScale` of its speed).
*Reading to confirm:* "destruction" may also mean the **victim** shattering on impact; modelled as an optional `ImpactDamage`
that can reach the cap (KO), not as a fourth outcome.

**Who owns the choice (*Proposal*, three layers, first match wins):**

| Order | Owner | Data | Decides |
|---|---|---|---|
| 1 | **Hit data** | `FGSTN_ImpactProfile { EGSTN_ImpactOutcome Force; FGameplayTagContainer Forbid; float SpeedScale }` on `FGSTN_HitData` | A move can force an outcome (wall-slam finisher) or forbid one (a light jab never splats) |
| 2 | **Victim** | `FGSTN_ImpactResponse` from the kit / definition (by `FGSTN_KitExtension` or the rules): per surface type outcome allowed, `MinImpactSpeed`, `Restitution`, `SplatFrames`, `ImpactDamageScale`; heavy enemies never splat | What this entity can undergo |
| 3 | **Surface** | `UGSTN_SurfaceImpactTable` (DataAsset on the rules): **surface type → outcome defaults and thresholds**; surface type = the contact's `UPhysicalMaterial` mapped to a native/ini tag `Surface.Type.*` (`Surface.Type.Wall`, `.Glass`, `.Crate`); an actor can override it with the tag on its component (`IGSTN_ImpactSurface`, also the hook that destroys a `BreakSurface` target) | The physical truth of the wall: glass breaks, concrete splats, a trampoline bounces |

Rule: `Forbid` removes outcomes; `Force` (if the surface allows anything at all, or `bForceIgnoresSurface`) wins over the rest;
otherwise the victim's response restricts the surface's default. The impact speed along the normal must exceed `MinImpactSpeed` of
the chosen row, else `None`. Pure function `UGSTN_ImpactMath::Resolve(const FGSTN_ImpactInput&, rules) → FGSTN_ImpactResult`,
integer-quantised inputs, golden-tested in Core.

**Where it runs.** The probe is the contact record of the tumble (`FGSTN_TumbleContactSyncState`: kind, normal, impact speed,
contact frame, **plus** `SurfaceTypeIndex`, `uint8`, from the physical material through a baked table): M4's knockback layered
move carries a minimal probe that writes it; M6's `FGSTN_TumbleContactModifier` (tech) is the same record, not a second one.
The combat tick reads it at end of F-1 (one frame after contact, as M6 §6) and applies the result as state and requests
(a freeze or a reflect velocity through Mover queued moves, damage GE, request to `IGSTN_ImpactSurface`, event
`Combat.Event.Impact.*`). The surface table is part of the rules asset and the physical-material mapping is baked at load;
nothing reads live components in the decision.

**Interaction with tech (M6).** At the same contact frame the tech window is evaluated first: a **successful tech cancels the
outcome** (no splat, no bounce, no damage; a wall tech is the reward). Only after a failed or absent tech does the impact
resolve, and its result **replaces M6's default failure branch** (ground knockdown, wall bounce-off) when the three layers
give a non-`None` outcome; with no data the M6 defaults apply unchanged. Hit data may also set `bNoTech` to make an impact
unavoidable (*Proposal*, unset by default). Splat ends in `Actionable` after `SplatFrames` (no tech from a splat in the first
version).

## 8. Hitstop for both sides

- Move clock: `MovePausedFrames++` on every hitstop frame.
- Buffer: presses during hitstop are recorded; the time-buffer age excludes hitstop frames — pass the last hitstop window as
  `FGSTN_MatchParams::PausedStart/PausedEnd` (already in M1).
  The resolver does not run during hitstop.
- Montage: paused while a montage plays (M0 rule), resumed at `HitstopEndFrame`.
- Mover: `FGSTN_LayeredMove_Freeze` (OverrideAll, zero velocity, keeps the incoming velocity in its state and restores it
  on finish for the attacker; the victim's knockback replaces it), plus the `Combat.Mover.Frozen` tag (spike 2.2).
  `FGSTN_LayeredMove_MoveTrack` (M3) does not advance while frozen.
- **Wall lock** (project, by extension point, per wall-movement.md): `UGSTN_WallMovementModeBase` gains a protected
  virtual `IsMovementLocked(const FMoverTickStartData&)` (default: Mover has `Combat.Mover.Frozen`); when locked the
  controlled wall mode skips `CheckForWallExit`, `TryWallJump`, `TryDetachByPullingAway` and does not advance
  `TimeWithoutWall`/`PullAwayTime`. **Tell the user**: this touches the wall movement core (a new extension point, no
  behaviour change when unlocked).

## 9. Lag compensation and presentation

- **Authority, attacker controlled by a remote client** (the host machine of a listen server, or a dedicated server),
  `bUseLagCompensation`: before building its hit queries the attacker
  calls `UNetworkPredictionWorldManager::RewindActors(Attacker, interpolation time of its last input)` and builds the
  target transforms from the rewound actors (a per-attacker snapshot overlay, not the shared snapshot), then
  `UnwindActors()` before returning. Requires `UNetworkPredictionLagCompensation` on hittable pawns. AI attackers never
  rewind.
- **`UGSTN_HitPresentationComponent`** (attacker's machine): when `FinalizeFrame` sees a new predicted entry in
  `HitTargets`, it plays local-only effects on the (interpolated) victim: flash material parameter, local cue
  (`ExecuteGameplayCueLocal`), victim mesh pause for the hitstop length. When a later corrected state removes that
  entry, it stops them early. The real cue comes from the victim's ASC and is deduplicated by hit id on the attacker's
  machine.

## 9b. Listen server (owner direction, game-design-direction.md §6b)

M4's "server" means **the authority**. The reference topology is a **listen server**: a friend hosts and plays, the others are
clients. The simulation code does not change (no topology branch, rule 1 of CLAUDE.md); what changes is what we assume about
the authority and how we measure it. A dedicated server stays supported.

- **The host is the authority and a zero-latency player.** Hit resolution for everyone, **lag compensation** (`RewindActors`) and
  the victim inbox of every non-owning entity run **on the host's machine**, in the same frame as the host's own simulation,
  rendering and input. The host's own pawn is both attacker and victim without correction: its hits resolve the frame they
  happen against current authoritative state, and hits on it arrive as ordinary inbox entries, not as client corrections.
- **Host advantage (stated).** The host never waits an RTT, never rolls back for hits, and never needs to be rewound for; every
  client attacker is partly compensated (`MaxRewindMs`) but still acts about one RTT after the host sees it. *Proposal*:
  (a) accept it in the first version, since lag compensation already moves the advantage toward the clients; (b) keep
  `MaxRewindMs` as the fairness knob, tested in the harness with the host as attacker and as victim; (c) add the presentation-only
  `bHostHitFeelParity` of combat-parts.md §5.5b only if playtests show it. No simulation delay for the host (it would break
  determinism and the host's own prediction).
- **Budget shares the frame.** Hit resolution, snapshot building, rewinds and (later) minion simulation (M8) share the host's frame
  with its own game and rendering; keep the per-hit cost small and measured on the host configuration, not on a bare server.
  The host's upload (home connection) is the bandwidth limit of the whole session; the M4 sync additions are small (§11), but the
  harness reports host bytes per second.
- **If the host leaves, the session ends** (no host migration unless the owner asks).
- Spike 2.1 and the harness cover both configurations: **listen-server (host + 2 clients)** and dedicated + 2 clients. Host as
  attacker (clients show about one RTT/2 + interpolation delay later), client as attacker against the host (the host's victim is
  authoritative: expected zero rollbacks on the host, one correction on nobody), client against client (as before).

## 10. Content, tools, harness

- Hurtbox set for the test character (manual capsules, or a Python helper that reads the Physics Asset bodies and
  writes the set) and a move with: a root hitbox, a baked-socket hitbox (Python helper samples the montage at the NPP
  tick rate and writes `BakedSockets` — replaced by the M5 tool), a `WorldAtMoveStart` hitbox, a variant for allies
  (half shape, no hitstun).
- Debug draw (`gstn.Combat.Debug.Hitboxes 1` / `Hurtboxes 1`): active hitboxes per frame, colour by group; hurtboxes by
  state.
- Harness scenarios (`gasnpp_test.py`; both clients' pawns placed facing each other by a helper):
  `scenario_hit_basic` (c1 jabs c2: c2 damage/knockback/hitstun equal on all machines, c1 predicted hitstop and
  cancel-on-hit), `scenario_hit_trade` (both on the same frame), `scenario_friendly_fire` (same team: off → no hit,
  on → variant applied), `scenario_hitstop_montage` (forced reconcile during hitstop with a montage),
  `scenario_hit_on_wall` (victim wall sliding, frozen during hitstop), `scenario_di` (victim holds a side: launch
  rotated), `scenario_hit_host` (host attacker, host victim, client against host: states equal, host without rollbacks),
  `scenario_dodge_feet` (feet intangible, torso hit during the base ground dodge), `scenario_impact_wall` (splat, bounce, forced
  and forbidden by hit data; tech cancels it in M6), all with 60/15 ms, with and without 2 % loss, **in the listen-server
  configuration (host + 2 clients) and in dedicated + 2 clients**. Clean = snapshots equal (combat state, attributes, positions
  after the launch settles) and no reconciles besides the forced ones **and the expected one per hit on the victim's
  client** (spike 2.1).
- Automation tests (Core): `UGSTN_ImpactMath` precedence (hit, victim, surface, thresholds), modifier order in every knockback
  model, the `HealthWeight` curve table, seams with fake providers (§3b), every shape pair (touching, separated by ε, contained), swept overlap catches a fast box,
  knockback model values against hand-computed ones, DI rotation limits, variant application, pipeline step order
  with fake contexts, inbox ordering and rollback trimming.

## 11. Sync state additions

`MoveOriginTransform` (quantized), `HitTargets[8]`, `HitConfirmFlags`, `HitstopStartFrame/EndFrame` (now real),
`HitstunEndFrame`, `PendingLaunch` (int16 × 3, cm/s / 4), `SDICount` (reserved, written by M6), `ImpactEndFrame` (splat),
hurt state of the current frame (derived, not
stored). Idle cost stays ~0 thanks to end-frame stamps.

## 12. Acceptance criteria

1. Spikes recorded; scenarios of section 10 clean as defined in both topologies; M1–M3 tests and scenarios still clean.
2. A hit costs the victim's client at most one rollback in the basic scenario.
3. The test move's three hitbox spaces hit where the debug draw shows them, on the authority (host) and on clients.
4a. The seams of §3b exist and are inert: combat tests and scenarios pass with no optional plugin installed; modifiers change
    knockback, hitstun and hitstop exactly once (Core tests).
4b. The base dodges (§1b) make only the feet intangible; surface impact outcomes follow the precedence of §7b.
4. Friendly-fire variants work per game mode by changing only the rules asset.
5. combat.md updated (§12, §17).

## 13. Risks

- Victim latency in PvE (interpolated enemies react ~RTT/2 + interpolation delay later): presentation must carry the
  feel; measure in spike 2.1 with presentation on.
- Forward prediction of attackers (combat.md §12.7) stays off in M4. Keep the victim inbox able to be drained on the
  owning client later (no server-only assumptions inside the resolution steps besides where the instance ticks), and
  log in spike 2.1 how many hits on the local player arrive as corrections: that number decides a later forward
  prediction milestone.
- Snapshot invalidation relies on `RestoreFrame` being called for every combat instance on rollback (true for the fixed
  rollback service, combat.md §2 fact 8); assert it in development builds.
- Swept overlaps multiply query cost; keep them opt-in per hitbox.
- **Host-only costs and advantage** (§9b): resolution, rewinds and rendering share one machine; host hits feel instant.
- **Seams slipping out of M4** make M4b edit the combat core (combat-parts.md §8.5): they are acceptance items (§12).
- **Impact data explosion** (surface types × victims × hits): defaults live in the surface table, victims only restrict, hits only
  force or forbid; a validator prints the resolved outcome matrix.
