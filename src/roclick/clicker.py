from __future__ import annotations

import ctypes
import platform
from typing import Any, Protocol


class _MOUSEINPUT(ctypes.Structure):
    _fields_ = [
        ("dx", ctypes.c_long),
        ("dy", ctypes.c_long),
        ("mouseData", ctypes.c_ulong),
        ("dwFlags", ctypes.c_ulong),
        ("time", ctypes.c_ulong),
        ("dwExtraInfo", ctypes.c_void_p),
    ]


class _INPUT_UNION(ctypes.Union):
    _fields_ = [("mi", _MOUSEINPUT)]


class _INPUT(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_ulong),
        ("union", _INPUT_UNION),
    ]


class Clicker(Protocol):
    def click(self, button_name: str) -> None:
        pass


class PynputClicker:
    def __init__(self, mouse_module: Any) -> None:
        self._mouse_module = mouse_module
        self._controller = mouse_module.Controller()

    def click(self, button_name: str) -> None:
        button_by_name = {
            "Left": self._mouse_module.Button.left,
            "Right": self._mouse_module.Button.right,
            "Middle": self._mouse_module.Button.middle,
        }
        self._controller.click(button_by_name[button_name])


class WindowsSendInputClicker:
    _INPUT_MOUSE = 0
    _MOUSEEVENTF_LEFTDOWN = 0x0002
    _MOUSEEVENTF_LEFTUP = 0x0004
    _MOUSEEVENTF_RIGHTDOWN = 0x0008
    _MOUSEEVENTF_RIGHTUP = 0x0010
    _MOUSEEVENTF_MIDDLEDOWN = 0x0020
    _MOUSEEVENTF_MIDDLEUP = 0x0040

    def __init__(self) -> None:
        self._send_input = ctypes.windll.user32.SendInput
        self._send_input.argtypes = (
            ctypes.c_uint,
            ctypes.POINTER(_INPUT),
            ctypes.c_int,
        )
        self._send_input.restype = ctypes.c_uint

    def click(self, button_name: str) -> None:
        down, up = self._flags_for_button(button_name)
        inputs = (_INPUT * 2)(
            self._mouse_input(down),
            self._mouse_input(up),
        )
        sent = self._send_input(len(inputs), inputs, ctypes.sizeof(_INPUT))
        if sent != 2:
            raise ctypes.WinError()

    def _mouse_input(self, flags: int) -> _INPUT:
        return _INPUT(
            type=self._INPUT_MOUSE,
            union=_INPUT_UNION(
                mi=_MOUSEINPUT(
                    dx=0,
                    dy=0,
                    mouseData=0,
                    dwFlags=flags,
                    time=0,
                    dwExtraInfo=None,
                )
            ),
        )

    def _flags_for_button(self, button_name: str) -> tuple[int, int]:
        return {
            "Left": (self._MOUSEEVENTF_LEFTDOWN, self._MOUSEEVENTF_LEFTUP),
            "Right": (self._MOUSEEVENTF_RIGHTDOWN, self._MOUSEEVENTF_RIGHTUP),
            "Middle": (self._MOUSEEVENTF_MIDDLEDOWN, self._MOUSEEVENTF_MIDDLEUP),
        }[button_name]


def create_clicker(mouse_module: Any) -> Clicker:
    if platform.system() == "Windows":
        return WindowsSendInputClicker()
    return PynputClicker(mouse_module)
