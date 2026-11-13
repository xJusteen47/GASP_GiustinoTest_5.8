# M11 slice L1 (thread "K5"): loadout seams (laptop, 2026-11-12)
Branch `claude/laptop-m11-k5` from `9f9589b1` (tip of `claude/laptop-io-2c`). Plan: `docs/architecture/plans/m11-kit-piano-a-fette.md` §2 L1
(card gate G1), spec `combat-loadout-mods.md` §11 seams 1, 2, 5. L1 is the first after-playtest slice and needs no move-design decision: none skipped.
**Done** (code `f93b7245`; the branch tip is the commit of this file):
1. Seam 1: `FGSTN_LoadoutData::Extensions` (`TArray<FInstancedStruct>`, base `FGSTN_LoadoutExtensionData`, new header in GSTNCombatCore
   `Loadout/GSTN_LoadoutExtensionData.h`; the spec's name `FGSTN_LoadoutExtension` was taken by the GAS registry entry). Wire ids:
   `FGSTN_LoadoutExtensionTypes::Register(WireId, Struct)` at module startup, else the struct path. Per entry: id, length, bytes (reflected
   binary or the type's `SerializePayload`). Missing type = entry dropped with a warning; overflow of the 1 KiB block = entry left out whole.
   Extension block layout byte 1 = typed; layout 0 and unknown layouts still carried raw (`ReservedExtension` + `ReservedLayout`).
   Extensions are in `operator==` and in the combat table checksum (`GetExtensionBlockBytes`, used by `GSTN_CombatSimComponent.cpp`).
   Helpers `GSTNLoadoutExtension::Find<T>` / `Set<T>` (one entry per type).
2. Seam 2: the registry existed (`FGSTN_LoadoutExtensionRegistry`); added `Priority` (lower first, then id) and `GetOrderedIds`.
3. Seam 5: `IGSTN_AttachmentProvider` + `FGSTN_AttachmentProviderRegistry` (GSTNCombatGAS `Cosmetics/`); `UGSTN_SkinComponent::RefreshAttachments`
   runs on each applied loadout, attaches static/skeletal meshes to sockets of the skinned mesh, no collision, replaces old ones. Sync load.
4. Docs: GSTNCombat and GSTNCombatGAS READMEs, spec §11 note.
**Decisions taken (recommended option, no owner needed)**: entries an extension adds keep the kit/weapon/super sources; a new
"extension" source kind would change the source-mask bit layout (sim state), left to G4 when the first plugin needs it. Async load of
attachments (keep old look until ready) and `FGSTN_EquipmentVisual` stay in G5 as planned.
**Gate (laptop)**: build ok after 1 fix (missing include in the new test). Headless GSTN **1517/0** (1514 + 3 new:
`GSTNCombat.Core.Data.LoadoutTypedExtensions`, `GSTNCombat.GAS.Loadout.ExtensionPriority`, `GSTNCombat.GAS.Skin.AttachmentProviders`),
0 ensures (`Saved/M11k5/headless-t1.log`). Suite `R31_ALL=1`: **50/50 PASS**, 0 ensures (`Saved/R31/report-m11k5-all.txt`).
Editor exit crash at quit as known (GSTNHeldRuntime/HeldCore frames, `Saved/Crashes/UECC-Windows-5D0FBF9C...`): not touched (desktop IO-2d).
**Not covered**: no plugin uses the typed extensions or a provider yet (first users: L2 pickups, L5 mods, L6 implants, L7 backpacks);
no harness scenario sends a loadout with extensions across the network (unit round trip only). Game/Shipping target not built here.
**Next in order**: L2 (GSTNPickups, desktop, Opus medium). No manual step for the owner; artifact not changed.
Cost: ~95k context, Opus, estimated ~$5-7. Wall clock ~1 h (2 builds, headless ~12 min, suite ~25 min).
