"""Restrained Data Manager-style desktop frontend for replay video export."""

from __future__ import annotations

import json
import os
import queue
import subprocess
import sys
import threading
import time
import tkinter as tk
import uuid
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

try:
    from tkinterdnd2 import COPY, DND_FILES, TkinterDnD
except ImportError:  # Development runs remain usable without optional drag/drop.
    COPY = DND_FILES = None
    TkinterDnD = None

from . import __version__
from .model import ENCODERS, PRESETS, JobError, RenderJob
from .paths import find_ffmpeg, find_renderer
from .process import RendererProcess
from .protocol import ProgressEvent, ProgressTracker, parse_progress_line


APP_NAME = "TH03 Replay Renderer"
REPLAY_FILTER = (("TH03 V14 replay", "*.rpy"), ("All files", "*.*"))
VIDEO_FILTER = (("Matroska video", "*.mkv"),)
GAME_TEXT_FILTER = (("Compiled T3GTL001 game text", "*.t3gtl"), ("All files", "*.*"))
CUTSCENE_TEXT_FILTER = (("Compiled T3CTP001 cutscene text", "*.t3ctp"), ("All files", "*.*"))
PICTURE_PATCH_FILTER = (("Compiled T3PTP001 picture patch", "*.t3ptp"), ("All files", "*.*"))


def _preferences_path() -> Path:
    root = os.environ.get("LOCALAPPDATA")
    if root:
        return Path(root) / "PoDD Port" / "replay-renderer.json"
    return Path.home() / ".config" / "podd-port" / "replay-renderer.json"


def _format_duration(milliseconds: int) -> str:
    seconds = max(0, milliseconds // 1000)
    hours, seconds = divmod(seconds, 3600)
    minutes, seconds = divmod(seconds, 60)
    return f"{hours:d}:{minutes:02d}:{seconds:02d}"


def _volume_from_text(value: str) -> int | None:
    return None if value == "Installed" else int(value.removesuffix("%"))


class ReplayRendererApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title(f"{APP_NAME} {__version__}")
        self.root.geometry("900x720")
        self.root.minsize(780, 650)
        self.renderer = find_renderer()
        self.events: queue.Queue[tuple[str, object]] = queue.Queue()
        self.child: RendererProcess | None = None
        self.worker: threading.Thread | None = None
        self.tracker = ProgressTracker()
        self.started_at = 0.0
        self.cancel_requested = False

        preferences = self._load_preferences()
        self.replay = tk.StringVar()
        self.output = tk.StringVar()
        self.data = tk.StringVar()
        self.ffmpeg = tk.StringVar(value=find_ffmpeg())
        self.resolution = tk.StringVar(value=preferences.get("resolution", "1920x1200"))
        self.encoder = tk.StringVar(value=preferences.get(
            "encoder", "Automatic (NVIDIA, then software)"
        ))
        self.preset = tk.StringVar(value=preferences.get("preset", "p4"))
        self.installed_settings = tk.BooleanVar(value=preferences.get("installed", True))
        self.language = tk.StringVar(value=preferences.get("language", "Installed"))
        self.flash = tk.StringVar(value=preferences.get("flash", "Installed"))
        self.master = tk.StringVar(value=preferences.get("master", "Installed"))
        self.music = tk.StringVar(value=preferences.get("music", "Installed"))
        self.sfx = tk.StringVar(value=preferences.get("sfx", "Installed"))
        self.game_text_pack = tk.StringVar()
        self.cutscene_text_pack = tk.StringVar()
        self.picture_patch_pack = tk.StringVar()
        self.status = tk.StringVar(value="Ready")
        self.phase = tk.StringVar(value="Choose a replay and output file.")
        self.timing = tk.StringVar(value="")
        self.command_preview = tk.StringVar(value="")

        self._build_style()
        self._build_ui()
        self._bind_updates()
        self._enable_drop()
        self.root.protocol("WM_DELETE_WINDOW", self._close)
        self.root.after(50, self._poll_events)
        self._update_command_preview()

    def _build_style(self) -> None:
        style = ttk.Style(self.root)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        style.configure("Title.TLabel", font=("Segoe UI Semibold", 20))
        style.configure("Section.TLabel", font=("Segoe UI Semibold", 10))
        style.configure("Accent.TButton", font=("Segoe UI Semibold", 10))

    def _build_ui(self) -> None:
        outer = ttk.Frame(self.root, padding=(20, 16, 20, 14))
        outer.pack(fill="both", expand=True)
        ttk.Label(outer, text="Replay Renderer", style="Title.TLabel").pack(anchor="w")
        ttk.Label(
            outer,
            text="Render a complete New Classic replay with exact native video and PMD audio.",
        ).pack(anchor="w", pady=(1, 16))

        source = ttk.Frame(outer)
        source.pack(fill="x")
        ttk.Label(source, text="FILES", style="Section.TLabel").grid(
            row=0, column=0, columnspan=3, sticky="w", pady=(0, 6)
        )
        source.columnconfigure(1, weight=1)
        self._path_row(source, 1, "Replay", self.replay, self._choose_replay)
        self._path_row(source, 2, "Output", self.output, self._choose_output)
        self._path_row(source, 3, "Game data", self.data, self._choose_data)
        ttk.Separator(outer).pack(fill="x", pady=14)

        options = ttk.Frame(outer)
        options.pack(fill="x")
        ttk.Label(options, text="RENDER", style="Section.TLabel").grid(
            row=0, column=0, columnspan=6, sticky="w", pady=(0, 7)
        )
        for column in (1, 3, 5):
            options.columnconfigure(column, weight=1)
        self._combo(options, 1, 0, "Resolution", self.resolution,
                    ("640x400", "1280x800", "1920x1200", "2560x1600"), True)
        self._combo(options, 1, 2, "Encoder", self.encoder, tuple(ENCODERS), False)
        self._combo(options, 1, 4, "Preset", self.preset, PRESETS, False)

        ttk.Checkbutton(
            options, text="Start from installed game settings",
            variable=self.installed_settings,
        ).grid(row=2, column=0, columnspan=2, sticky="w", pady=(12, 0))
        self._combo(options, 3, 0, "Language", self.language,
                    ("Installed", "Japanese", "English"), False)
        self._combo(options, 3, 2, "Flash FX", self.flash,
                    ("Installed", "Reduced", "Full"), False)
        volume_values = ("Installed", *(f"{value}%" for value in range(0, 101, 5)))
        self._combo(options, 4, 0, "Master", self.master, volume_values, False)
        self._combo(options, 4, 2, "Music", self.music, volume_values, False)
        self._combo(options, 4, 4, "SFX", self.sfx, volume_values, False)

        advanced = ttk.Frame(options)
        advanced.grid(row=5, column=0, columnspan=6, sticky="ew", pady=(12, 0))
        advanced.columnconfigure(1, weight=1)
        ttk.Label(advanced, text="FFmpeg").grid(row=0, column=0, sticky="w")
        ttk.Entry(advanced, textvariable=self.ffmpeg).grid(
            row=0, column=1, sticky="ew", padx=(10, 6)
        )
        ttk.Button(advanced, text="Browse...", command=self._choose_ffmpeg).grid(
            row=0, column=2
        )
        ttk.Label(advanced, text="Game text pack").grid(
            row=1, column=0, sticky="w", pady=(7, 0)
        )
        ttk.Entry(advanced, textvariable=self.game_text_pack).grid(
            row=1, column=1, sticky="ew", padx=(10, 6), pady=(7, 0)
        )
        ttk.Button(advanced, text="Browse...", command=self._choose_game_text).grid(
            row=1, column=2, pady=(7, 0)
        )
        ttk.Label(advanced, text="Cutscene text pack").grid(
            row=2, column=0, sticky="w", pady=(7, 0)
        )
        ttk.Entry(advanced, textvariable=self.cutscene_text_pack).grid(
            row=2, column=1, sticky="ew", padx=(10, 6), pady=(7, 0)
        )
        ttk.Button(advanced, text="Browse...", command=self._choose_cutscene_text).grid(
            row=2, column=2, pady=(7, 0)
        )
        ttk.Label(advanced, text="Picture patch pack").grid(
            row=3, column=0, sticky="w", pady=(7, 0)
        )
        ttk.Entry(advanced, textvariable=self.picture_patch_pack).grid(
            row=3, column=1, sticky="ew", padx=(10, 6), pady=(7, 0)
        )
        ttk.Button(advanced, text="Browse...", command=self._choose_picture_patch).grid(
            row=3, column=2, pady=(7, 0)
        )

        ttk.Separator(outer).pack(fill="x", pady=14)
        progress = ttk.Frame(outer)
        progress.pack(fill="both", expand=True)
        top = ttk.Frame(progress)
        top.pack(fill="x")
        ttk.Label(top, text="PROGRESS", style="Section.TLabel").pack(side="left")
        ttk.Label(top, textvariable=self.timing).pack(side="right")
        ttk.Label(progress, textvariable=self.phase).pack(anchor="w", pady=(6, 5))
        self.progress = ttk.Progressbar(progress, maximum=10_000, mode="determinate")
        self.progress.pack(fill="x")

        command = ttk.Frame(progress)
        command.pack(fill="x", pady=(10, 0))
        command.columnconfigure(0, weight=1)
        ttk.Entry(command, textvariable=self.command_preview, state="readonly").grid(
            row=0, column=0, sticky="ew"
        )
        ttk.Button(command, text="Copy Command", command=self._copy_command).grid(
            row=0, column=1, padx=(7, 0)
        )

        self.log_button = ttk.Button(progress, text="Show Log", command=self._toggle_log)
        self.log_button.pack(anchor="w", pady=(8, 0))
        self.log_frame = ttk.Frame(progress)
        self.log = tk.Text(
            self.log_frame, height=8, wrap="word", state="disabled",
            font=("Cascadia Mono", 9), relief="flat", borderwidth=0,
        )
        scroll = ttk.Scrollbar(self.log_frame, orient="vertical", command=self.log.yview)
        self.log.configure(yscrollcommand=scroll.set)
        self.log.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")

        actions = ttk.Frame(outer)
        actions.pack(fill="x", pady=(14, 0))
        ttk.Label(actions, textvariable=self.status).pack(side="left")
        self.open_button = ttk.Button(
            actions, text="Open Folder", command=self._open_output_folder, state="disabled"
        )
        self.open_button.pack(side="right")
        self.cancel_button = ttk.Button(
            actions, text="Cancel", command=self._cancel, state="disabled"
        )
        self.cancel_button.pack(side="right", padx=(0, 8))
        self.render_button = ttk.Button(
            actions, text="Render Replay", style="Accent.TButton", command=self._start
        )
        self.render_button.pack(side="right", padx=(0, 8))

    def _path_row(self, parent: ttk.Frame, row: int, label: str,
                  variable: tk.StringVar, command: object) -> None:
        ttk.Label(parent, text=label, width=9).grid(row=row, column=0, sticky="w", pady=3)
        ttk.Entry(parent, textvariable=variable).grid(
            row=row, column=1, sticky="ew", padx=(7, 6), pady=3
        )
        ttk.Button(parent, text="Browse...", command=command).grid(
            row=row, column=2, pady=3
        )

    def _combo(self, parent: ttk.Frame, row: int, column: int, label: str,
               variable: tk.StringVar, values: tuple[str, ...], editable: bool) -> None:
        ttk.Label(parent, text=label).grid(row=row, column=column, sticky="w", pady=(4, 0))
        ttk.Combobox(
            parent, textvariable=variable, values=values,
            state="normal" if editable else "readonly", width=22,
        ).grid(row=row, column=column + 1, sticky="ew", padx=(7, 18), pady=(4, 0))

    def _bind_updates(self) -> None:
        for variable in (
            self.replay, self.output, self.data, self.ffmpeg, self.resolution,
            self.encoder, self.preset, self.language, self.flash, self.master,
            self.music, self.sfx, self.game_text_pack, self.cutscene_text_pack,
            self.picture_patch_pack,
        ):
            variable.trace_add("write", lambda *_args: self._update_command_preview())
        self.installed_settings.trace_add(
            "write", lambda *_args: self._update_command_preview()
        )

    def _enable_drop(self) -> None:
        if TkinterDnD is None or DND_FILES is None:
            return
        self.root.drop_target_register(DND_FILES)
        self.root.dnd_bind("<<Drop>>", self._drop)

    def _drop(self, event: object) -> str:
        paths = self.root.tk.splitlist(event.data)
        if paths:
            path = Path(paths[0])
            if path.suffix.lower() == ".rpy":
                self._set_replay(path)
        return COPY

    def _choose_replay(self) -> None:
        selected = filedialog.askopenfilename(title="Choose Replay", filetypes=REPLAY_FILTER)
        if selected:
            self._set_replay(Path(selected))

    def _set_replay(self, path: Path) -> None:
        self.replay.set(str(path))
        if not self.output.get():
            self.output.set(str(path.with_suffix(".mkv")))

    def _choose_output(self) -> None:
        selected = filedialog.asksaveasfilename(
            title="Choose Output Video", defaultextension=".mkv", filetypes=VIDEO_FILTER
        )
        if selected:
            self.output.set(selected)

    def _choose_data(self) -> None:
        selected = filedialog.askdirectory(title="Choose Extracted Game Data Directory")
        if selected:
            self.data.set(selected)

    def _choose_ffmpeg(self) -> None:
        selected = filedialog.askopenfilename(title="Choose FFmpeg")
        if selected:
            self.ffmpeg.set(selected)

    def _choose_game_text(self) -> None:
        selected = filedialog.askopenfilename(
            title="Choose Game Text Pack", filetypes=GAME_TEXT_FILTER
        )
        if selected:
            self.game_text_pack.set(selected)

    def _choose_cutscene_text(self) -> None:
        selected = filedialog.askopenfilename(
            title="Choose Cutscene Text Pack", filetypes=CUTSCENE_TEXT_FILTER
        )
        if selected:
            self.cutscene_text_pack.set(selected)

    def _choose_picture_patch(self) -> None:
        selected = filedialog.askopenfilename(
            title="Choose Picture Patch Pack", filetypes=PICTURE_PATCH_FILTER
        )
        if selected:
            self.picture_patch_pack.set(selected)

    def _job(
        self, overwrite: bool = False, staging_output: Path | None = None,
    ) -> RenderJob:
        if self.renderer is None:
            raise JobError("The bundled replay renderer could not be found.")
        language = {"Installed": None, "Japanese": "ja", "English": "en"}[
            self.language.get()
        ]
        flash = {"Installed": None, "Reduced": True, "Full": False}[
            self.flash.get()
        ]
        pack = Path(self.game_text_pack.get()) if self.game_text_pack.get() else None
        cutscene_pack = (
            Path(self.cutscene_text_pack.get()) if self.cutscene_text_pack.get() else None
        )
        picture_pack = (
            Path(self.picture_patch_pack.get()) if self.picture_patch_pack.get() else None
        )
        return RenderJob(
            renderer=self.renderer,
            replay=Path(self.replay.get()),
            data=Path(self.data.get()),
            output=Path(self.output.get()),
            ffmpeg=self.ffmpeg.get(),
            resolution=self.resolution.get(),
            encoder_label=self.encoder.get(),
            encoder_preset=self.preset.get(),
            use_installed_settings=self.installed_settings.get(),
            language=language,
            reduced_flash=flash,
            master_volume=_volume_from_text(self.master.get()),
            music_volume=_volume_from_text(self.music.get()),
            sfx_volume=_volume_from_text(self.sfx.get()),
            game_text_pack=pack,
            cutscene_text_pack=cutscene_pack,
            picture_patch_pack=picture_pack,
            staging_output=staging_output,
            overwrite=overwrite,
        )

    def _update_command_preview(self) -> None:
        try:
            self.command_preview.set(self._job().command_preview())
        except (JobError, KeyError, ValueError):
            self.command_preview.set("Complete the required fields to preview the command.")

    def _copy_command(self) -> None:
        value = self.command_preview.get()
        if value and not value.startswith("Complete"):
            self.root.clipboard_clear()
            self.root.clipboard_append(value)
            self.status.set("Equivalent command copied")

    def _start(self) -> None:
        try:
            initial = self._job()
            initial.validate()
        except (JobError, KeyError, ValueError) as error:
            messagebox.showerror("Cannot Render Replay", str(error), parent=self.root)
            return
        overwrite = initial.output.exists()
        if overwrite and not messagebox.askyesno(
            "Replace Video?",
            f"{initial.output.name} already exists. Replace it?",
            parent=self.root,
        ):
            return
        staging = initial.output.with_name(
            f".{initial.output.name}.th03-gui-{uuid.uuid4().hex}.mkv"
        )
        job = self._job(overwrite=overwrite, staging_output=staging)
        self._save_preferences()
        self.tracker = ProgressTracker()
        self.progress.configure(value=0)
        self.phase.set("Preparing audio pass...")
        self.timing.set("")
        self.status.set("Rendering")
        self.cancel_requested = False
        self.started_at = time.monotonic()
        self.render_button.configure(state="disabled")
        self.cancel_button.configure(state="normal")
        self.open_button.configure(state="disabled")
        self._clear_log()
        self._append_log(job.command_preview())
        self.worker = threading.Thread(target=self._run_job, args=(job,), daemon=True)
        self.worker.start()

    def _run_job(self, job: RenderJob) -> None:
        try:
            self.child = RendererProcess(job.command())
            for line in self.child.lines():
                self.events.put(("line", line))
            code = self.child.wait()
            self.events.put(("exit", (code, job)))
        except Exception as error:  # Surface process creation/ownership failures.
            self.events.put(("error", (str(error), job)))
        finally:
            self.child = None

    def _poll_events(self) -> None:
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == "line":
                    self._consume_line(str(value))
                elif kind == "exit":
                    code, job = value
                    self._finish(int(code), job)
                elif kind == "error":
                    error, job = value
                    self._append_log(str(error))
                    self._finish(1, job)
        except queue.Empty:
            pass
        self.root.after(50, self._poll_events)

    def _consume_line(self, line: str) -> None:
        try:
            event = parse_progress_line(line)
            if event is not None:
                self._show_progress(event)
                return
        except ValueError as error:
            self._append_log(f"Progress protocol error: {error}")
            return
        self._append_log(line)

    def _show_progress(self, event: ProgressEvent) -> None:
        overall = self.tracker.accept(event)
        self.progress.configure(value=overall)
        label = "Audio pass 1 of 2" if event.render_pass == "audio" else "Video pass 2 of 2"
        self.phase.set(f"{label} · {event.completed_permyriad / 100:.1f}%")
        elapsed = _format_duration(int((time.monotonic() - self.started_at) * 1000))
        eta = _format_duration(event.eta_milliseconds)
        speed = event.speed_milli / 1000
        self.timing.set(f"Elapsed {elapsed}  ·  ETA {eta}  ·  {speed:.1f}×")

    def _finish(self, code: int, job: RenderJob) -> None:
        self.render_button.configure(state="normal")
        self.cancel_button.configure(state="disabled")
        cancelled = self.cancel_requested
        self.cancel_requested = False
        if cancelled:
            self.status.set("Cancelled")
            self.phase.set("Render cancelled; the destination was left unchanged.")
            self._remove_partial(job)
        elif code == 0 and job.output.is_file():
            self.progress.configure(value=10_000)
            self.status.set("Complete")
            self.phase.set(f"Finished {job.output.name}")
            self.open_button.configure(state="normal")
        else:
            self.status.set("Failed")
            self.phase.set("The renderer stopped with an error. Open the log for details.")
            self._remove_partial(job)
            self._show_log()

    def _cancel(self) -> None:
        if self.child is None:
            return
        if not messagebox.askyesno("Cancel Render?", "Stop this render and remove partial output?", parent=self.root):
            return
        self.cancel_requested = True
        self.status.set("Cancelling...")
        self.child.terminate()

    @staticmethod
    def _remove_partial(job: RenderJob) -> None:
        paths = [Path(str(job.output) + ".th03-audio.flac")]
        if job.staging_output is not None:
            paths.append(job.staging_output)
        for path in paths:
            try:
                path.unlink(missing_ok=True)
            except OSError:
                pass

    def _open_output_folder(self) -> None:
        output = Path(self.output.get())
        if os.name == "nt":
            subprocess.Popen(["explorer", "/select,", str(output)])
        else:
            subprocess.Popen(["open" if sys.platform == "darwin" else "xdg-open", str(output.parent)])

    def _toggle_log(self) -> None:
        if self.log_frame.winfo_ismapped():
            self.log_frame.pack_forget()
            self.log_button.configure(text="Show Log")
        else:
            self._show_log()

    def _show_log(self) -> None:
        if not self.log_frame.winfo_ismapped():
            self.log_frame.pack(fill="both", expand=True, pady=(6, 0))
            self.log_button.configure(text="Hide Log")

    def _append_log(self, line: str) -> None:
        self.log.configure(state="normal")
        self.log.insert("end", line + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _clear_log(self) -> None:
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")

    def _load_preferences(self) -> dict[str, object]:
        try:
            value = json.loads(_preferences_path().read_text(encoding="utf-8"))
            return value if isinstance(value, dict) else {}
        except (OSError, json.JSONDecodeError):
            return {}

    def _save_preferences(self) -> None:
        path = _preferences_path()
        value = {
            "resolution": self.resolution.get(), "encoder": self.encoder.get(),
            "preset": self.preset.get(), "installed": self.installed_settings.get(),
            "language": self.language.get(), "flash": self.flash.get(),
            "master": self.master.get(), "music": self.music.get(), "sfx": self.sfx.get(),
        }
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            temporary = path.with_suffix(".tmp")
            temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
            os.replace(temporary, path)
        except OSError:
            pass

    def _close(self) -> None:
        if self.child is not None:
            if not messagebox.askyesno("Exit?", "Cancel the active render and exit?", parent=self.root):
                return
            self.cancel_requested = True
            self.child.terminate()
        self.root.destroy()


def run_gui() -> None:
    root = TkinterDnD.Tk() if TkinterDnD is not None else tk.Tk()
    ReplayRendererApp(root)
    root.mainloop()
