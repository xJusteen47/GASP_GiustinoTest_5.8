# M11 slice L3: support weapon stats rule, in-match special swap (laptop, 2026-11-12)
Branch `claude/laptop-m11-l3` from `971c85a4` (L2). Code `7a836cad`. Plan `docs/architecture/plans/m11-kit-piano-a-fette.md` §2 L3,
card gate G3, spec `combat-loadout-mods.md` §6-7 (both now carry a *Built (M11 L3)* note).
**Done**:
1. `UGSTN_LoadoutModeRules` (GSTNCombatGAS `Loadout/`), the loadout's GameRules piece (per mode/level, no Project Settings, no ini).
   `SupportWeaponStats`: `WeaponOwn` (default = behaviour before M11: a support weapon's own stats effect applies if authored) or
   `KitOnly` (weapons after the primary never change stats; carrier keeps the kit's stats). Both answers to M11.6 shipped as data.
2. `UGSTN_CombatSimComponent::BuildWeaponStats(Resolved, Policy)` overload; the old form reads the rule at loadout apply. A rules change
   re-applies the loadout (existing `HandleGameRulesChanged`), so the predicted `SwapWeaponStats` path is unchanged, no new sync state.
3. Validator warning: a weapon tagged `Combat.Weapon.Role.Support` with a `StatsEffect` (spec §6).
4. Special swap in a match: `UGSTN_LoadoutComponent::ServerRequestSpecialSwap(WeaponIndex, Command, Alternative)` -> `AcceptSpecialSwap`
   = `WithSpecial` (one customisation per weapon+command, null = weapon's own) through `AcceptLoadout` (rules judge it, new revision,
   never predicted). Gate `bAllowSpecialSwapInMatch` (default false, spec §7, M11.9). UI helpers `GetSpecialChoices` (own first, then
   `AllowedAlternatives`), `GetChosenSpecial`, `GetNextSpecial(Step)`.
5. Docs: GSTNCombatGAS README "M11 support weapons and special swaps (L3)", spec notes.
**Decisions taken (recommended option)**: rule lives in the GameRules piece (mode data), not on the kit; "support" = every weapon after the
primary (loadout order), not the role tag; swap gate default off as the spec says (the hub rules asset can turn it on); NO button bound to
the special swap (input/move design is the owner's: a cycle command or a loadout-screen entry, see question below).
**Gate (laptop)**: build ok first try. Headless GSTN **1523/0**, 0 ensures (+2: `GSTNCombat.GAS.Loadout.SpecialSwap`,
`GSTNCombat.NP.Table.SupportStatsRule`; `Saved/M11L3/headless-t1.log`). Suite `R31_ALL=1` **50/50**, 0 ensures (`Saved/R31/report-m11l3-all.txt`).
**Not covered**: nobody saw it in PIE; no harness scenario for a support switch under `KitOnly` or a special swap over the network
(the RPC path is AcceptLoadout's, already harness-tested by kit swap); Game/Shipping not built. Edge: switching the rule to `KitOnly`
while a support weapon is active leaves that weapon's effect on until the next switch (rules never change mid-match by contract).
**Owner questions (grouped)**: M11.6 now a mode setting, still needs the answer for the default. New: how does a player change a special in a
match, if ever (a button that cycles, a menu at supply points, only between levels)? Until then the API exists with no input.
Still open from L2: M11.2, M11.3, M11.4.
**Next in order**: L4, L6, L9 in any order. No manual step for the owner; artifact not changed.
Cost: ~95k context, Opus, estimated ~$5-7. Wall clock ~45 min (1 build, headless ~3 min, suite ~15 min).
