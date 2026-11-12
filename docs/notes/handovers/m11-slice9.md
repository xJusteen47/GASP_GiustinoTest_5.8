# M11 slice L6: equipment look (implants, connector, cosmetic mods) (laptop, 2026-11-12)
Branch `claude/laptop-m11-l6` from `01173a47` (L4). Code `2c65c06c`. Plan `docs/architecture/plans/m11-kit-piano-a-fette.md` §2 L6,
card gate G5, spec `combat-loadout-mods.md` §9 (now carries a *Built (M11 L6)* note). Taken on the laptop (plan said desktop): no game-feel choice.
**Done** (all GSTNCombatGAS `Cosmetics/`, no Core/NP change):
1. `FGSTN_EquipmentVisual` (list of `FGSTN_AttachmentRequest`) as `Visual` on kit and weapon (weapon: empty = nearest loaded parent's).
   Names in `GSTNEquipment` (`Implant_Back`, `Implant_Hand/Foot_L/R`, `Weapon_Hand_R`, `Connector_Backpack`, slot `Implant.Back`; a kit
   part on `Implant_Back` gets the slot by itself).
2. Request fields: `ParentSlot` (backpack on the implant's connector: any backpack fits any kit; L7 uses it), `bShownByDefault`,
   `bReplacesSlot` (mod mesh swap), `Vector/ScalarParameters` (MID; no mesh = recolour the slot). Pure `FGSTN_EquipmentVisualUtils::Compose`.
   So mod cosmetics need no separate `FGSTN_ModVisual` type: L5 mods emit plain requests through a provider.
3. `UGSTN_SkinComponent`: kit + primary weapon look, then providers, composed; async load (old look kept until ready, newer refresh
   cancels), `SocketFallbacks` (missing Implant_* socket -> mannequin bone, per component, not a project setting), slot show/hide API
   (`SetVisualSlotShown`/`Clear...`) = hook for the future `FGSTN_VisualTrack`.
4. Kit/weapon `Validate`: back implant mesh without `Connector_Backpack`, part that does nothing, replacement without slot.
5. `Tools/GAS/setup_kit_visuals.py` (idempotent): `SM_Implant_Placeholder` with connector, tinted back implant on every `KIT_Test_*`,
   hand/foot spheres on every `WPN_Test_*`. **Not run** (content belongs to the desktop; never run on two machines).
6. Tests `GSTNCombat.GAS.Equipment.{Compose,LoadoutVisuals,SkinPieces}`; Seams test preloads its cube. README + spec note.
**Decisions taken (recommended option)**: only weapon 0 is shown (which weapon is in hand is sim state; support visuals wait for the
visual track); connector check in the asset `Validate`, not a separate M5 validator; fallbacks on the component, data per pawn class.
**Gate (laptop)**: build ok first try; headless GSTN **1530/0**, 0 ensures (+3; `Saved/M11L6/headless-t1.log`). Suite `R31_ALL=1`
**50/50**, 0 ensures (`Saved/R31/report-m11l6-all.txt`).
**Not covered / nobody has seen in PIE**: no content has a Visual yet, so nothing shows; the async path (old look kept) and the bone
fallbacks are untested in a real PIE; setup script untested. Desktop: build, run the script with the editor closed, look at the hub
kit swap in PIE (implant changes colour, nothing stacks). Game/Shipping not built.
**Owner questions**: none new. Still open: M11.2/M11.3/M11.4, M11.6, special swap input.
**Next in order**: L9 (aimed ranged moves: needs owner game-feel input?), then L5 (must not run in parallel with a GSTNCombatCore slice).
No manual step for the owner; artifact not changed.
Cost: ~85k context, Opus, estimated ~$5-7. Wall clock ~45 min (1 build, 1 headless run, suite ~12 min).
