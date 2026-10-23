# Owner requirements, 2026-10-23 (desktop thread)

To fold into the M4 (hits) and M5 (editor) specs at their next docs pass.

- **Enemies in the hit/timeline editor.** The editor must work on enemies too, humanoid and non-humanoid skeletons. Enemy
  attacks use the same timeline assets as the player, with the enemy mesh as the preview subject (answer 1A).
- **Mirroring.** Left/right hurtbox mirroring only when the skeleton has recognisable name pairs, configurable per skeleton;
  otherwise off (answer 2A, affects E9).
- **Multi-mesh preview.** The preview shows several meshes together: the main skeletal mesh plus attachments (weapon in the
  hand, turret on a base). Reason: the reach of a sword cannot be judged without the sword (answer 3B, E6).
- **Weapons with moddable length (M4).** Hitboxes scale with the weapon length, both for swings and for stabs, and a hitbox
  carries sweet spots and sour spots (different hit data by region along the weapon). The M4 hitbox spec (`FGSTN_HitboxSpec`,
  hit data variants) needs: a length parameter from the equipped weapon, a scaling rule per hitbox (along an axis, swing arc vs
  stab line), and sub-regions with their own hit data. Open question for M4 planning: is the length a weapon stat replicated as
  part of the loadout (deterministic, read by the simulation) or fixed per weapon asset.
