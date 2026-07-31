from __future__ import annotations

import queue
import sys
import threading
import time
import tkinter as tk
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import customtkinter as ctk
from PIL import Image

from .window_utils import get_foreground_window_title


BACKGROUND = "#EEF7FF"
GLASS = "#F8FBFF"
BLUE = "#0A84FF"
RED = "#FF453A"
GREEN = "#30D158"
TEXT = "#111111"
SECONDARY = "#6E6E73"
BORDER = "#FFFFFF"
WINDOW_WIDTH = 480
WINDOW_HEIGHT = 760


def asset_path(name: str) -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys._MEIPASS) / "assets" / name
    return Path(__file__).resolve().parents[2] / "assets" / name


@dataclass(frozen=True)
class ClickSettings:
    interval_ms: int
    button: Any
    lock_to_target: bool
    target_title: str


class RoClickApp(ctk.CTk):
    def __init__(self) -> None:
        super().__init__()

        self.title("RoClick")
        self.geometry(f"{WINDOW_WIDTH}x{WINDOW_HEIGHT}")
        self.minsize(440, 700)
        self.configure(fg_color=BACKGROUND)
        self._set_window_icon()
        self._enable_soft_transparency()

        self._keyboard_module, self._mouse_module = self._load_input_modules()
        self._command_queue: queue.Queue[str] = queue.Queue()
        self._mouse_controller = self._mouse_module.Controller()
        self._state_lock = threading.Lock()
        self._active_event = threading.Event()
        self._shutdown_event = threading.Event()
        self._target_title = ""
        self._interval_ms = tk.IntVar(value=100)
        self._button_name = tk.StringVar(value="Left")
        self._lock_enabled = tk.BooleanVar(value=False)
        self._status_text = tk.StringVar(value="Stopped")
        self._helper_text = tk.StringVar(value="Ready. Press F6 to start.")
        self._target_text = tk.StringVar(value="No target selected")
        self._interval_text = tk.StringVar(value="100 ms")
        self._background_image: ctk.CTkImage | None = None
        self._logo_image: ctk.CTkImage | None = None
        self._blocky_asset_image: ctk.CTkImage | None = None

        self._build_ui()

        self._worker_thread = threading.Thread(
            target=self._click_loop,
            name="RoClickClickWorker",
            daemon=True,
        )
        self._worker_thread.start()

        self._keyboard_listener = self._keyboard_module.Listener(on_press=self._on_key_press)
        self._keyboard_listener.start()

        self.after(50, self._drain_command_queue)
        self.protocol("WM_DELETE_WINDOW", self._close)

    def _build_ui(self) -> None:
        ctk.set_appearance_mode("light")
        ctk.set_default_color_theme("blue")

        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(2, weight=1)

        self._build_background()

        header = ctk.CTkFrame(
            self,
            fg_color=GLASS,
            corner_radius=24,
            border_width=1,
            border_color=BORDER,
        )
        header.grid(row=0, column=0, sticky="ew", padx=22, pady=(20, 14))
        header.grid_columnconfigure(1, weight=1)

        self._logo_image = self._load_ctk_image("roclick-logo.png", (58, 58))
        logo_label = ctk.CTkLabel(
            header,
            text="",
            image=self._logo_image,
        )
        logo_label.grid(row=0, column=0, sticky="w", padx=(18, 0), pady=16)

        title_stack = ctk.CTkFrame(header, fg_color="transparent")
        title_stack.grid(row=0, column=1, sticky="ew", padx=(14, 18), pady=16)
        ctk.CTkLabel(
            title_stack,
            text="RoClick",
            text_color=TEXT,
            font=ctk.CTkFont(size=28, weight="bold"),
            anchor="w",
        ).grid(row=0, column=0, sticky="w")
        ctk.CTkLabel(
            title_stack,
            text="Local desktop click control",
            text_color=SECONDARY,
            font=ctk.CTkFont(size=14),
            anchor="w",
        ).grid(row=1, column=0, sticky="w", pady=(2, 0))

        status_card = self._card(row=1, pady=(0, 14))
        status_card.grid_columnconfigure(0, weight=1)

        status_line = ctk.CTkFrame(status_card, fg_color="transparent")
        status_line.grid(row=0, column=0, sticky="ew", padx=24, pady=(22, 0))
        status_line.grid_columnconfigure(1, weight=1)
        self._status_dot = ctk.CTkFrame(
            status_line,
            width=14,
            height=14,
            fg_color=SECONDARY,
            corner_radius=7,
        )
        self._status_dot.grid(row=0, column=0, padx=(0, 10))
        ctk.CTkLabel(
            status_line,
            textvariable=self._status_text,
            text_color=TEXT,
            font=ctk.CTkFont(size=24, weight="bold"),
            anchor="w",
        ).grid(row=0, column=1, sticky="w")

        ctk.CTkLabel(
            status_card,
            textvariable=self._helper_text,
            text_color=SECONDARY,
            font=ctk.CTkFont(size=14),
            anchor="w",
        ).grid(row=1, column=0, sticky="ew", padx=24, pady=(8, 20))

        self._primary_button = ctk.CTkButton(
            status_card,
            text="Start",
            height=52,
            corner_radius=18,
            fg_color=BLUE,
            hover_color="#006EDB",
            text_color="white",
            font=ctk.CTkFont(size=17, weight="bold"),
            command=self.toggle_active,
        )
        self._primary_button.grid(row=2, column=0, sticky="ew", padx=24, pady=(0, 24))

        settings_card = self._card(row=2, pady=(0, 22))
        settings_card.grid_columnconfigure(0, weight=1)

        ctk.CTkLabel(
            settings_card,
            text="Settings",
            text_color=TEXT,
            font=ctk.CTkFont(size=20, weight="bold"),
            anchor="w",
        ).grid(row=0, column=0, sticky="ew", padx=24, pady=(22, 16))

        self._blocky_asset_image = self._load_ctk_image("blocky-click-asset.png", (380, 112))
        ctk.CTkLabel(
            settings_card,
            text="",
            image=self._blocky_asset_image,
            corner_radius=18,
        ).grid(row=1, column=0, sticky="ew", padx=22, pady=(0, 18))

        interval_row = ctk.CTkFrame(settings_card, fg_color="transparent")
        interval_row.grid(row=2, column=0, sticky="ew", padx=24)
        interval_row.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(
            interval_row,
            text="Interval",
            text_color=TEXT,
            font=ctk.CTkFont(size=15, weight="bold"),
            anchor="w",
        ).grid(row=0, column=0, sticky="w")
        ctk.CTkLabel(
            interval_row,
            textvariable=self._interval_text,
            text_color=SECONDARY,
            font=ctk.CTkFont(size=15),
            anchor="e",
        ).grid(row=0, column=1, sticky="e")

        slider = ctk.CTkSlider(
            settings_card,
            from_=50,
            to=1000,
            number_of_steps=190,
            progress_color=BLUE,
            button_color=BLUE,
            button_hover_color="#006EDB",
            command=self._set_interval,
        )
        slider.set(self._interval_ms.get())
        slider.grid(row=3, column=0, sticky="ew", padx=24, pady=(10, 20))

        ctk.CTkLabel(
            settings_card,
            text="Mouse Button",
            text_color=TEXT,
            font=ctk.CTkFont(size=15, weight="bold"),
            anchor="w",
        ).grid(row=4, column=0, sticky="ew", padx=24)
        self._button_selector = ctk.CTkSegmentedButton(
            settings_card,
            values=["Left", "Right", "Middle"],
            variable=self._button_name,
            selected_color="#FFFFFF",
            selected_hover_color="#F8FBFF",
            unselected_color="#E9EEF5",
            unselected_hover_color="#E5E5EA",
            text_color=TEXT,
            text_color_disabled=SECONDARY,
            font=ctk.CTkFont(size=14, weight="bold"),
        )
        self._button_selector.grid(row=5, column=0, sticky="ew", padx=24, pady=(10, 20))

        target_frame = ctk.CTkFrame(settings_card, fg_color="#F9F9FB", corner_radius=18)
        target_frame.grid(row=6, column=0, sticky="ew", padx=24, pady=(0, 18))
        target_frame.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(
            target_frame,
            text="Target",
            text_color=TEXT,
            font=ctk.CTkFont(size=15, weight="bold"),
            anchor="w",
        ).grid(row=0, column=0, sticky="ew", padx=16, pady=(14, 0))
        ctk.CTkLabel(
            target_frame,
            textvariable=self._target_text,
            text_color=SECONDARY,
            font=ctk.CTkFont(size=13),
            anchor="w",
            wraplength=260,
        ).grid(row=1, column=0, sticky="ew", padx=16, pady=(4, 14))
        ctk.CTkButton(
            target_frame,
            text="Capture Active",
            width=124,
            height=34,
            corner_radius=13,
            fg_color=BLUE,
            hover_color="#006EDB",
            font=ctk.CTkFont(size=13, weight="bold"),
            command=self._start_target_capture,
        ).grid(row=0, column=1, rowspan=2, sticky="e", padx=16, pady=14)

        lock_switch = ctk.CTkSwitch(
            settings_card,
            text="Click only while target is active",
            variable=self._lock_enabled,
            button_color=BLUE,
            progress_color=BLUE,
            text_color=TEXT,
            font=ctk.CTkFont(size=14, weight="bold"),
        )
        lock_switch.grid(row=7, column=0, sticky="w", padx=24, pady=(0, 14))

        ctk.CTkLabel(
            settings_card,
            text="Use only where you have permission. RoClick never reads app memory or hides automation.",
            text_color=SECONDARY,
            font=ctk.CTkFont(size=12),
            anchor="w",
            wraplength=370,
        ).grid(row=8, column=0, sticky="ew", padx=24, pady=(0, 22))

    def _card(self, row: int, pady: tuple[int, int]) -> ctk.CTkFrame:
        card = ctk.CTkFrame(
            self,
            fg_color=GLASS,
            corner_radius=22,
            border_width=1,
            border_color=BORDER,
        )
        card.grid(row=row, column=0, sticky="nsew", padx=24, pady=pady)
        return card

    def _build_background(self) -> None:
        self._background_image = self._load_ctk_image(
            "blocky-background.png",
            (WINDOW_WIDTH, WINDOW_HEIGHT),
        )
        background = ctk.CTkLabel(self, text="", image=self._background_image)
        background.place(x=0, y=0, relwidth=1, relheight=1)
        background.lower()

    def _load_ctk_image(self, name: str, size: tuple[int, int]) -> ctk.CTkImage:
        image = Image.open(asset_path(name))
        return ctk.CTkImage(light_image=image, dark_image=image, size=size)

    def _set_window_icon(self) -> None:
        icon = asset_path("roclick.ico")
        if icon.exists():
            try:
                self.iconbitmap(str(icon))
            except tk.TclError:
                pass

    def _enable_soft_transparency(self) -> None:
        try:
            self.attributes("-alpha", 0.985)
        except tk.TclError:
            pass

    def _set_interval(self, value: float) -> None:
        interval = int(round(value / 5) * 5)
        interval = max(50, min(1000, interval))
        self._interval_ms.set(interval)
        self._interval_text.set(f"{interval} ms")

    def _start_target_capture(self) -> None:
        self._target_text.set("Focus target window now...")
        self.after(1600, self._capture_target_title)

    def _capture_target_title(self) -> None:
        title = get_foreground_window_title()
        with self._state_lock:
            self._target_title = title
        self._target_text.set(title or "No foreground title detected")

    @staticmethod
    def _load_input_modules() -> tuple[Any, Any]:
        from pynput import keyboard, mouse

        return keyboard, mouse

    def _on_key_press(self, key: Any) -> None:
        if key == self._keyboard_module.Key.f6:
            self._command_queue.put("toggle")
        elif key == self._keyboard_module.Key.esc:
            self._command_queue.put("stop")

    def _drain_command_queue(self) -> None:
        while True:
            try:
                command = self._command_queue.get_nowait()
            except queue.Empty:
                break

            if command == "toggle":
                self.toggle_active()
            elif command == "stop":
                self.stop_clicking()

        if not self._shutdown_event.is_set():
            self.after(50, self._drain_command_queue)

    def toggle_active(self) -> None:
        if self._active_event.is_set():
            self.stop_clicking()
        else:
            self.start_clicking()

    def start_clicking(self) -> None:
        self._active_event.set()
        self._status_text.set("Active")
        self._helper_text.set("Press F6 or Escape to stop.")
        self._status_dot.configure(fg_color=GREEN)
        self._primary_button.configure(text="Stop", fg_color=RED, hover_color="#D9362E")

    def stop_clicking(self) -> None:
        self._active_event.clear()
        self._status_text.set("Stopped")
        self._helper_text.set("Ready. Press F6 to start.")
        self._status_dot.configure(fg_color=SECONDARY)
        self._primary_button.configure(text="Start", fg_color=BLUE, hover_color="#006EDB")

    def _current_settings(self) -> ClickSettings:
        button_by_name = {
            "Left": self._mouse_module.Button.left,
            "Right": self._mouse_module.Button.right,
            "Middle": self._mouse_module.Button.middle,
        }
        with self._state_lock:
            target_title = self._target_title
        return ClickSettings(
            interval_ms=self._interval_ms.get(),
            button=button_by_name[self._button_name.get()],
            lock_to_target=self._lock_enabled.get(),
            target_title=target_title,
        )

    def _target_allows_click(self, settings: ClickSettings) -> bool:
        if not settings.lock_to_target:
            return True
        if not settings.target_title:
            return False

        active_title = get_foreground_window_title()
        return active_title == settings.target_title

    def _click_loop(self) -> None:
        while not self._shutdown_event.is_set():
            if not self._active_event.wait(0.05):
                continue

            settings = self._current_settings()
            if self._target_allows_click(settings):
                self._mouse_controller.click(settings.button)

            delay_seconds = settings.interval_ms / 1000
            end_time = time.monotonic() + delay_seconds
            while (
                self._active_event.is_set()
                and not self._shutdown_event.is_set()
                and time.monotonic() < end_time
            ):
                remaining = end_time - time.monotonic()
                if remaining <= 0:
                    break
                time.sleep(min(0.02, remaining))

    def _close(self) -> None:
        self._shutdown_event.set()
        self._active_event.clear()
        try:
            self._keyboard_listener.stop()
        except Exception:
            pass
        self._worker_thread.join(timeout=1.5)
        self.destroy()


def main() -> None:
    app = RoClickApp()
    app.mainloop()
