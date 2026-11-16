# Opens the GSTN button bar a few seconds after Cascadeur starts (the main window is not up yet when this event fires).
from PySide6 import QtCore


def _open():
    try:
        from gstn_scripts import _bar
        _bar.show()
    except Exception as ex:   # never break Cascadeur's start-up for a convenience bar
        print(f"GSTN bar: {ex}")


def run():
    QtCore.QTimer.singleShot(5000, _open)
