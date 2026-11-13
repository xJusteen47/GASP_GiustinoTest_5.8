# Worn backpacks visible on the back (laptop, UE 5.9, 2026-11-13)
Branch `claude/project-thread-2v2slw` from `2d505953` (kit box on the back bone). Owner asked: a worn backpack shows on the back.
**Done**:
1. `UGSTN_SkinComponent` (CombatGAS): two flags, both default true so a designer-placed component is unchanged. `bApplySkin` false =
   equipment pieces only; `bIncludeLoadoutVisuals` false = only the attachment providers' pieces (backpacks, mod visuals), not the kit's
   and weapon's own `Visual`.
2. `UGSTN_KitPlaceholderLookSubsystem` (same 4 Hz scan, drawing machines only): every player pawn whose PlayerState has a loadout and that
   has no `UGSTN_SkinComponent` gets a transient `GSTN_EquipmentLook` one with both flags off. It binds to the PlayerState's loadout at
   begin play and rebuilds on every applied loadout, so pick up / drop / swap / loadout screen all add and remove the backpack. No
   Blueprint, nothing replicated, nothing in the simulation. Pieces ride the drawn body mesh (GASP visual override), socket
   `Connector_Backpack` -> bone `spine_05` (fallback from the previous thread).
3. Data: `Tools/GAS/setup_backpack_visuals.py` (idempotent, headless, assets only) filled `Visual.Parts` of `DA_Backpack_JetPack`
   (2 orange tanks + dark nozzles), `DA_Backpack_ShieldDome` (blue box + cyan dome), `DA_Example_Backpack_Dome` (purple box + yellow
   disc). Engine basic shapes recoloured by `BasicShapeMaterial`'s `Color`, no new mesh, no Nanite. Offsets in spine_05 space measured
   from the UEFN mannequin ref pose (X up, Y forward, Z right; backpack centre 40 cm behind the bone, just behind the kit box).
**Why kit/weapon Visual is off on the auto component**: the kit's look is still the placeholder box (owner-approved). The KIT_Test_*
implants made by `setup_kit_visuals.py` assume the bone X is the back normal, but on spine_05 X is up: they would draw as a flat slab.
Fix those offsets before turning `bIncludeLoadoutVisuals` on (and hide the box when a kit has its own Visual). Also a backpack on a real
implant inherits the implant's non-uniform scale: implant meshes should be authored at scale 1.
`BPK_Test_JetPack` (setup_kit_mod_test.py) has no parts touched by this script; if it is still empty it shows nothing.
**Gates (5.9)**: editor build ok; headless GSTN **1584/0**, 0 ensures. **Not seen in PIE** (no capture): owner checks in L_Test_Hub (loadout screen,
pick a backpack, look at the back; drop/swap removes it).
Logs: `Saved/UE59Load/{build,headless}-backpack-look.*`, `Saved/Backpack/setup_backpack_visuals.txt`.
Cost: ~110k context, Opus, no subagent; estimated ~$10-12. Wall clock ~1 h (1 build, 1 asset script, 1 headless run).
