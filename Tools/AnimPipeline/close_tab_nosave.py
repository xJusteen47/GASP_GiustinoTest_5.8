# Cascadeur: close tab NAME WITHOUT saving. close_scene on a modified tab opens a QML "save changes?" window; this script
# lists that window's buttons and, only if CONFIRM is set to the exact "don't save" button text, presses it.
# Params: NAME (tab name), CONFIRM (button text to press, or "" to only list). Output prefix CLOSETAB in the log.
import csc, json
from PySide6 import QtCore, QtGui, QtQuick
import shiboken6
NAME = globals().get("NAME"); CONFIRM = globals().get("CONFIRM") or ""
app = csc.app.get_application(); sm = app.get_scene_manager()

def windows():
    for w in QtGui.QGuiApplication.topLevelWindows():
        if w.isVisible():
            qw = shiboken6.wrapInstance(shiboken6.getCppPointer(w)[0], QtQuick.QQuickWindow)
            if qw.inherits("QQuickWindow"):
                yield qw

def dialogs():
    out = []
    for qw in windows():
        if "Cascadeur" in qw.title() and qw.title().endswith("Cascadeur"):
            continue
        items, stack = [], [qw.contentItem()]
        while stack:
            it = stack.pop(); stack.extend(it.childItems())
            t = str(it.property("text") or "")
            if t and it.isVisible():
                items.append((it.metaObject().className(), t[:120], it))
        out.append((qw, items))
    return out

tabs = [s for s in sm.scenes() if s.name() == NAME]
if tabs and not CONFIRM:
    other = [s for s in sm.scenes() if s.name() != NAME]
    if other:
        sm.set_current_scene(other[0])
    app.get_data_source_manager().close_scene(tabs[0])

def report():
    rep = [{"title": qw.title(), "items": [(c, t) for c, t, _ in items]} for qw, items in dialogs()]
    pressed = None
    if CONFIRM:
        for qw, items in dialogs():
            for c, t, it in items:
                if t == CONFIRM and "Button" in c:
                    QtCore.QMetaObject.invokeMethod(it, "clicked"); pressed = {"window": qw.title(), "button": t}
                    break
            if pressed:
                break
    print("CLOSETAB " + json.dumps({"dialogs": rep, "pressed": pressed, "tabs": [s.name() for s in sm.scenes()]}))
QtCore.QTimer.singleShot(600, report)
print(json.dumps({"requested_close": bool(tabs)}))
