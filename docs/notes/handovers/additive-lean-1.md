# Handover: additive lean 1 (cloud, 2026-11-18)
Branch claude/project-thread-zqg4ty (from claude/project-thread-98dml5 134dbbb1). Model Opus 5.5 medium. Not built (cloud).
Plan / details: docs/notes/additive-lean-1.md.

Done:
- FGSTN_LocomotionLeanSettings + FGSTN_LocomotionLeanSolver + UGSTN_LocomotionAnimSet (GSTNCombatNP Presentation/GSTN_LocomotionLean.h/.cpp).
- Native tag Anim.Set.Locomotion (GSTN_CombatNPTags).
- UGSTN_RunPresentationComponent: TickLean + layer 1 of the additive request (through the existing directional additive node: no ABP change).
- CVar a.GSTN.LocomotionLean (scale, 0 off). Tests GSTNCombat.NP.LocomotionLean.Direction / .Solver.
- Tools/GAS/setup_locomotion_profile.py: DA_AnimSet_Locomotion_Default + rule in DA_AnimSetCollection_SandboxCharacter (all kits).

Decisions applied (rule 6): lean via the existing additive node (spine only, legs/root/foot lock = GASP); off in air, attacks, skid (scale 0,
settings); values in a per-character anim set, not in the kit DA (owner 12:10Z principle).

Next (desktop): build + tests, run the setup script, capture base (CVar 0) vs lean, Fable review. Not checked: compile (no engine in cloud).
