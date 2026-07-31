from __future__ import annotations

import ctypes
import platform


def get_foreground_window_title() -> str:
    """Return the active foreground window title on Windows.

    RoClick is a Windows desktop utility. Returning an empty string on other
    platforms keeps imports and tests usable without pretending to support
    target-window locking outside Windows.
    """

    if platform.system() != "Windows":
        return ""

    user32 = ctypes.windll.user32
    handle = user32.GetForegroundWindow()
    if not handle:
        return ""

    length = user32.GetWindowTextLengthW(handle)
    if length <= 0:
        return ""

    buffer = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(handle, buffer, length + 1)
    return buffer.value.strip()
