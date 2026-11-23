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
  stab line), and sub-regions with their own hit data. Owner answer: lengths may change during a match (extensible weapons) but only in discrete steps, not continuously, and
  modding must be possible. Decision: the length is a weapon stat as a GAS attribute (`Combat.Stat` leaf), set by the weapon's
  stats effect (M3 weapon switch already swaps it) or by a mod/extension effect; the fork rolls attributes back with Network
  Prediction, so a discrete change is predicted and corrected like any effect. The hitbox evaluation reads the attribute value of
  that frame, quantised (e.g. to 1 cm) before scaling, so every machine scales the same; the editor previews with a chosen length.
