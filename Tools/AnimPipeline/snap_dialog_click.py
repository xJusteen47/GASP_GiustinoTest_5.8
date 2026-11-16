# Cascadeur: press TARGET ("All Frames" / "Only keys") on every open "Apply AutoPhysics" dialog window, then (deferred)
# report how much jab_snap_test.casc moved against jab_v7.casc. Output prefix SNAPCLICK in the Cascadeur log.
import csc, json
import numpy as np
from PySide6 import QtCore, QtGui, QtQuick
import shiboken6
TARGET = globals().get("TARGET") or "All Frames"
app = csc.app.get_application(); sm = app.get_scene_manager()

def quick_windows():
    out = []
    for w in QtGui.QGuiApplication.topLevelWindows():
        if not w.isVisible():
            continue
        qw = shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow)
        if qw.inherits("QQuickWindow"):
            out.append(qw)
    return out

def press(win, item):
    for m in ("click", "clicked"):
        try:
            if QtCore.QMetaObject.invokeMethod(item, m):
                return m
        except Exception:
            pass
    c = item.mapToScene(QtCore.QPointF(item.width() / 2, item.height() / 2))
    for t in (QtCore.QEvent.MouseButtonPress, QtCore.QEvent.MouseButtonRelease):
        ev = QtGui.QMouseEvent(t, c, c, QtCore.Qt.LeftButton, QtCore.Qt.LeftButton if t == QtCore.QEvent.MouseButtonPress else QtCore.Qt.NoButton, QtCore.Qt.NoModifier)
        QtGui.QGuiApplication.sendEvent(win, ev)
    return "mouse"

pressed = []
for w in quick_windows():
    if w.title() != "Apply AutoPhysics":
        continue
    stack = [w.contentItem()]
    while stack:
        it = stack.pop(); stack.extend(it.childItems())
        if str(it.property("text") or "") == TARGET and "DialogButton" in it.metaObject().className():
            pressed.append({"window": w.title(), "via": press(w, it)})
            break
print("SNAPCLICK pressed " + json.dumps(pressed))

def diff():
    a = [s for s in sm.scenes() if s.name() == "jab_snap_test.casc"][0].domain_scene()
    b = [s for s in sm.scenes() if s.name() == "jab_v7.casc"][0].domain_scene()
    def pos(ds, name, f):
        mv = ds.model_viewer(); dv = ds.data_viewer()
        o = [x for x in mv.get_objects() if mv.get_object_name(x) == name][0]
        for d in dv.get_all_data_id(o):
            try:
                if dv.get_data_name(d) == "Position":
                    return np.array(dv.get_data_value(d, f))
            except Exception:
                pass
    rep = {}
    for n in ("pelvis_MainPoint", "spine_04_MainPoint", "head_MainPoint", "hand_MainPoint_l", "hand_MainPoint_r", "foot_MainPoint_l"):
        ds_ = [float(np.linalg.norm(pos(a, n, f) - pos(b, n, f))) for f in range(0, 38)]
        rep[n] = {"max_cm": round(max(ds_), 2), "at": int(np.argmax(ds_)), "frames_moved": [f for f, d in enumerate(ds_) if d > 0.05][:40]}
    still_open = [w.title() for w in quick_windows() if w.title() == "Apply AutoPhysics"]
    print("SNAPCLICK diff " + json.dumps({"diff_vs_v7": rep, "dialogs_still_open": len(still_open)}))
QtCore.QTimer.singleShot(2500, diff)
