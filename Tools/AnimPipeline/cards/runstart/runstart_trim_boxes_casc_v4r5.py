# Run start v4r5, step 2 of the trim (after runstart_trim_casc_v4r5.py, which moves only the rig POINTS): the FK boxes (fingers, twist
# boxes, pelvis/spine boxes) and the spine "spline offset box" objects carry their own local rotation/position keys (fists, chest and
# head restyle). Copy them from the open RunStart_v4r4.casc tab at f+SHIFT to RunStart_v4r5.casc at f. Then the bone check
# (every bone v4r5 f == v4r4 f+SHIFT) must be ~0. Run in Cascadeur (run_python, exec) with both tabs open, v4r5 current.
import numpy as np

SHIFT = int(globals().get('SHIFT_OVERRIDE', 10))
N = 59
sm = app.get_scene_manager()
tabs = {str(s.get_path_name()).replace("\\", "/").split('/')[-1]: s for s in sm.scenes()}
src_t, dst_t = tabs['RunStart_v4r4.casc'], tabs['RunStart_v4r5.casc']
assert sm.current_scene() is not None and str(sm.current_scene().get_path_name()).endswith('RunStart_v4r5.casc')

def handles(tab):
    sc = tab.domain_scene(); mv = sc.model_viewer(); bv = mv.behaviour_viewer()
    h = {}
    for o in mv.get_objects():
        n = mv.get_object_name(o)
        if not n or not ('_Box' in n or 'offset box' in n):
            continue
        try:
            tr = bv.get_behaviour_by_name(o, 'Transform')
            h[n] = (bv.get_behaviour_data(tr, 'local_rotation'), bv.get_behaviour_data(tr, 'local_position'))
        except Exception:
            pass
    return sc, h

ssc, sh = handles(src_t)
dsc, dh = handles(dst_t)
sdv = ssc.model_viewer().data_viewer()
names = sorted(set(sh) & set(dh))
vals = {f: {n: (sdv.get_data_value(sh[n][0], f + SHIFT), sdv.get_data_value(sh[n][1], f + SHIFT)) for n in names} for f in range(N)}

def change(me, ue, su):
    de = me.data_editor()
    ids = set()
    for f in range(N):
        for n in names:
            r, p = vals[f][n]
            de.set_data_value(dh[n][0], f, r)
            de.set_data_value(dh[n][1], f, p)
            ids.add(dh[n][0]); ids.add(dh[n][1])
        su.run_update(ids, f)

ok = dsc.modify_update('runstart v4r5 trim boxes', change)
result = {'ok': bool(ok), 'boxes': len(names)}
