# GSTNModeVerbs

Mode verbs framework (`M12 G2`, [game-modes.md](../../docs/architecture/game-modes.md) §2.4):
- **`GSTNModeVerbsCore`**: native tags (`Ability.Mode.*`, `Command.Mode.*`, `Interact.Target.Objective`, `Message.Mode.Verb`), `FGSTN_VerbSpec`, `FGSTN_ModeVerbPayload`, and core automation tests.
- **`GSTNModeVerbsNP`**: `UGSTN_GA_ModeVerb` (predicted hold inside NP, single confirm event at confirmation frame), `UGSTN_RequestableComponent` (frame-stamped `Arm`/`Disarm` interactable), `UGSTN_VerbSubsystem` (server validation + `Message.Mode.Verb` publish).
