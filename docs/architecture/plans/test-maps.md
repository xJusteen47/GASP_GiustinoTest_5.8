# Test maps: movement, combat, enemies (build briefs)

Status: **plan, 2026-10-23.** Owner request (verbatim): "Dopo le milestones relative, sarebbe bello avere delle test map per il combat, il
movimento e i nemici. Si puo usare qualsiasi mesh o animazione, anche primitive. Tanto è solo per testare." and "per quello pero ti faro
aiutare da ludus AI". So each map below is a **brief that an editor AI assistant (Ludus AI) or a person can follow** step by step. It is not
code we write. Each brief has a "build after" point, a layout of numbered stations, and an acceptance checklist.

These maps compose many features in one place. They do not replace each plugin's own minimal map (modularity-and-tags.md §7.1), which stays inside
the plugin and must work with that plugin alone.

## 0. Rules for every test map

1. **Location and names.** Each map goes in `Content/Levels/Test/` and is called `L_Test_<Name>`; its helper assets go in
   `Content/GASNPPTest/TestMaps/<Name>/`. Both folders are versioned through Git LFS, without locks (CLAUDE.md).
2. **Art.** Use engine primitives (`/Engine/BasicShapes/Cube`, `Cylinder`, `Plane`), the engine grid material, simple lights (one directional
   light, a sky light, an exponential height fog) and the sample's existing mannequin and animations. No new art. Colour code only:
   - blue = walkable;
   - orange = wall-slide surface;
   - red = KO zone;
   - green = trampoline;
   - yellow = target or dummy.
3. **Labels.** Every station gets a number and a `TextRender` label ("3: jump gaps 2–6 m"). One board near the start lists all stations.
4. **Measures.** Floors carry 1 m marks (thin cubes or a grid). Heights carry a vertical ruler (a 10 cm thick column with 1 m marks).
   Numbers in a station name are the real sizes in the level.
5. **Players.** Each map has four `PlayerStart`s 3 m apart: a listen host plus three clients. World settings use the project's test game mode.
6. **Do not touch shared things.** Do not change C++, plugin content, the project's character, game mode or input Blueprints. Tell the owner
   before editing any shared Blueprint (CLAUDE.md). New behaviour comes only from placing actors that already exist in the plugins: KO zones,
   trampolines, spawners, camera sections, reference volumes, pickups. If one is missing, write it in the map's notes. Do not build it.
7. **Network.** Every map must run under the harness (`Tools/GASNPPTest/gasnpp_test.py`, testing.md) with a listen host and two clients.
   Moving geometry is a simple kinematic mover that is the same on every machine, or it is left out.
8. **Done means:** the acceptance checklist is ticked, the map loads with no errors in the message log, and Data Validation is clean on the
   new assets.

## TM1. `L_Test_Movement`

**Build after:** the wall movement core (plans/README.md T5) and the jump. Extend it after traversal surfaces (T1–T4) and later with camera
sections (camera.md C3) and the depth marker.

| # | Station | Exact layout | Tests |
|---|---|---|---|
| 1 | Run lane | 40 m straight, 4 m wide, 1 m marks | walk, run, stop distance |
| 2 | Steps | blocks 30, 45, 60, 90, 120, 150, 200 cm high, 2 m apart | step-up limit, jump height |
| 3 | Jump gaps | platforms 3 m long with gaps of 2, 3, 4, 5, 6 m, same height | jump distance |
| 4 | Height gaps | platforms rising by 1, 2, 3 m | jump up and drop down |
| 5 | Slopes | ramps of 15°, 30°, 45°, 50°, 60°, each 6 m long | walkable angle, sliding |
| 6 | Wall slide | orange walls 3, 6 and 10 m high; a vertical one, one at 80° and one at 100° (overhang) | wall slide entry, speed, exit |
| 7 | Wall corners | 90° inner corner, 90° outer corner, a 135° corner, a thin 50 cm pillar | corner slowdown and wall slide v2 (wall-slide-v2.md) |
| 8 | Wall bumps | a 10 m wall with 10, 20 and 40 cm ledges sticking out | bump step-up while sliding |
| 9 | Stacked platforms | three platforms stacked at 2, 4 and 6 m with 1.5 m overlap; a 20 cm thin platform | depth marker below, edge hold |
| 10 | Pit | a 10 m wide gap with a red KO zone 15 m below, plus a depth reference volume at the lip | KO, above ring of the depth marker |
| 11 | Mover lift | a 4×4 m platform moving 6 m up and down every 4 s (only if a kinematic mover exists) | standing on moving bases |
| 12 | Gravity | a gravity-flip volume and a sideways-gravity volume (only once gravity volumes exist) | arbitrary gravity |
| 13 | Camera | camera sections: one fixed angle with manual control locked, one rail, one orbit with limits (after camera C3) | camera sections, stick hold on cuts |

**Acceptance:**
- [ ] Every station is reachable from the start without cheats.
- [ ] Wall slide starts on every orange wall at 6 and 7. It never starts on blue floors or on the 60° ramp.
- [ ] Jump distances at 3 match the tuning: write down the largest gap cleared.
- [ ] Falling into 10 triggers a KO and the respawn.
- [ ] A host and two clients run every station with no visible corrections, measured with the harness's correction count.

## TM2. `L_Test_Combat`

**Build after:** M4 H13. It grows the small test map that H13 already asks for. Add stations after M4b (parts), M6 (shield, parry, grabs),
M10 (held objects) and M11 (pickups).

| # | Station | Exact layout | Tests |
|---|---|---|---|
| 1 | Dummy row | five yellow dummies 4 m apart: light, medium and heavy weight; one with a high stagger threshold; one with super armour | hit, hitstun, knockback tiers, stagger resistance |
| 2 | Knockback lane | 30 m lane with 1 m marks in front of one medium dummy, a 10 m height ruler beside it | launch distance and height, DI |
| 3 | Damage scale | three medium dummies with health preset at 0 %, 50 % and 100 % of the cap (if the test tools allow) | weight dropping with damage (`WeightDriven`) |
| 4 | Walls and glass | a concrete wall, a glass panel and a bouncy wall, each 3 m from a dummy | splat, bounce, destruction |
| 5 | KO zone | an arena edge over a red KO zone | ring-out KO |
| 6 | Trampolines | three green trampolines with different launch strengths | launch surfaces (H11b) |
| 7 | Ally dummy | a dummy on the player's team | `Ally` variant (half shape, no hitstun) |
| 8 | Parts dummy (after M4b) | a large dummy with a hard part (ricochet), a weak point and a breakable part | hardness vs penetration, ricochet, part break |
| 9 | Duel arena (after M6) | a 20 m round arena with KO edges, two `PlayerStart`s facing each other | shield, parry, grabs, Tekken-style duel camera (camera.md §2.5) |
| 10 | Long weapons (after M11) | a dummy row at 1 m steps from 1 to 5 m, plus kit pickups for the base sword and the long-blade mod | reach scaling, sweet and sour spots |
| 11 | Objects and pickups (after M10/M11) | throwable objects, a pile of 30 weapon pickups within 3 m | hold to pick up, interaction focus among many items |

**Acceptance:**
- [ ] Every dummy reacts as its label says: light dummies launch, heavy ones push back, super armour takes no hitstun.
- [ ] Knockback at 2 is the same on host and clients, within the harness tolerance.
- [ ] Every station has a label stating its expected result.
- [ ] The H13 harness scenarios run on this map.

## TM3. `L_Test_Enemies`

**Build after:** M8 (minions). Add the parts stations after M4b P16, and the platform enemies once `GSTNPlatform` exists.

| # | Station | Exact layout | Tests |
|---|---|---|---|
| 1 | Spawner pads | one spawner pad per minion type, 6 m apart, each with a "spawn one" trigger | each type's behaviour |
| 2 | Bench area | a 60×60 m flat arena with spawners for 40, 60 and 150 minions, behind a wall the player can toggle | listen-host cost (M8 bench), awake vs dormant |
| 3 | Navigation | ramps, a 2 m gap, a narrow bridge, stairs | enemy pathing |
| 4 | Platform enemy | a large enemy whose back is a platform and whose flank is a wall-slide surface | standing on enemies, wall slide on enemy parts |
| 5 | Parts boss (after P16) | a big enemy with breakable parts and weak points | parts on minions and fighters |
| 6 | Flyers | a flying enemy over a pit | depth marker on enemies |
| 7 | Throw lane | a KO pit plus a wall, 10 m from a grabbable enemy | grabbing and throwing enemies (M6, M10) |

**Acceptance:**
- [ ] Every type spawns and attacks.
- [ ] The bench at 2 reports host cost and upload in the harness.
- [ ] No enemy falls through the platform enemy at 4.
- [ ] The host and two clients see the same enemy states.

## Hand-off to Ludus AI

Give it one brief at a time: section 0 plus one map section. Ask it to build stations in order, label them, and stop when an actor it
needs does not exist. It writes the missing items in a `Notes` text actor at the start of the map, for the owner to see. The owner or a
thread then runs the acceptance checklist with the harness.
