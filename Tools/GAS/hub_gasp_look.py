"""HUB-GASP-LOOK (owner 2026-11-19): L_Test_Hub gets the sky, lighting, fog, post process and grid textures of GASP's base level
(DefaultLevel), so animations and VFX are judged under the same light. Our gameplay actors stay.
  1. the hub's own Sun / Sky / Fog are deleted; GASP's LevelVisuals actor (directional light, sky light, height fog, post process,
     decal) is copied from DefaultLevel with its edited values (copy/paste through the editor clipboard; fallback: spawn the class);
  2. floors, ramps, steps and walls (MI_Hub_Blue / Grey walls / DarkBlue) take GASP's grid materials: MI_Grid_WorldAligned on floors and
     ramps, MI_Grid_Gray on walls and boards. Rulers, pads, KO zones and markers keep their colours (they carry information).
Saves only L_Test_Hub. Idempotent (a second run finds no Sun/Sky/Fog and one LevelVisuals). Report Saved/Hub/gasp_look.txt.
Headless: copy to a path without spaces, -run=pythonscript."""
import os
import unreal

OUT = unreal.Paths.project_saved_dir() + "Hub/gasp_look.txt"
GASP = "/Game/Levels/DefaultLevel"
HUB = "/Game/GSTN/Maps/L_Test_Hub"
VIS_CLASS = "/Game/Levels/LevelPrototyping/LevelVisuals.LevelVisuals_C"
FLOOR_MAT = "/Game/Levels/LevelPrototyping/Materials/MI_Grid_WorldAligned"
WALL_MAT = "/Game/Levels/LevelPrototyping/Materials/MI_Grid_Gray"
FLOOR_FROM = {"MI_Hub_Blue"}
WALL_FROM = {"MI_Hub_DarkBlue"}
WALL_LABELS = ("Z1_Wall", "Z3_Wall")            # grey walls (the other grey items are pads and columns: they keep their colour)
OLD_LIGHTS = ("DirectionalLight", "SkyLight", "ExponentialHeightFog", "SkyAtmosphere", "VolumetricCloud", "PostProcessVolume")
log = []
def p(*a): log.append(" ".join(str(x) for x in a))

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
try:
    les.load_level(GASP)
    vis = [a for a in eas.get_all_level_actors() if a.get_class().get_name() == "LevelVisuals_C"]
    p("gasp LevelVisuals", len(vis))
    # The instance's edited values travel as component properties (the clipboard does not work headless); read them before the level changes
    PROPS = ("intensity", "light_color", "relative_rotation", "relative_location", "fog_density", "fog_height_falloff", "fog_inscattering_luminance",
             "settings", "source_type", "cubemap", "temperature", "use_temperature", "volumetric_fog", "start_distance", "sky_atmosphere_ambient_contribution_color_scale")
    snap, vis_xf = {}, None
    if vis:
        vis_xf = (vis[0].get_actor_location(), vis[0].get_actor_rotation())
        for c in vis[0].get_components_by_class(unreal.ActorComponent):
            for prop in PROPS:
                try:
                    snap[(c.get_name(), prop)] = c.get_editor_property(prop)
                except Exception:
                    pass
    p("snapshot props", len(snap))
    copied = False
    les.load_level(HUB)
    acts = eas.get_all_level_actors()
    old_vis = [a for a in acts if a.get_class().get_name() == "LevelVisuals_C"]
    for a in acts:
        if a.get_class().get_name() in OLD_LIGHTS:
            p("delete", a.get_class().get_name(), a.get_actor_label()); eas.destroy_actor(a)
    if not old_vis:
        before = set(x.get_name() for x in eas.get_all_level_actors())
        if copied:
            eas.paste_selected_actors(unreal.EditorLevelLibrary.get_editor_world())
        new = [x for x in eas.get_all_level_actors() if x.get_name() not in before and x.get_class().get_name() == "LevelVisuals_C"]
        if not new:
            cls = unreal.load_class(None, VIS_CLASS)
            new = [eas.spawn_actor_from_class(cls, vis_xf[0] if vis_xf else unreal.Vector(0, 0, 0),
                                              vis_xf[1] if vis_xf else unreal.Rotator(0, 0, 0))]
            n_set = 0
            for c in new[0].get_components_by_class(unreal.ActorComponent):
                for (cn, prop), v in snap.items():
                    if cn == c.get_name():
                        try:
                            c.set_editor_property(prop, v); n_set += 1
                        except Exception:
                            pass
            p("LevelVisuals spawned from class, component values copied:", n_set)
        else:
            p("LevelVisuals pasted from DefaultLevel")
        new[0].set_actor_label("GASP_LevelVisuals")
    else:
        p("LevelVisuals already there")
    floor_mat, wall_mat = unreal.load_asset(FLOOR_MAT), unreal.load_asset(WALL_MAT)
    n_floor = n_wall = 0
    for a in eas.get_all_level_actors():
        if a.get_class().get_name() != "StaticMeshActor":
            continue
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            mats = c.get_materials()
            name = mats[0].get_name() if mats and mats[0] else ""
            if name in FLOOR_FROM:
                c.set_material(0, floor_mat); n_floor += 1
            elif name in WALL_FROM or a.get_actor_label() in WALL_LABELS:
                c.set_material(0, wall_mat); n_wall += 1
    p("floors", n_floor, "walls", n_wall)
    p("saved", les.save_current_level())
except Exception:
    import traceback; p("ERROR", traceback.format_exc())
os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w").write("\n".join(log))
