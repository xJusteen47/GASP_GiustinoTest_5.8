# Input options per player: plan (2026-11-11)

Design summary from the laptop design thread (it could not commit), approved by the owner on 2026-11-11 ("Ok a tutte").

## Facts today
- Chain: physical key -> Input Action (in the mapping contexts) -> logical button `Input.Button.*` (InputConfig asset) -> function.
  Only the logical button travels on the network; keys never do.
- The GSTN Held settings of slice 3b map logical button -> function (pickup/drop, throw, kit swap), as data.
- Still in C++: 6 points in older plugins (jump: surface launch and drill exit; shield: tech, parry; mount interact + crouch;
  spin crouch) and the two right-stick buttons. List with files: `docs/notes/handovers/m11-slice3b.md`.
- Before IO-1 nothing was per player.

## Owner rules (2026-11-11)
1. No hardcoded inputs.
2. No Project Settings for inputs or gameplay factors: keep modularity high, Lyra / Game Feature style.
3. Two local split-screen players must be able to have different options.
4. An options menu will come.

## Approved decisions
1. The options menu remaps the BASE buttons (attack, jump, shield, interact...) per local player, Smash-style.
   "B near an object = pick up" stays a design choice, not a player option.
2. Contextual functions (pick up, throw, kit swap) get no button of their own for now.
3. Per-player behaviour options (stick jump, shield hold/press) come later, slice IO-3 (~$5-8).
4. ONE GameRules asset per mode/level, with a piece per plugin (Held, Link...); exceptions live in the kit/object.
   The server picks it and sends it to everyone before the match.
5. Order: IO-1 ($4-6) -> GR-1 GameRules starting with Held ($6-9) -> IO-2 single button-layout asset replacing the
   Held/Link fields and the 6 C++ points ($6-9; a piece of GameRules) -> the other 10 plugins when next touched
   ($2-4 each) -> IO-3 and the menu.

## Slices
| Slice | What | Network |
|---|---|---|
| IO-1 | Per local player key -> Input Action bindings (Enhanced Input user settings), own save per player, subsystem API for the menu | none: local only |
| GR-1 | GameRules asset, Held piece first, chosen by the server and sent before the match | match setup only |
| IO-2 | Button-layout asset (logical button -> function) inside GameRules, replaces Held/Link fields and the 6 C++ points | same buttons, data driven |
| IO-3 | Per-player behaviour options (stick jump, shield hold/press) | travels as player option |
| Menu | Options UI over the IO-1 / IO-3 API | none |
