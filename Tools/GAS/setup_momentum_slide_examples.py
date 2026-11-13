# Momentum slide on the designer examples (owner 2026-11-13): ground attacks started while running slide on their feet along the momentum,
# differently when started against it. The data lives on each move (Timeline > Movement > Momentum Slide); this script only turns it on for
# two example moves of the ESEMPIO kit so it can be felt in L_Test_Hub, with the two "against" behaviours side by side:
#   GA_Example_Light    along: slide, fixed friction 1500 cm/s^2 (600 cm/s rests in ~120 cm)   against: hard brake (5000 cm/s^2, ~36 cm)
#   GA_Example_Strong   along: slide, character friction x0.25 (the kit's own braking)        against: Stop (the speed drops at once)
# Run after setup_designer_examples.py, headless (editor closed):
#   UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this file>"   or   import setup_momentum_slide_examples as m; m.run()
# Idempotent: a move whose slide is already enabled is left alone (the designer's numbers stay); run(force=True) writes these values again.
# Output: Saved/Examples/momentum_slide.log, at most 5 printed lines.
import os

import unreal

DIR = '/Game/GASNPPTest/Examples'

# name -> (along fields, against fields); everything else keeps the struct defaults (MinSpeed 150, threshold -0.3, stop at ledge, root motion added)
EXAMPLES = {
    'GA_Example_Light': (
        dict(mode=unreal.GSTN_MomentumSlideMode.SLIDE, friction=unreal.GSTN_MomentumSlideFriction.FIXED, deceleration=1500.0),
        dict(mode=unreal.GSTN_MomentumSlideMode.SLIDE, friction=unreal.GSTN_MomentumSlideFriction.FIXED, deceleration=5000.0)),
    'GA_Example_Strong': (
        dict(mode=unreal.GSTN_MomentumSlideMode.SLIDE, friction=unreal.GSTN_MomentumSlideFriction.CHARACTER, friction_scale=0.25),
        dict(mode=unreal.GSTN_MomentumSlideMode.STOP)),
}

_lines = []


def log(msg):
    _lines.append(str(msg))
    unreal.log(f'[momentum_slide] {msg}')


def _response(response, fields):
    for prop, value in fields.items():
        response.set_editor_property(prop, value)
    return response


def run(force=False):
    del _lines[:]
    written = 0
    for name, (along, against) in EXAMPLES.items():
        path = f'{DIR}/{name}'
        cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
        if not cls:
            log(f'ERROR: {name} missing (run setup_designer_examples.py first)')
            continue
        cdo = unreal.get_default_object(cls)
        timeline = cdo.get_editor_property('timeline')
        slide = timeline.get_editor_property('momentum_slide')
        if slide.get_editor_property('enabled') and not force:
            log(f'{name}: already enabled, left alone')
            continue
        slide.set_editor_property('enabled', True)
        slide.set_editor_property('along', _response(slide.get_editor_property('along'), along))
        slide.set_editor_property('against', _response(slide.get_editor_property('against'), against))
        timeline.set_editor_property('momentum_slide', slide)
        cdo.set_editor_property('timeline', timeline)
        if unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
            written += 1
            log(f'{name}: slide enabled and saved')
        else:
            log(f'ERROR: {name}: could not save')
    log(f'done, {written} written')
    out_dir = os.path.join(unreal.Paths.project_saved_dir(), 'Examples')
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, 'momentum_slide.log'), 'w') as f:
        f.write('\n'.join(_lines) + '\n')
    return _lines[-5:]


if __name__ == '__main__':
    for line in run():
        print(line)
