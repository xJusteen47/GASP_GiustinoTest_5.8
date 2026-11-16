# Cascadeur (copy scene jab_snap_test.casc): trigger "Snap to Auto Physics" and, on deferred timers, dump every visible
# QML item that carries text (dialogs are QML items, Cascadeur has no QWidgets). Output goes to the Cascadeur log
# (%LOCALAPPDATA%\Nekki Limited\Cascadeur\logs\cascadeur_log.log) with the prefix SNAPPROBE. Params: CLICK (button text or "").
import csc, json
from PySide6 import QtCore, QtGui, QtQuick
import shiboken6
CLICK = globals().get("CLICK") or ""
app = csc.app.get_application(); sm = app.get_scene_manager()
tab = [s for s in sm.scenes() if s.name() == "jab_snap_test.casc"][0]
sm.set_current_scene(tab)
ds = tab.domain_scene(); lv = ds.layers_viewer(); mv = ds.model_viewer()
layer_ids = list(lv.all_layer_ids())
ds.get_layers_selector().set_full_selection_by_parts(layer_ids, 3, 17)
ids = {mv.get_object_name(o): o for o in mv.get_objects()}
ds.selector().select({o for n, o in ids.items() if mv.get_object_type_name(o) == "Point"})

def texts(item, depth=0, acc=None):
    acc = [] if acc is None else acc
    try:
        vis = item.isVisible()
    except Exception:
        vis = True
    if vis:
        t = item.property("text")
        if t:
            r = item.mapRectToScene(QtCore.QRectF(0, 0, item.width(), item.height()))
            acc.append({"cls": item.metaObject().className(), "name": item.objectName(), "text": str(t)[:80],
                        "rect": [round(r.x()), round(r.y()), round(r.width()), round(r.height())], "depth": depth})
    for ch in item.childItems():
        texts(ch, depth + 1, acc)
    return acc

def quick_windows():
    out = []
    for w in QtGui.QGuiApplication.topLevelWindows():
        if not w.isVisible():
            continue
        qw = w if isinstance(w, QtQuick.QQuickWindow) else shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow)
        if qw.inherits("QQuickWindow"):
            out.append(qw)
    return out

def scan(tag):
    found = []
    for w in quick_windows():
        try:
            root = w.contentItem()
        except Exception as e:
            found.append({"window": w.title()[:60], "err": str(e)[:100]}); continue
        items = texts(root)
        # keep likely dialog content: short texts mentioning snap/physics/frames/keys or button classes
        keep = [i for i in items if any(k in i["text"].lower() for k in ("snap", "frame", "key", "physic", "secondary", "ok", "cancel", "yes", "no", "apply", "all"))
                or "Button" in i["cls"]]
        found.append({"window": w.title()[:60], "n_items": len(items), "keep": keep[:60]})
    print("SNAPPROBE " + tag + " " + json.dumps(found)[:20000])
    return found

def click_text(target):
    for w in quick_windows():
        stack = [w.contentItem()]
        while stack:
            it = stack.pop(); stack.extend(it.childItems())
            if str(it.property("text") or "") == target and it.isVisible():
                ok = QtCore.QMetaObject.invokeMethod(it, "click")
                print("SNAPPROBE click " + json.dumps({"target": target, "cls": it.metaObject().className(), "invoked": bool(ok)}))
                return
    print("SNAPPROBE click-miss " + target)

app.get_action_manager().call_action("AutoPhysicsTool.Snap to Auto Physics")
scan("immediate")
QtCore.QTimer.singleShot(400, lambda: scan("t400"))
QtCore.QTimer.singleShot(1500, lambda: scan("t1500"))
if CLICK:
    QtCore.QTimer.singleShot(1700, lambda: click_text(CLICK))
    QtCore.QTimer.singleShot(3000, lambda: scan("after_click"))
print("snap triggered")
