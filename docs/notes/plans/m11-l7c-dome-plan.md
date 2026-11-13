# M11 L7c: the shield-backpack dome blocks hits (plan, 2026-11-13)

Owner rule (2026-11-13): the dome blocks EVERY hit coming from outside and pushes back whoever hits it from outside; a hit never counts
both inside and outside (one hit = one side). Dome state already exists (L7b): `FGSTN_BackpackFragment::{DomeStartFrame, DomeUntilFrame,
DomeRadiusCm, DomeCenter}` + `DomeCovers`, world-fixed centre (the carrier at the raise frame).

## Findings that shape the design

- Snapshot blobs: `FGSTN_SnapshotExtensionSet::MaxBlobs = 3` and `FGSTN_HitVictimView::MaxSnapshotExtensions = 3`, already taken by
  Defence, Parts, Link. A backpack contributor needs the cap at **4** (+48 B per entry and per history record: cheap).
- Hit sources: melee runs the pipeline on the attacker (prediction) and the victim; projectiles/hitscan post straight to the victim inbox
  (no attacker pipeline, no attacker outcome). So the block must live in a **pipeline step** (seen by every source); the recoil of a melee
  attacker comes from the existing **attacker outcome registry** (`IGSTN_AttackerOutcomeExtension` -> `Ricochet`, native, registered by the
  module: automatic). Ricochet = the hit does nothing on the victim, the attacker's move ends into its recoil (move's `RicochetResponse`, else
  the combat default shove, 30 frames, no displacement).
- Steps enter the rules through content libraries (`UGSTN_DefenceContentLibrary::AddDefenceStepsToRules`); same pattern here.

## Design (recommended options, flag to owner)

1. **Side of a hit = where the attacker's root was in the snapshot (end of F-1)**, the same snapshot attacker and victim read, so every
   machine and a lag-compensation rewind give one verdict per hit. Projectile shell test uses the projectile's previous position.
2. **Backpack snapshot contributor** (GSTNBackpackNP, type id `GSTNFeatureIds::Backpack` = 21), POD <= 40 B:
   dome centre (FVector3f), radius, bHasDome, and `InsideDomes[4]` (owner entity ids) + count.
   - `Contribute`: the dome from the fragment if up at the frame; at a rebuild keep `InsideDomes` from `Previous`.
   - `AppendExtraHurtboxes`: the dome as a sphere, root-relative centre (inverse entity transform of the world centre), Priority 255,
     no IgnoreHitTypes (everything, grabs too), Source = Backpack, new flag `bOutsideOnly`.
   - `FinalizeSnapshot`: early out when no entry has a dome; else for each entry, the domes whose sphere holds its root -> `InsideDomes`
     (entries without a backpack blob get one). O(entries x domes), domes are rare.
3. **Core: `FGSTN_ExtraHurtbox::bOutsideOnly`** (sphere only): the attacker tick skips that extra when the attacker root is inside it;
   the projectile subsystem when the projectile's previous position is inside. An attacker inside never touches the shell.
4. **Step `UGSTN_HitStep_BackpackDome`** right after TeamFilter (before Parry/Shield). Blocked when the hurtbox is a Backpack-source extra,
   or when the victim's `InsideDomes` holds a dome the attacker's (`Incoming.FindAttackerExtension`) does not (no attacker blob = outside).
   Blocked = no damage, no knockback, no hitstun/flinch, outcome extension `FGSTN_BackpackDomeBlock` (dome owner id), event
   `Backpack.Event.DomeBlocked`. Pure, same on attacker prediction and victim.
5. **Attacker outcome `Backpack.Dome`**: Ricochet when the view carries `FGSTN_BackpackDomeBlock` (melee attacker recoils).
6. `UGSTN_BackpackContentLibrary::AddDomeStepToRules` / `Remove...` + `Tools/GAS/create_backpack_assets.py` calls it on the standard rules.

## Not in this slice (next)

- Displacement push per dome data (`PushbackCm`, frames): needs the outcome extension to hand its own `FGSTN_RicochetResponse` to the
  attacker tick (core interface change). Until then the push is the recoil (shove montage).
- Dome visual; pickup pad and PIE check; harness scenario `backpack_dome` (needs a backpack loadout PIE run).

## Tests to add

Core: shell filter (attacker inside/outside), MaxBlobs 4. Backpack NP: contributor blob + hurtbox, FinalizeSnapshot inside lists, step
verdicts (shell hit, inside victim vs outside attacker, both inside, two domes), outcome = Ricochet only with the block extension.
