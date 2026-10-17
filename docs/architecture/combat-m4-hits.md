# Combat M4 — Hits (implementation specification)

Part of the combat design ([combat.md](combat.md)): read §5.3, §12 and §15 there, and the M1–M3 specifications.
Section numbers below are local to this file.

## 1. Scope

**Goal:** moves hit. Hitboxes from move tracks overlap hurtboxes; the attacker predicts its own hitstop and
hit-confirm (cancel windows OnHit/OnBlock become live); the victim, on the machine that simulates it, resolves the hit
through the pipeline: damage, meter, knockback with DI, hitstun, tumble, hitstop with a real freeze (also on walls).
Teams and friendly-fire variants. Lag compensation for remote attackers. Hit presentation on interpolated victims.

Also in: **dodges** (roll, spot dodge, air dodge — the movement moves of combat.md §11b whose `HurtboxModifier`
windows make them intangible), now that intangibility exists.

Out: shield, parry, links and grabs, tech, SDI tuning beyond the basic displacement (M6); piggyback (M7); minions
(M8); editor gizmos (M5).

## 2. Spikes first (record results in combat.md §17)

1. **Cross-entity cost**: client 1's pawn hits client 2's pawn (server-resolved); measure client 2's rollbacks per hit
   and the visible delay on client 1 (presentation off). Expect about one rollback per hit on client 2.
2. **Freeze**: `FGSTN_LayeredMove_Freeze` (OverrideAll, zero velocity) in Walking, Falling and each project wall mode,
   with forced reconciles inside the freeze. Confirms how a layered move exposes a Mover gameplay tag (check
   `UMoverComponent::HasGameplayTag` sources: modes, layered moves, modifiers); if layered moves cannot carry tags, the
   freeze also queues a `FGSTN_FrozenModifier` that does.
3. **Hitstop with a montage playing**: the M0 pause path with a real montage, forced reconcile in the window.

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
FInstancedStruct CustomData; }`.

**Hitbox** — `FGSTN_HitboxTrack : FGSTN_MoveTrack { TArray<FGSTN_HitboxSpec> Hitboxes; }`, `FGSTN_HitboxSpec {
FGSTN_HitShape Shape; EGSTN_HitboxSpace Space (Root/BakedSocket/WorldAtMoveStart/Projectile); FName Socket;
TArray<FGSTN_HitboxKey> Keys (frame, offset, rotation, scale; linear interpolation); uint8 HitGroup; bool
bSweepBetweenFrames; FGSTN_HitData Data; TMap<FGameplayTag, FGSTN_HitVariant> RelationshipVariants; }`.
`FGSTN_HitVariant { bool bIgnore; float ShapeScale = 1; FGSTN_HitDataOverride Override (per-field optional values and
multipliers); }`. `BakedSocket` uses `UGSTN_MoveDefinition::BakedSockets (TMap<FName, TArray<FTransform>>)` — one
root-relative transform per frame, written by the M5 baking tool; in M4 a Python helper bakes them (section 10).

**Hurtboxes** — `UGSTN_HurtboxSetDefinition { TArray<FGSTN_Hurtbox { FName Name; FName Bone; FGSTN_HitShape Shape
(root-relative, baked from the reference pose); FGameplayTagContainer Tags (e.g. Combat.Hurtbox.Head) }> }`.
`FGSTN_HurtboxModifierTrack : FGSTN_MoveTrack { EGSTN_HurtState State (Normal/Intangible/Invincible/Armor); float
ArmorThreshold; TArray<FName> AffectedHurtboxes (empty = all); TArray<FGSTN_HitShape> ShapeOverrides; }`.
Intangible = not hit at all; Invincible = hit registers (attacker hitstop, confirm) but no damage/knockback/hitstun;
Armor = no knockback/hitstun below the threshold, damage applies.

**Knockback model** — `UGSTN_KnockbackModel` (abstract, EditInlineNew, const methods): `Compute(const
FGSTN_KnockbackInput&) → FGSTN_KnockbackResult { FVector LaunchVelocity; int32 HitstunFrames; bool bTumble; int32
HitstopFrames; int32 AttackerHitstopFrames; }`. Default `UGSTN_KnockbackModel_PercentScaling`: knockback grows with the
victim's damage percent and the hit's damage, scales inversely with weight, plus base and growth (the commonly
documented platform-fighter formula, every constant exposed as a property); hitstun = knockback × `HitstunFactor`;
tumble above `TumbleThreshold`; hitstop = `floor(Damage × HitstopPerDamage + HitstopBase)` clamped to `MaxHitstop`,
× the hit's multipliers. `UGSTN_KnockbackModel_Fixed` (HP games): knockback = base, no percent. Direction: yaw from
`YawSource` on the movement plane, pitch from `AngleDeg` (`Sakurai3D`: low angle on grounded victims below a knockback
threshold, higher in the air; `AutoLink`: towards the hitbox's velocity).

**DI** — `UGSTN_HitMathUtils::ApplyDI(Launch, StickOnPlane, MaxDIDegrees)`: rotate the horizontal component of the
launch around world up by `MaxDIDegrees × (component of the stick perpendicular to the launch direction)`; vertical
component unchanged; DI on the plane only, as decided.

**Resolution pipeline** — `UGSTN_HitResolutionStep` (abstract, EditInlineNew, const): `EGSTN_StepResult
Resolve(FGSTN_HitResolutionContext& Ctx) const` (Continue/Stop). `Ctx` holds the incoming hit, the victim's state at
end of F-1 (combat state, attributes snapshot, tags), and the outcome being built `{ bHit, bBlocked, bParried, bClank,
Damage, Knockback result, MeterGains, Events (tags), Effects to apply }`. Steps in M4: `TeamFilter`, `HurtState`
(intangible/invincible/armor), `ComputeKnockback` (calls the model), `ApplyDamage` (adds the damage GE), `Hitstop`,
`MeterGain`. Placeholders for M6 (`Shield`, `Parry`) are documented, not implemented.

**Combat rules** — `UGSTN_CombatRules` (DataAsset): `Pipeline (TArray<UGSTN_HitResolutionStep*> Instanced)`,
`KnockbackModel (Instanced)`, `DamageMode (Percent/Health)`, `bFriendlyFire`, `HittableRelationships`
(`FGameplayTagContainer`), `RelationshipResolver (Instanced)`, `bUseLagCompensation`, `MaxRewindMs`, DI/SDI tuning.
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

1. Active hitboxes of the current move frame (paused frames excluded) → world shapes: `Root` = end-of-F-1 actor
   transform × local key; `BakedSocket` = transform × baked frame; `WorldAtMoveStart` = `MoveOriginTransform` (new sync
   field, quantized, set when the move starts); `Projectile` = owned by the projectile system (later).
2. Broadphase → candidates (excluding self unless the variant for `Self` exists).
3. **Relationship** first (resolver: self / same team = Ally / `FGenericTeamId::NoTeam` = Neutral / else Enemy), then
   `HittableRelationships` and `bFriendlyFire`, then the variant (`bIgnore`, `ShapeScale`).
4. Overlap (swept if flagged) against the target's hurtboxes with its overrides; `Intangible` skipped.
5. **Hit groups**: `HitTargets` ring in the combat sync state (8 entries `{ EntityId, MoveStartFrame, HitGroup }`) — a
   target is hit once per group per move execution.
6. For each new hit: record it; **predict own consequences**: hit-confirm flags (`OnHit`, or `OnBlock` when the
   snapshot says the victim is shielding — always false until M6), attacker hitstop window from the knockback model's
   `AttackerHitstopFrames` (computed with the snapshot's victim data), attacker meter gain request; deliver the hit to
   the victim (7).

## 7. Victim side

- **Inbox** on the victim component: `{ int32 Frame; int32 AttackerId; resolved FGSTN_HitData (variant applied);
  FVector HitLocation; FVector HitboxVelocity; float AttackerYaw; FGameplayTag Relationship; }`, written only when the
  victim instance ticks on this machine (authority, or locally controlled). Drained at the victim's combat tick of
  `Frame + 1` (entries with `Frame < F`), in (AttackerId, hitbox order) order. On rollback the inbox drops entries with
  `Frame ≥` the restored frame (they are re-delivered by the attackers' re-simulation).
- **Resolution** (combat tick at F+1): run the pipeline per entry. Outcome applied as state and requests:
  `ActionState = Hitstun` (or `Tumble`), `HitstopStartFrame = F`, `HitstopEndFrame = F + HitstopFrames`,
  `HitstunEndFrame = HitstopEndFrame + HitstunFrames`, `PendingLaunch` (quantized velocity), cancel of the current move
  (`ActiveTableIndex = INDEX_NONE`; GA_Move ends itself), requests: damage GE, extra GEs, cue, freeze move (duration =
  hitstop), buffer event `Combat.BufferEvent.HitReceived`, gameplay event `Combat.Event.Hit.Received` (reaction
  abilities may listen; they must not set combat state).
- **Hitstop end** (frame `HitstopEndFrame`): DI from this frame's stick → `ApplyDI` → request knockback move
  (`FGSTN_LayeredMove_Knockback { Velocity; float DecayPerFrame; float GravityScale; int32 ElapsedFrames; }`, forcing
  Falling, applied through a GE + move creator) and `KnockbackResistance` applied.
- **SDI** (during hitstop): each new direction event with zone Tilt moves the victim by `SDIDistance` along it on the
  plane (instant movement effect), at most `MaxSDIPerHit` times.
- **Hitstun end**: `ActionState = Actionable` (or `Tumble` until landing when tumbling); M6 adds tech.
- Tags projected: `Combat.State.Hitstun`, `Combat.State.Tumble`, `Combat.State.Hitstop`.

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

- Server, attacker controlled by a remote client, `bUseLagCompensation`: before building its hit queries the attacker
  calls `UNetworkPredictionWorldManager::RewindActors(Attacker, interpolation time of its last input)` and builds the
  target transforms from the rewound actors (a per-attacker snapshot overlay, not the shared snapshot), then
  `UnwindActors()` before returning. Requires `UNetworkPredictionLagCompensation` on hittable pawns. AI attackers never
  rewind.
- **`UGSTN_HitPresentationComponent`** (attacker's machine): when `FinalizeFrame` sees a new predicted entry in
  `HitTargets`, it plays local-only effects on the (interpolated) victim: flash material parameter, local cue
  (`ExecuteGameplayCueLocal`), victim mesh pause for the hitstop length. When a later corrected state removes that
  entry, it stops them early. The real cue comes from the victim's ASC and is deduplicated by hit id on the attacker's
  machine.

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
  rotated), all with 60/15 ms, with and without 2 % loss. Clean = snapshots equal (combat state, attributes, positions
  after the launch settles) and no reconciles besides the forced ones **and the expected one per hit on the victim's
  client** (spike 2.1).
- Automation tests (Core): every shape pair (touching, separated by ε, contained), swept overlap catches a fast box,
  knockback model values against hand-computed ones, DI rotation limits, variant application, pipeline step order
  with fake contexts, inbox ordering and rollback trimming.

## 11. Sync state additions

`MoveOriginTransform` (quantized), `HitTargets[8]`, `HitConfirmFlags`, `HitstopStartFrame/EndFrame` (now real),
`HitstunEndFrame`, `PendingLaunch` (int16 × 3, cm/s / 4), `SDICount`, hurt state of the current frame (derived, not
stored). Idle cost stays ~0 thanks to end-frame stamps.

## 12. Acceptance criteria

1. Spikes recorded; scenarios of section 10 clean as defined; M1–M3 tests and scenarios still clean.
2. A hit costs the victim's client at most one rollback in the basic scenario.
3. The test move's three hitbox spaces hit where the debug draw shows them, on server and client.
4. Friendly-fire variants work per game mode by changing only the rules asset.
5. combat.md updated (§12, §17).

## 13. Risks

- Victim latency in PvE (interpolated enemies react ~RTT/2 + interpolation delay later): presentation must carry the
  feel; measure in spike 2.1 with presentation on.
- Snapshot invalidation relies on `RestoreFrame` being called for every combat instance on rollback (true for the fixed
  rollback service, combat.md §2 fact 8); assert it in development builds.
- Swept overlaps multiply query cost; keep them opt-in per hitbox.
