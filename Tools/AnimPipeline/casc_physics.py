# Cascadeur: AutoPhysics settings from a script (run in Cascadeur: the cascadeur-plus run_python tool, or csc_run.py).
# PHYSICS = path of a JSON plan {"tab": <scene file name>, "step": "set" | "verify", ...}:
#   toggles  {"Smooth trajectory": false, ...}: the Physics settings panel switches (Physics corrector, Smooth trajectory,
#            Smooth rotation, Compensation motion, Separation of motion, Secondary motion). They are per-scene settings
#            with no Python setter (cascadeur_api_verified.md 5, 6c): the script opens the "Physics settings" side tab,
#            finds the label's row by its Off/On CheckButtons (the per-point "Secondary motion" header in Object properties
#            has none), scrolls it into view and sends a synthetic mouse click on the wanted button. Emitting `clicked` on
#            the CheckButton does nothing (tried 2026-11-03); a real press/release does. The setting changes only after the
#            event loop runs: call again with "step": "verify" to read the values back.
#   points   {"<object name substring>": {"Behaviour.property": value}}: per-point AutoPhysics data on every frame
#            (cascadeur_api_verified.md 4), e.g. {"hand_": {"CompensationMotion.muscle_stiffness": 15}}.
#   snap     "All Frames" | "Only keys": Snap to AutoPhysics (6b). It MOVES the key poses, so it is refused unless the
#            plan says "allow_key_changes": true and the tab is a copy ("_physics" or "_spacelab" in its path): never on
#            a scene that holds the owner's locked poses.
#   save     true: save the tab to its own file at the end of "set" (toggles are stored in the .casc).
import csc, json
from PySide6 import QtCore, QtGui, QtQuick
import shiboken6
P = json.load(open(globals()["PHYSICS"]))
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == P["tab"]][0]
sm.set_current_scene(tab)
sh = tab.get_setting_handler()
TOGGLES = ("Physics corrector", "Smooth trajectory", "Smooth rotation", "Compensation motion", "Separation of motion", "Secondary motion")


def windows():
    for w in QtGui.QGuiApplication.topLevelWindows():
        if w.isVisible():
            qw = shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow)
            if P["tab"] in qw.title():
                yield qw


def items(qw):
    stack = [qw.contentItem()]
    while stack:
        it = stack.pop(); stack.extend(it.childItems()); yield it


def click(qw, it):
    c = it.mapToScene(QtCore.QPointF(it.width() / 2, it.height() / 2))
    for t, b in ((QtCore.QEvent.MouseButtonPress, QtCore.Qt.LeftButton), (QtCore.QEvent.MouseButtonRelease, QtCore.Qt.NoButton)):
        QtGui.QGuiApplication.sendEvent(qw, QtGui.QMouseEvent(t, c, qw.mapToGlobal(c), QtCore.Qt.LeftButton, b, QtCore.Qt.NoModifier))
    return [round(c.x()), round(c.y())]


def open_panel(qw):
    for it in items(qw):
        if str(it.property("text") or "") == "Physics settings" and "TabButton" in it.metaObject().className() and it.isVisible():
            return click(qw, it)


def visible_in_flickable(it):
    """Scroll the nearest Flickable ancestor so the item is inside its viewport; True if it is (now) inside."""
    f = it.parentItem()
    while f is not None and f.property("contentY") is None:
        f = f.parentItem()
    if f is None:
        return True
    top = it.mapToItem(f, QtCore.QPointF(0, 0)).y()
    if top < 0 or top + it.height() > f.height():
        f.setProperty("contentY", max(0.0, float(f.property("contentY")) + top - f.height() / 3))
        QtCore.QCoreApplication.processEvents()
        top = it.mapToItem(f, QtCore.QPointF(0, 0)).y()
    return 0 <= top and top + it.height() <= f.height()


def row_button(qw, label, want):
    for it in items(qw):
        if str(it.property("text") or "") != label or not it.isVisible():
            continue
        sub, btns = [it.parentItem()], {}
        while sub:
            x = sub.pop(); sub.extend(x.childItems())
            if "CheckButton" in x.metaObject().className():
                btns[str(x.property("text"))] = x
        if {"On", "Off"} <= set(btns) and btns[want].isVisible():
            return btns[want]
    return None


res = {"tab": P["tab"]}
if P.get("step", "set") == "set":
    for qw in windows():
        res["panel"] = open_panel(qw)
        QtCore.QCoreApplication.processEvents()
        done = {}
        for label, value in (P.get("toggles") or {}).items():
            assert label in TOGGLES, label
            if sh.get_bool_value("AutoPhysics", label) == bool(value):
                done[label] = "already"; continue
            b = row_button(qw, label, "On" if value else "Off")
            if b is None or not visible_in_flickable(b):
                done[label] = "row not found"; continue
            done[label] = click(qw, b)
            QtCore.QCoreApplication.processEvents()
        res["toggles"] = done
        break
    if P.get("points"):
        ds = tab.domain_scene(); mv = ds.model_viewer(); dv = ds.data_viewer(); bv = ds.behaviour_viewer()
        n = dv.get_animation_size(); hits = []
        # resolve outside modify, and write each value with the type the data already holds ("bad variant access"
        # when an int goes into a float datum)
        for o in mv.get_objects():
            name = mv.get_object_name(o)
            for pat, props in P["points"].items():
                if pat not in name:
                    continue
                for key, value in props.items():
                    beh, prop = key.split(".", 1)
                    try:
                        d = bv.get_behaviour_data(bv.get_behaviour_by_name(o, beh), prop)
                        cur = dv.get_data_value(d, 0) if dv.get_data(d).mode != csc.model.DataMode.Static else dv.get_data_value(d)
                    except Exception:
                        continue
                    hits.append((d, type(cur)(value), dv.get_data(d).mode == csc.model.DataMode.Static, f"{name}.{key}"))

        def mod(model, update, su):
            de = model.data_editor()
            for d, value, static, _ in hits:
                if static:
                    de.set_data_value(d, value)
                else:
                    de.set_data_value(d, set(range(n)), value)
        res["points_ok"] = ds.modify("casc physics: point settings", mod)
        res["points"] = len(hits)
        res["readback"] = {h[3]: dv.get_data_value(h[0], n - 1) if not h[2] else dv.get_data_value(h[0]) for h in hits[:3]}
    if P.get("snap"):
        path = str(tab.get_path_name())
        if not (P.get("allow_key_changes") and ("_physics" in path or "_spacelab" in path)):
            raise RuntimeError("snap moves the key poses (owner locks): only on a _physics/_spacelab copy with allow_key_changes")
        app.get_action_manager().call_action("AutoPhysicsTool.Snap to Auto Physics")
        for qw in (shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow) for w in QtGui.QGuiApplication.topLevelWindows()):
            if qw.isVisible() and qw.title() == "Apply AutoPhysics":
                for it in items(qw):
                    if it.property("text") == P["snap"] and "DialogButton" in it.metaObject().className():
                        QtCore.QMetaObject.invokeMethod(it, "clicked"); res["snap"] = P["snap"]
    if P.get("save"):
        tab.save(str(tab.get_path_name())); res["saved"] = True
res["values"] = {k: sh.get_bool_value("AutoPhysics", k) for k in TOGGLES}
print(json.dumps(res))
