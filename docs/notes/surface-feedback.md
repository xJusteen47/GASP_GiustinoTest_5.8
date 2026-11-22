# Surface feedback: footwear x floor footsteps + pad rumble (2026-11-21)

Implements docs/notes/research/surface-audio.md and haptics.md (branch claude/project-thread-w299nb). Owner decisions there: two axes,
FOOTWEAR per skin (Clay = stock GASP, Metal, Barefoot, Shoe sole) x FLOOR (Physical Material surface type); one layered MetaSound per
event family; stock GASP sounds first. Code: branch claude/project-thread-c2dfra (base claude/runstart-generic-desktop 8cb52ea7).

## What the code does (GSTNPresentationCore)
- `UGSTN_SurfaceResponseTable` (DataAsset): rows `(EventTag, Footwear | any, Floor | any)` -> Sound (layered MetaSound) + volume,
  random pitch, layer gains, attenuation, concurrency + rumble (dynamic pulse: intensity, duration, motors, left/right foot; or a Force
  Feedback Effect asset). Magnitude -> intensity with the same `FGSTN_PresentationScale` as the VFX table. Lookup: nearest event tag
  first, then footwear, then floor; Parent table; no row = stock sound, no rumble. Tests `GSTNPresentation.Core.Surface.*`.
- `UGSTN_SurfaceFeedbackComponent`: floor trace (only if a row of that event names a floor), footwear (cvar > component override >
  skin > Clay), plays the row. Inputs: `HandleFoleyEventOn(Actor, FoleyTag, Location, Side)` for the stock foley notify (true = skip the
  stock sound), and every accepted presentation event (the presentation component adds it at BeginPlay and forwards Land, Hit,
  Shield.Hit...). Same-foot repeats within 0.12 s play once (MM blend doubles). Rumble only on the local controlling player's pad;
  nothing on a dedicated server. BP event `OnSurfaceResponse`.
- Skin: `UGSTN_SkinDefinition::Footwear` (tag); `UGSTN_SkinComponent` pushes it on apply, clears it on restore.
- Table asset path in `Config/DefaultGame.ini` (`ProjectDefaultTable`, `/Game/GSTN/Audio/DA_GSTN_SurfaceResponse_Default`).

## MetaSound inputs (names are fixed in code)
`Footwear` int (table's FootwearParamOrder; default Clay 0, Metal 1, Barefoot 2, ShoeSole 3), `Floor` int (EPhysicalSurface: 0 Default,
1 = SurfaceType1...), `Side` int (0 none, 1 left, 2 right), `LayerMode` int (0 both, 1 footwear, 2 floor), `Speed` float (cm/s or
the event's unit), `Weight` float 0-1 (component), `Intensity` float, `FootwearGain` float, `FloorGain` float (0 when LayerMode mutes it).

## Debug Options (GSTN Debug Options window, groups Audio and Haptics, one control each)
`gstn.Audio.Surface` check (1), `gstn.Audio.FootwearOverride` number (-1 skin, 0-3), `gstn.Audio.LayerMode` number (0-2),
`gstn.Audio.SurfaceDebug` number (0 off, 1 print, 2 draw trace), `gstn.Haptics.Enable` check (1), `gstn.Haptics.Intensity` number (0-3, 2).

## Desktop thread: steps (editor, ~45-60 min, budget ~5 $)
1. Lock `_ue59/IN-USO.txt`. Merge this branch into the playtest branch, real incremental build (Run-Timed script). Expected: build OK;
   then tests `GSTNPresentation.Core.Surface` and `GSTNCombat.Debug.OptionsWindow` green. If `PlayDynamicForceFeedback` (6-argument
   native overload of APlayerController) does not compile in 5.9, ask the Unreal Assistant for its 5.9 name and fix that one line.
2. Run `Tools/Audio/create_surface_feedback_assets.py` in the editor. Expected print: `SURFACE TABLE created: /Game/GSTN/Audio/... rows=8`.
3. Ask the Unreal Assistant (rule 14): "In GASP 5.9 BP_AnimNotify_FoleyEvent, where is the footstep sound spawned, and does it call an
   interface function on the owner first?" Save the reply in docs/notes/research/unreal-assistant-foley-hook.md.
4. Hook (one node, divergence row 24): where the stock graph spawns the sound (prefer the pawn-side interface function of our
   SandboxCharacter_Mover if the notify calls one; else inside BP_AnimNotify_FoleyEvent `Received_Notify`), add BEFORE the spawn:
   `Handle Foley Event On` (Owner = mesh owner, Foley Event = the notify's tag, Location = the foot location the stock graph uses or
   zero, Side = the notify's side mapped to None/Left/Right) -> Branch: True = skip the stock spawn, False = stock spawn unchanged.
   Tell the owner before editing the shared Blueprint (CLAUDE.md). Back up the asset first.
5. PIE, `gstn.Audio.SurfaceDebug 1`: walking prints `SURF ... Foley.Event.Run footwear=...Clay ... sound=- rumble=0.12`, the stock
   sound is unchanged (row has no Sound). With an Xbox pad: light ticks per step, thump on landing. `gstn.Audio.FootwearOverride 1`
   (Metal): stronger ticks on both motors.
6. MetaSound `MSS_GSTN_Footstep` in /Game/GSTN/Audio (first describe it to the Unreal Assistant, owner rule for new effects):
   inputs as above; two Wave Players: footwear layer = WaveAsset array picked by `Footwear` (start: the stock GASP run footstep waves,
   with pitch/EQ per footwear: Metal +2-4 st + short ring, Barefoot -2 st low-pass, ShoeSole +1 st high shelf; Clay = stock as is),
   floor layer = array picked by `Floor` (start: one stock texture, the default floor); each layer x its gain, random pitch +-1 st;
   output gain x Intensity. Concurrency asset `SCON_GSTN_Footsteps` (max 6, stop quietest). One-layer variant later for Switch 2.
7. Put `MSS_GSTN_Footstep` in the Sound column of the Foley.Event.Run/Walk rows for Metal, Barefoot and ShoeSole only (Clay keeps the
   stock sound: no row change). Re-running the script keeps the Sound column.
8. Optional: Physical Surfaces in Project Settings (Stone, Grass, Wood, Metal, Sand) + Physical Materials on L_Test_Hub floors, then
   floor rows. Only if the owner wants floor differences now.
9. Capture is audio + rumble: no video review (rule 10 is for animation). Ask the owner to try it on the pad; build again, free the lock.

## Open (da confermare, for the coordinator)
- Hit rumble is on whatever character the presentation Hit event fires for (victim side). Attacker hit-confirm rumble may need its own event.
- Landing: rumble from Presentation.Event.Land (fall speed), sound from the stock Foley.Event.Land notify. Avoid rows with sound on both.
- `GetVelocity()` on the Mover pawn is assumed to return the moving speed (Speed input / Foley magnitude); check the debug print.
