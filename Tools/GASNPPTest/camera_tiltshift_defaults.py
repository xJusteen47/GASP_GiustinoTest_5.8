# Puts the lighter tilt-shift defaults (commit aca5d77a, owner 2026-11-18: the blur was too strong) on DA_CameraAnchor_Default.
import unreal

PATH = '/Game/Blueprints/Data/DA_CameraAnchor_Default'
VALUES = {'fstop': 4.0, 'sensor_width': 120.0, 'band_max_radius': 3.0, 'band_half_height': 0.25, 'band_feather': 0.35, 'intensity': 1.0}

da = unreal.load_asset(PATH)
done = []
for prop in ('settings', 'anchor', 'camera', 'anchor_settings'):
    try:
        holder = da.get_editor_property(prop)
    except Exception:
        continue
    ts = holder.get_editor_property('tilt_shift')
    for k, v in VALUES.items():
        ts.set_editor_property(k, v)
    holder.set_editor_property('tilt_shift', ts)
    da.set_editor_property(prop, holder)
    done.append(prop)
    break
if not done:
    ts = da.get_editor_property('tilt_shift')
    for k, v in VALUES.items():
        ts.set_editor_property(k, v)
    da.set_editor_property('tilt_shift', ts)
    done.append('tilt_shift')
unreal.EditorAssetLibrary.save_loaded_asset(da, only_if_is_dirty=False)
print('tiltshift defaults set via ' + done[0])
