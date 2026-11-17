"""Polish M2: the project's default presentation table /GSTNPresentation/VFX/DA_GSTN_Presentation_Default (event -> Niagara of the kit v1).

Rows are data (polish plan section 1: every VFX is a row, never code): a kit or a skin puts its own table over this one and names
only what it changes. DefaultGame.ini points UGSTN_PresentationComponent::ProjectDefaultTable here, so every fighter uses it.
Event location = the actor (capsule centre): ground effects are offset down to the feet (FEET). Magnitudes: tag comments in
GSTN_PresentationTags.cpp (Land = fall speed cm/s, Hit = hitstun frames, StageUp = new stage, bounces = speed into the surface).
Run in the open editor after setup_presentation_vfx.py: python -I Tools/GAS/run_in_editor.py setup_presentation_table
"""
import unreal

ROOT = "/GSTNPresentation/VFX"
PATH = f"{ROOT}/DA_GSTN_Presentation_Default"
FEET = -88.0  # capsule half height of the GASP character
EV = "Presentation.Event."
ACCENT = unreal.LinearColor(1.0, 0.044, 0.028, 1.0)  # #FF3B2F in linear: the default kit's second colour (red over the hit's orange)
SPRINT2_STAGE = 3  # run stages 1 run, 2 sprint 1, 3 sprint 2 (FGSTN_RunStagesTuning::MakeDefault)


def _tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(f'(TagName="{EV}{name}")')
    return tag


def _tags(names):
    box = unreal.GameplayTagContainer()
    box.import_text("(GameplayTags=(" + ",".join(f'(TagName="{EV}{n}")' for n in names) + "))")
    return box


def _cue(event, system, min_mag=0.0, scale=None, socket="", offset=(0, 0, 0), orient=False, scale_size=True, stop_on=(), max_duration=0.0,
         strong=0.0, accent=None):
    cue = unreal.GSTN_PresentationCue()
    cue.set_editor_property("event_tag", _tag(event))
    cue.set_editor_property("min_magnitude", float(min_mag))
    if scale:
        s = unreal.GSTN_PresentationScale()
        s.set_editor_property("input_range", unreal.Vector2D(*scale[0]))
        s.set_editor_property("output_range", unreal.Vector2D(*scale[1]))
        cue.set_editor_property("scale", s)
    cue.set_editor_property("niagara", unreal.load_asset(f"{ROOT}/{system}"))
    cue.set_editor_property("attach_socket", socket)
    cue.set_editor_property("offset", unreal.Vector(*offset))
    cue.set_editor_property("orient_to_direction", orient)
    cue.set_editor_property("scale_size", scale_size)
    cue.set_editor_property("stop_on", _tags(stop_on))
    cue.set_editor_property("max_duration", float(max_duration))
    cue.set_editor_property("strong_intensity", float(strong))
    if accent:
        cue.set_editor_property("accent_tint", accent)
    return cue


def rows():
    feet = (0, 0, FEET)
    return [
        _cue("RunStart", "NS_GSTN_RunStartDust", offset=feet, orient=True),
        _cue("Skid.Enter", "NS_GSTN_SkidDust", socket="root", scale_size=False,
             stop_on=("Skid.Exit", "Jump", "Hit", "Splat", "Knockdown", "Tumble.Start"), max_duration=1.5),
        _cue("Jump.Ground", "NS_GSTN_JumpPuff", offset=feet),
        _cue("Land", "NS_GSTN_LandRing", min_mag=300, scale=((600, 2500), (0.7, 1.5)), offset=feet),  # a plain jump lands at ~1300 cm/s
        _cue("StageUp", "NS_GSTN_SpeedLines", min_mag=SPRINT2_STAGE, socket="root", scale_size=False,
             stop_on=("StageDown", "Skid", "Hit", "Splat", "Knockdown", "Tumble.Start")),
        # Hit sizes from the hitstun (frames): 28 or less = small (0.5), ~43 = medium (0.8), 58+ = large (1.1); intensity is also the share
        # of the 8 shards that spawn (4 / 6 / 8) and the sprite size scale. Measured 2026-11-17 (KIT_Test_Hits jab): 31 frames at 0 % (0.56),
        # 40 at 90 % (0.74), 67 at 170 % (1.1). From 1.0 the hit is strong: 3 accent stars in the kit's second colour (a kit row overrides it)
        _cue("Hit", "NS_GSTN_HitSpark", scale=((28, 58), (0.5, 1.1)), orient=True, strong=1.0, accent=ACCENT),
        _cue("WallBounce", "NS_GSTN_Impact", scale=((200, 1500), (0.7, 1.3)), orient=True),
        _cue("GroundBounce", "NS_GSTN_Impact", scale=((200, 1500), (0.7, 1.3)), offset=feet, orient=True),
        _cue("Splat", "NS_GSTN_Impact", scale=((200, 1500), (0.8, 1.4)), orient=True),
        # The downed landing after a tumble: the landing ring at the feet, a little bigger (no magnitude: intensity 1 -> 1.2)
        _cue("Knockdown", "NS_GSTN_LandRing", scale=((0, 1), (1.2, 1.2)), offset=feet),
        _cue("Tumble.Start", "NS_GSTN_TumbleTrail", socket="pelvis", scale_size=False,
             stop_on=("Tumble.End", "Knockdown", "GetUp", "Splat", "Land"), max_duration=3.0),
    ]


def run():
    if unreal.EditorAssetLibrary.does_asset_exist(PATH):
        table = unreal.load_asset(PATH)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.GSTN_PresentationTable)
        table = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_GSTN_Presentation_Default", ROOT,
                                                                         unreal.GSTN_PresentationTable, factory)
    cues = rows()
    table.set_editor_property("cues", cues)
    unreal.EditorAssetLibrary.save_loaded_asset(table)
    return f"{len(cues)} rows in {PATH}"
