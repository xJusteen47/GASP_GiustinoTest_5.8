# M11 loadout screen merged onto the UE 5.9 line (laptop, 2026-11-13)
Branch `claude/project-thread-tixkmv` from `61086557` (= `feature/gas-npp-5.8`, 5.9 line, owner pushed it 17:02Z).
**Merged**: `claude/project-thread-ulc64s` (`cc69ab5c`, loadout screen, slice 20, built on the 5.8 line from `fbf98070`). Merge base
`fbf98070`, only 2 commits on that side: **no conflicts** (new plugin `GSTNLoadoutMenu`, loadout choice registry, `IGSTN_InputConfigSource`).
The laptop working copy is now on this branch (5.9 line). Local `.mcp.json` (Rider entry) and untracked `Tools/Animation/`,
`.mcp.json.tmp` carried over untouched (same `.mcp.json` on both lines).
**Blocker**: UE 5.9 editor build fails before compiling: the laptop has only MSVC **14.51.36231** (VS 18 Community and BuildTools), and
UBT 5.9 bans 14.51.x ("compiler issues"); it asks for 14.50.35717. Project files generated fine. So: **no 5.9 build, no headless, no suite**
here. Log `Saved/UE59Load/build-ue59.out`; scripts `Saved/UE59Load/{gate,suite,run_editor}.ps1` (5.9 copies of the M11Loadout/R31 ones).
**Fix (owner, manual)**: Visual Studio Installer → Modify → Individual components → "MSVC v14.50 ... x64/x86 build tools" (14.50.35717),
then rerun `Saved/UE59Load/gate.ps1 -Tag ue59` and `Saved/UE59Load/suite.ps1`. The desktop already builds 5.9, so it can verify instead.
**Not done (by decision)**: no command to move `feature/gas-npp-5.8` to this tip until it is built and tested on 5.9 (recommended; the
merge is clean but the loadout code was only compiled against 5.8). Backpack mesh on the back: separate task.
Cost: ~75k context, Opus, no subagents; estimated ~$5-7. Wall clock ~25 min.
