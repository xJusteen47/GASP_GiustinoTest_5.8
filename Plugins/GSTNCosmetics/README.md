# GSTNCosmetics

Modular cosmetic themes (design: `docs/notes/skin-cosmetici-modulari.md` on branch `claude/skin-cosmetici-ricerca`, Lyra comparison
`skin-lyra-confronto.md` on `claude/project-thread-u2vd2x`). Optional plugin, `EnabledByDefault` false, enabled in the project.
Presentation only: nothing in the simulation reads it, pieces never collide, only IDs travel on the network.

The idea: a theme is sold once and decorates **any** conforming item. The item (host) says WHERE (slots with an envelope, decorative
splines, vertex colour channels, UV1); the theme says HOW (one piece per slot class, one module per spline class, one layered material,
style channels). Art cost grows as themes + items, never themes x items.

## Modules

| Module | What | Depends on |
|---|---|---|
| `GSTNCosmeticsCore` | `UGSTN_CosmeticThemeData` (`GSTN_CosmeticTheme`), `UGSTN_CosmeticHostData` (`GSTN_CosmeticHost`), the types (`GSTN_CosmeticTypes.h`), native tags `Cosmetic.Slot.*`, `Cosmetic.Spline.*`, `Cosmetic.Theme`, `Cosmetic.Style`, and `FGSTN_CosmeticPlanning`: piece lookup with parent-class fallback and item overrides, fit into the envelope, spline segments, silhouette limit, style resolve, the plan, tags gathered from pieces, the cache key, validation | engine only |

## Phases

1. **Done (cloud, 2026-11-23)**: data, plan, validation, tests `GSTNCosmetics.Core.*`. Not compiled in the cloud.
2. Editor (after the Content reorganisation, one editor at a time): copy sockets, bounds and piece sizes from meshes, the assembler that
   fuses a plan into one mesh (Geometry Script, check runtime use on 5.9 first), the locker cache and async build for remote players
   with the amorphous placeholder, asset manager scan paths, the loadout extension that carries theme and style IDs.
3. Reactive styles, temporary parts with handles (Lyra pattern), body slots on the game skeleton, LODs.

## Rules

- `Collision` on every piece and module is a checked field: anything but `NoCollision` is a validation error, and the plan forces
  `NoCollision` anyway. Hitboxes belong to the kit.
- Silhouette: a placed piece may not lengthen the item beyond 5% on X (reach axis) or widen it beyond 15% (`FGSTN_CosmeticLimits`).
- Budget per item: 12 pieces, 4 splines, 8 segments per spline. Over budget the first slots in authored order win.
- More than one override in ten slots for a theme is a warning: fix the item's slots instead.
