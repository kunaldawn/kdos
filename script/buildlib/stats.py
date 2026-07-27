# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

"""Build telemetry: a sampler thread, timing history and the ETA model."""

import json
import os
import threading
import time

from . import phases as phases_mod
from .snapshot import dir_usage

RAMP = " ⡀⣀⣄⣤⣦⣶⣷⣿"     # braille density ramp
EWMA_ALPHA = 0.4
FAST_INTERVAL = 1.0
WALK_INTERVAL = 20.0
WALK_BUDGET = 15.0        # seconds; the fs walk is a HUD counter, not a job


class Sampler(threading.Thread):
    """Collects cheap stats every second and walks build/fs occasionally."""

    def __init__(self, manager, build_dir, history=120):
        super().__init__(daemon=True)
        self.manager = manager
        self.build_dir = build_dir
        self.stop_event = threading.Event()

        self.load1 = 0.0
        self.mem_used = 0
        self.mem_total = 0
        self.disk_free = 0
        self.lines_per_sec = 0.0
        self.history = []
        self.history_len = history

        self.fs_files = 0
        self.fs_bytes = 0
        self.fs_partial = False
        self.fs_sampled_at = 0.0
        self._walking = False

    def stop(self):
        self.stop_event.set()

    def run(self):
        last_lines = self.manager.total_lines
        last_walk = 0.0
        while not self.stop_event.wait(FAST_INTERVAL):
            now = time.time()

            lines = self.manager.total_lines
            self.lines_per_sec = max(0, lines - last_lines) / FAST_INTERVAL
            last_lines = lines
            self.history.append(self.lines_per_sec)
            del self.history[:-self.history_len]

            try:
                self.load1 = os.getloadavg()[0]
            except OSError:
                pass
            self._read_meminfo()
            try:
                st = os.statvfs(self.build_dir)
                self.disk_free = st.f_bavail * st.f_frsize
            except OSError:
                pass

            if now - last_walk >= WALK_INTERVAL and not self._walking:
                last_walk = now
                self._walking = True
                threading.Thread(target=self._walk_fs, daemon=True).start()

    def _read_meminfo(self):
        try:
            total = available = 0
            with open("/proc/meminfo", 'r') as f:
                for line in f:
                    if line.startswith("MemTotal:"):
                        total = int(line.split()[1]) * 1024
                    elif line.startswith("MemAvailable:"):
                        available = int(line.split()[1]) * 1024
                    if total and available:
                        break
            self.mem_total = total
            self.mem_used = max(0, total - available)
        except (OSError, ValueError, IndexError):
            pass

    def _walk_fs(self):
        # Hard time budget: this is a cosmetic counter and must never pin a
        # core, however pathological the tree gets.
        try:
            fs_path = os.path.join(self.build_dir, "fs")
            usage = dir_usage(fs_path, deadline=time.monotonic() + WALK_BUDGET)
            self.fs_bytes, self.fs_files = usage.bytes, usage.files
            self.fs_partial = not usage.complete
            self.fs_sampled_at = time.time()
        except Exception:
            pass
        finally:
            self._walking = False

    def sparkline(self, width):
        if width <= 0:
            return ""
        data = self.history[-width:]
        if not data:
            return " " * width
        peak = max(data) or 1.0
        cells = "".join(RAMP[min(len(RAMP) - 1, int(v / peak * (len(RAMP) - 1)))]
                        for v in data)
        return cells.rjust(width)


class TimingStore:
    """Per-step and per-phase durations, EWMA-smoothed across builds."""

    def __init__(self, path):
        self.path = path
        self.steps = {}
        self.phases = {}
        self.dirty = False
        self.load()

    def load(self):
        try:
            with open(self.path, 'r') as f:
                data = json.load(f)
            self.steps = dict(data.get("steps", {}))
            self.phases = dict(data.get("phases", {}))
        except Exception:
            self.steps, self.phases = {}, {}

    def save(self):
        if not self.dirty:
            return
        try:
            os.makedirs(os.path.dirname(self.path), exist_ok=True)
            tmp = self.path + ".tmp"
            with open(tmp, 'w') as f:
                json.dump({"steps": self.steps, "phases": self.phases}, f, indent=1)
            os.replace(tmp, self.path)
            self.dirty = False
        except OSError:
            pass

    @staticmethod
    def _blend(old, new):
        if old is None:
            return new
        return old * (1 - EWMA_ALPHA) + new * EWMA_ALPHA

    def record_step(self, key, duration):
        if duration <= 0:
            return
        self.steps[key] = round(self._blend(self.steps.get(key), duration), 2)
        self.dirty = True

    def record_phase(self, key, duration):
        if duration <= 0:
            return
        self.phases[key] = round(self._blend(self.phases.get(key), duration), 2)
        self.dirty = True

    def step_estimate(self, key, fallback):
        return self.steps.get(key, fallback)

    def phase_estimate(self, dir_name):
        return self.phases.get(dir_name)

    # ----- ETA ------------------------------------------------------------

    def eta(self, manager):
        """Seconds remaining, or None when there is nothing to base it on."""
        done_durations = [s.duration() for s in manager.execution_order
                          if not s.is_group and s.status == phases_mod.STATUS_DONE
                          and s.duration() > 0]
        observed_mean = (sum(done_durations) / len(done_durations)) if done_durations else None
        known_mean = (sum(self.steps.values()) / len(self.steps)) if self.steps else None
        fallback = observed_mean or known_mean

        total = 0.0
        have_estimate = False

        for group in manager.roots:
            if group.status in (phases_mod.STATUS_SKIPPED, phases_mod.STATUS_DONE):
                continue

            if group.children:
                for child in group.children:
                    if child.status in (phases_mod.STATUS_DONE, phases_mod.STATUS_SKIPPED):
                        continue
                    est = self.steps.get(child.timing_key(), fallback)
                    if est is None:
                        continue
                    have_estimate = True
                    if child.status == phases_mod.STATUS_RUNNING:
                        total += max(0.0, est - child.duration())
                    else:
                        total += est
            else:
                est = self.phases.get(group.meta.dir_name)
                if est is None:
                    continue
                have_estimate = True
                total += est

        if not have_estimate:
            return None
        return total
