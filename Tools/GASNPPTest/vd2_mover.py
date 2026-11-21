# Runs inside UnrealEditor (Python): moves every top-level window of THIS process to a target virtual desktop, every ~0.5 s.
# IVirtualDesktopManager::MoveWindowToDesktop only works on the caller's own windows, hence it runs in the editor.
import ctypes, ctypes.wintypes as wt, os, uuid, unreal

TARGET = os.environ.get('VD_TARGET_GUID', '94a28a1c-cfe5-4e5c-807e-df5a7493ba76')
LOG = os.environ.get('VD_LOG')

class GUID(ctypes.Structure):
    _fields_ = [('d1', ctypes.c_ulong), ('d2', ctypes.c_ushort), ('d3', ctypes.c_ushort), ('d4', ctypes.c_ubyte * 8)]
    @classmethod
    def of(cls, s):
        b = uuid.UUID(s).bytes_le
        g = cls(); ctypes.memmove(ctypes.byref(g), b, 16); return g
    def __str__(self):
        return str(uuid.UUID(bytes_le=bytes(ctypes.string_at(ctypes.byref(self), 16))))

ole32 = ctypes.OleDLL('ole32'); user32 = ctypes.WinDLL('user32')
ole32.CoInitializeEx.restype = ctypes.c_long
try:
    ctypes.windll.ole32.CoInitializeEx(None, 2)
except Exception:
    pass
_mgr = ctypes.c_void_p()
ctypes.windll.ole32.CoCreateInstance(ctypes.byref(GUID.of('aa509086-5ca9-4c25-8f95-589d3c07b48a')), None, 1 | 4,
    ctypes.byref(GUID.of('a5cd92ff-29be-454c-8d04-d82879fb3f1b')), ctypes.byref(_mgr))
_vt = ctypes.cast(ctypes.cast(_mgr, ctypes.POINTER(ctypes.c_void_p))[0], ctypes.POINTER(ctypes.c_void_p))
_GetId = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, wt.HWND, ctypes.POINTER(GUID))(_vt[4])
_Move = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, wt.HWND, ctypes.POINTER(GUID))(_vt[5])
_target = GUID.of(TARGET)
_pid = os.getpid()
_moved = set()
# Owner 2026-11-20 (speed-up.md 4): captures are low-res scene captures, so the editor's own windows are shrunk to EDITOR_WIN
# ("WxH", default 960x540) and render less. EDITOR_WIN=full keeps them as they open: use it for owner-facing hi-res videos
# (PIE_CAPTURE_HIRES viewport shots, screen recordings), which take the real window's size.
_WIN = os.environ.get('EDITOR_WIN', '960x540').lower()
_WIN = None if _WIN in ('', 'full', '0') else tuple(int(v) for v in _WIN.split('x'))
_shrunk = set()
_acc = [0.0]
# Owner 2026-11-21 (runstart-v23): editor and Play windows on his SECOND MONITOR, so nothing flashes over his work.
# WINDOW_PLACE=monitor2 (default): every window of this process goes to the first non-primary monitor of the current desktop
# (top-left of its work area, size kept or EDITOR_WIN); no virtual desktop move. WINDOW_PLACE=vd2: the old rule (virtual desktop
# VD_TARGET_GUID). With one monitor only, monitor2 falls back to vd2.
_PLACE = os.environ.get('WINDOW_PLACE', 'monitor2').lower()


def _second_monitor():
    mons = []
    MONITORENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HMONITOR, wt.HDC, ctypes.POINTER(wt.RECT), wt.LPARAM)

    class MI(ctypes.Structure):
        _fields_ = [('cbSize', wt.DWORD), ('rcMonitor', wt.RECT), ('rcWork', wt.RECT), ('dwFlags', wt.DWORD)]

    def cb(hm, hdc, r, l):
        mi = MI(); mi.cbSize = ctypes.sizeof(MI)
        user32.GetMonitorInfoW(hm, ctypes.byref(mi))
        if not (mi.dwFlags & 1):
            w = mi.rcWork; mons.append((w.left, w.top, w.right - w.left, w.bottom - w.top))
        return True
    user32.EnumDisplayMonitors(None, None, MONITORENUMPROC(cb), 0)
    return mons[0] if mons else None


_MON = _second_monitor() if _PLACE == 'monitor2' else None
_placed = set()

def _log(msg):
    if LOG:
        with open(LOG, 'a') as f:
            f.write(msg + '\n')

def sweep():
    hwnds = []
    cb = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)(lambda h, l: hwnds.append(h) or True)
    user32.EnumWindows(cb, 0)
    for h in hwnds:
        p = wt.DWORD(); user32.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value != _pid or not user32.IsWindowVisible(h):
            continue
        if _MON:
            if h not in _placed:
                r = wt.RECT(); user32.GetWindowRect(h, ctypes.byref(r))
                w, hh = (_WIN if _WIN else (r.right - r.left, r.bottom - r.top))
                w, hh = min(w, _MON[2]), min(hh, _MON[3])
                user32.ShowWindow(h, 9)   # SW_RESTORE: a maximized window ignores SetWindowPos
                user32.SetWindowPos(h, None, _MON[0], _MON[1], w, hh, 0x0014)   # SWP_NOZORDER | SWP_NOACTIVATE
                _placed.add(h); _log('monitor2 hwnd %s -> %s %dx%d' % (h, _MON[:2], w, hh))
            continue
        if _WIN and h not in _shrunk:
            r = wt.RECT(); user32.GetWindowRect(h, ctypes.byref(r))
            if r.right - r.left > _WIN[0] or r.bottom - r.top > _WIN[1]:
                user32.ShowWindow(h, 9)   # SW_RESTORE: a maximized window ignores SetWindowPos
                user32.SetWindowPos(h, None, 0, 0, _WIN[0], _WIN[1], 0x0014)   # SWP_NOZORDER | SWP_NOACTIVATE
                _log('shrink hwnd %s %dx%d -> %dx%d' % (h, r.right - r.left, r.bottom - r.top, _WIN[0], _WIN[1]))
            _shrunk.add(h)
        cur = GUID()
        if _GetId(_mgr, h, ctypes.byref(cur)) != 0 or str(cur) == TARGET:
            continue
        hr = _Move(_mgr, h, ctypes.byref(_target))
        if h not in _moved:
            _moved.add(h); _log('move hwnd %s from %s hr=%d' % (h, cur, hr & 0xffffffff))

def _tick(dt):
    _acc[0] += dt
    if _acc[0] >= 0.5:
        _acc[0] = 0.0
        try:
            sweep()
        except Exception as e:
            _log('sweep error %r' % e)

_log('vd2_mover start pid=%d mgr=%s place=%s mon=%s' % (_pid, _mgr.value, _PLACE, _MON))
sweep()
HANDLE = unreal.register_slate_post_tick_callback(_tick)
