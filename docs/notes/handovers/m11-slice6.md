# M11 slice L2: ground pickups, plugin GSTNPickups (laptop, 2026-11-12)
Branch `claude/laptop-m11-l2` from `759372df` (L1) + merge of `origin/claude/desktop-io-2d` (`b00a916e`, editor exit fix), merge `36d4a82d`.
Plan `docs/architecture/plans/m11-kit-piano-a-fette.md` §2 L2, card gate G2, spec `combat-loadout-mods.md` §4, §6, §12. Code `2dffd69e`.
**Done**:
1. New optional plugin `Plugins/GSTNPickups` (one Runtime module, README, enabled in the .uproject; GSTNHeld and the game module depend on it).
   `AGSTN_KitPickup` and `UGSTN_KitSwapSubsystem` moved out of GSTNCombatGAS (git renames; class redirects in `Config/DefaultGSTNPickups.ini`).
2. `AGSTN_EquipmentPickup`: replicated `FInstancedStruct` payload (base `FGSTN_PickupPayload`, pure `MakeChange`), `Take` = payload change ->
   `AcceptLoadout` -> leftover stays. `AGSTN_KitPickup` is its child (`GetLoadout`/`SetLoadout` kept, payload `FGSTN_KitPickupPayload`).
3. Support weapon payload: one at a time; the last additional weapon is swapped out and stays on the ground, its slot customisations dropped;
   the kit's `AllowedAdditionalWeapons`/`MaxAdditionalWeapons` decide through the rules.
4. Race rule `FGSTN_PickupClaim::SortForRace` (earliest request frame, then lower player id, then queue order); GSTNHeld's `KitInteract`
   bridge sorts its pending swaps with it (before: arrival order) and now serves every equipment pickup (support weapons swap with hold B too).
5. Docs: GSTNPickups README, Plugins/README index, CLAUDE.md table, GSTNHeld README step 17, combat.md.
**Decisions taken (recommended option)**: the kit payload keeps K1's whole-loadout swap (carry-over is an owner question, below); the K1
bridge stays in GSTNHeld (its tuning, slot and input function `Held.KitSwap` are GSTNHeld's; moving them renames ini keys); the interact
meaning stays `Interact.Target.Kit` for all equipment (no `Interact.Target.Equipment` yet: one more tag and slot for no player-visible gain).
**Gate (laptop)**: build ok after 1 fix (two `GetLoadout` calls in the bridge). Headless GSTN **1521/0**, 0 ensures (1517 - moved
`GSTNCombat.GAS.Loadout.KitPickupSwap` + 5 new `GSTNPickups.Payload.Kit|SupportWeapon`, `.Race`, `.Take.Kit|SupportWeapon`;
`Saved/M11L2/headless-t1.log`). Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l2-all.txt`); `kit_swap_hold` listen PASS
(`Saved/Logs/m11l2/kit.log`). **Editor exit**: clean after the io-2d merge (exit 0, no new crash dump in three editor runs).
**Not covered**: no harness scenario for a support weapon pickup or two players on one kit (race tested pure); no `BP_Pickup_*` children and
no support-weapon pad in the hub (G8 sweep / K3); the test component has no spawn helper for a support pickup; Game/Shipping not built.
**Owner questions (grouped, none blocks the next slices)**: M11.2 when you swap kit, does your support weapon stay with you (if the new kit
can carry it) or stay on the ground with the old kit? M11.3 meters (Ex/Super) keep their value across a kit swap? M11.4 mods: your saved
setup for that kit or the ones on the pickup? M11.6 may a heavy support weapon slow you (own stats), or never?
**Next in order**: L3, L4, L6, L9 in any order. No manual step for the owner; artifact not changed.
Cost: ~120k context, Opus, estimated ~$7-9. Wall clock ~1 h (2 builds, headless ~3 min, suite ~15 min, kit scenario ~3 min).
