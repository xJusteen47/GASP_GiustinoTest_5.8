# Hitbox editor H1: setup scripts never overwrite hand edits (cloud, 2026-11-15)

Branch `claude/project-thread-ibkcds` from `claude/integ-cp3` 069287f7. Opus medium, cloud, no editor. Plan: project files `handoffs/archivio/2026-11/hitbox-editor-ux-plan.md`.

## Done
- `Tools/GAS/setup_guard.py`: ledger `Tools/GAS/setup_guard_ledger.json` = sha256 of each .uasset right after a setup script saved it (= its LFS oid).
  Before touching an existing AS/AM/TL a script classifies it: new / owned (write) or edited / unsaved (dirty in the editor) / unknown (KEEP).
  Kept assets are not changed or saved; the only merge is re-linking a timeline's Montage that a refresh emptied. `force=True`,
  `force=['TL_Overlord_Jab1']` or env `SETUP_FORCE=...` overwrites after a copy to `Saved/Backups/SetupGuard/<time>/`.
- Wired into `setup_overlord_jab_kit.py`, `setup_overlord_dair_kit.py`, `setup_overlord_dropkick_kit.py` (refresh deletes, anim/montage
  property writes, ik-foot bake, montage segments, timeline data) and `setup_dropkick_hold_until_landed.py` (refuses an edited timeline).
  Weapon, kit and GA Blueprints are plumbing and still rewritten.
- Ledger seeded with the 24 Overlord AS/AM/TL LFS oids of integ-cp3 069287f7 (`git lfs ls-files -l`): those files count as script-owned.
- Bake on save ON by default (plugin ini `bBakeOnSave=True`, settings test flipped, README). C++/ini only: NOT BUILT here.
- CLI without editor: `python Tools/GAS/setup_guard.py status` (lists edited/missing), `... adopt /Game/...` (vouch current files as script output).

## Proof
- `python3 Tools/GAS/tests/test_setup_guard.py` -> `15 passed` (states, force all/by name, backup, ledger hash = known sha256 of "abc",
  relink only an empty montage, never on unsaved, save_and_record saves/records only written assets, CLI).
- `python3 -m py_compile Tools/GAS/setup_*.py` -> ok. The kit scripts themselves were NOT run (no editor): first desktop run = check.

## For the next threads (rule: commit the ledger with the uassets a setup script wrote)
- Desktop `_ue59` on integ-cp3: `status` should print `ledger: 24 owned`. Anything else (e.g. r1 branch c9c860f5 rebuilt jab clips) is
  KEPT on rerun: if those are script/pipeline outputs, `adopt` them first; never adopt files the owner edited.
- The animpipe publish step to the game path (`_Build` -> consolidate -> rename, manual on desktop) is NOT guarded: run `status` first.
  The owner should edit the copies in `Combat/Overlord`, not `Content/AnimPipeline/..._Build` (an `animpipe build` rewrites those).
- Known gap: a kept montage whose clips a refresh remade keeps dangling segments (logged only by the timeline relink path). Rare.
- H4 build: run `GSTNCombat.Editor.Settings` tests; then one jab script rerun after an owner edit should print `KEPT: TL_Overlord_Jab1`.

## Choices applied (rule 6)
Unknown (no ledger entry) = keep, not write: safe default. Anim/montage copies protected too (owner said he will edit jab animation in
Unreal), not only timelines. No "da confermare".
