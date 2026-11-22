# GSTN Presentation

Presentation events of a character and the table of their effects (polish plan M0, `handoffs/polish-movimento-vfx-piano.md`).
Presentation only: nothing here is read by a simulation, nothing is replicated. Depends on GSTNAnimation (short additives) and Niagara.

## Pieces

| Piece | What it does |
|---|---|
| `Presentation.Event.*` (native, `GSTN_PresentationTags.h`) | Jump(.Ground/.Air), Land, RunStart, StageUp, StopRelease, Skid(.Enter/.Loop/.Exit), Turn180, Hit, Splat, WallBounce, GroundBounce, Bump(.Wall/.Ceiling), Knockdown, GetUp, Roll, Dodge, Shield(.On/.Hit/.Break). The tag comment gives the unit of the magnitude. |
| `UGSTN_PresentationTable` (DataAsset) | Rows `FGSTN_PresentationCue`: event tag -> Niagara (world or socket, oriented, scaled, `User.Intensity`), sound (volume), camera shake (view target or world radius), short additive (through the directional additive node). Threshold `MinMagnitude` and scale (input range -> intensity, exponent). A row for a parent tag answers its children; `Parent` table = fallback (skin -> kit -> default). |
| `FGSTN_PresentationDeriver` | Pure: two consecutive presented snapshots -> events (edges of grounded, run stage, skid phase, the simulation's bounce counts with reaction velocity reflections as fallback, last hit frame, tag rules). |
| `FGSTN_PresentationDedupe` | Rollback guard: once per (event, frame), +-`DedupeFrameWindow` (2) for edges read from presented state, exact frame for simulation stamps (hits, move starts). |
| `UGSTN_PresentationComponent` | Per character. `SubmitSnapshot`, `ReportEvent`, table stack (`SetTableLayer(Source, Table, Priority)` over `DefaultTable`), effects (not on a dedicated server), Blueprint `OnPresentationEvent` and `BindEventByTag` (one Blueprint event per type). |

The fighter adapter is in GSTNCombatNP: `UGSTN_CombatPresentationEvents` (added by `UGSTN_CombatSimComponent` at BeginPlay) fills the
snapshot from Mover + the combat state view + ASC owned tags, and keeps the "Kit" layer on the kit's `FGSTN_PresentationKitExtension`.

## Online
Every machine derives from its own presented state, so the Blueprint events fire on the owner, the server and every client without RPCs.
A correction can show an edge twice: the dedupe drops it. An edge moved more than the window by a correction plays twice (accepted).

## Tag rules (data, `DeriveRules.TagRules`)
Defaults: `Combat.State.Shielding` -> Shield.On, `Combat.State.ShieldStun` -> Shield.Hit, `Combat.State.ShieldBroken` -> Shield.Break,
`Combat.State.Knockdown` -> Knockdown (added) / GetUp (removed). Roll and Dodge need the tags of those abilities (rule with `bMoveTag`).

## Tests
`GSTNPresentation.Core.*` (scale, lookup, dedupe, derive air/run/tags, resimulation).
