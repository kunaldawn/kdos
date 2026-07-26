# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

"""Curses front end: snapshot picker, build screen, HUD."""

import curses
import locale
import os
import time

from .phases import (STATUS_DONE, STATUS_FAIL, STATUS_PENDING, STATUS_RUNNING,
                     STATUS_SKIPPED, human_bytes, human_time)

CP_BG = 1
CP_HL = 2
CP_RUN = 3
CP_DONE = 4
CP_FAIL = 5
CP_TITLE = 6
CP_BAR = 7
CP_LOG = 8
CP_TIME = 9
CP_KEY = 10
CP_DONE_REV = 11
CP_TRACK = 12
CP_RUN_SEL = 13
CP_DIM = 14
CP_WARN = 15
CP_SNAP = 16
CP_SNAP_REV = 17

UNICODE_OK = "utf" in (locale.getpreferredencoding(False) or "").lower().replace("-", "")

if UNICODE_OK:
    SPINNER = ["⣾", "⣽", "⣻", "⢿", "⡿", "⣟", "⣯", "⣷"]
    BAR_FILL, BAR_TRACK = "█", "░"
    GLYPH_SNAP, GLYPH_OK, GLYPH_SEP = "◆", "✓", "│"
else:
    SPINNER = ["|", "/", "-", "\\", "|", "/", "-", "\\"]
    BAR_FILL, BAR_TRACK = "#", "-"
    GLYPH_SNAP, GLYPH_OK, GLYPH_SEP = "*", "v", "|"

LOGO = [
    "██╗  ██╗██████╗  ██████╗ ███████╗",
    "██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝",
    "█████╔╝ ██║  ██║██║   ██║███████╗",
    "██╔═██╗ ██║  ██║██║   ██║╚════██║",
    "██║  ██╗██████╔╝╚██████╔╝███████║",
    "╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝",
] if UNICODE_OK else [
    " _  _____   ___  ___ ",
    "| |/ /   \\ / _ \\/ __|",
    "| ' <| |) | (_) \\__ \\",
    "|_|\\_\\___/ \\___/|___/",
]


def init_colors():
    try:
        curses.start_color()
        curses.init_pair(CP_BG, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(CP_HL, curses.COLOR_WHITE, curses.COLOR_BLUE)
        curses.init_pair(CP_RUN, curses.COLOR_YELLOW, curses.COLOR_BLACK)
        curses.init_pair(CP_DONE, curses.COLOR_GREEN, curses.COLOR_BLACK)
        curses.init_pair(CP_FAIL, curses.COLOR_RED, curses.COLOR_BLACK)
        curses.init_pair(CP_TITLE, curses.COLOR_CYAN, curses.COLOR_BLACK)
        curses.init_pair(CP_BAR, curses.COLOR_BLUE, curses.COLOR_BLACK)
        curses.init_pair(CP_LOG, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(CP_TIME, curses.COLOR_CYAN, curses.COLOR_BLACK)
        curses.init_pair(CP_KEY, curses.COLOR_YELLOW, curses.COLOR_BLACK)
        curses.init_pair(CP_DONE_REV, curses.COLOR_BLACK, curses.COLOR_GREEN)
        curses.init_pair(CP_TRACK, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(CP_RUN_SEL, curses.COLOR_YELLOW, curses.COLOR_BLUE)
        curses.init_pair(CP_DIM, curses.COLOR_BLUE, curses.COLOR_BLACK)
        curses.init_pair(CP_WARN, curses.COLOR_MAGENTA, curses.COLOR_BLACK)
        curses.init_pair(CP_SNAP, curses.COLOR_MAGENTA, curses.COLOR_BLACK)
        curses.init_pair(CP_SNAP_REV, curses.COLOR_BLACK, curses.COLOR_MAGENTA)
    except curses.error:
        pass


class Screen:
    """Drawing helpers shared by the picker and the build screen."""

    def __init__(self, stdscr):
        self.stdscr = stdscr
        self.h, self.w = stdscr.getmaxyx()

    def put(self, y, x, text, attr=0):
        if y < 0 or x < 0 or y >= self.h or x >= self.w:
            return
        text = text[:max(0, self.w - x)]
        if not text:
            return
        try:
            self.stdscr.addstr(y, x, text, attr)
        except (curses.error, UnicodeEncodeError):
            try:
                ascii_text = text.encode("ascii", "replace").decode("ascii")
                self.stdscr.addstr(y, x, ascii_text, attr)
            except curses.error:
                pass

    def bar(self, y, x, width, pct, fill_attr=None, track_attr=None):
        if width <= 0:
            return
        pct = min(1.0, max(0.0, pct))
        fill = int(round(width * pct))
        fill_attr = curses.color_pair(CP_DONE) | curses.A_BOLD if fill_attr is None else fill_attr
        track_attr = curses.color_pair(CP_DIM) if track_attr is None else track_attr
        self.put(y, x, BAR_FILL * fill, fill_attr)
        self.put(y, x + fill, BAR_TRACK * (width - fill), track_attr)

    def hline(self, y, x, width, attr=None):
        attr = curses.color_pair(CP_TITLE) | curses.A_BOLD if attr is None else attr
        for i in range(width):
            try:
                self.stdscr.addch(y, x + i, curses.ACS_HLINE, attr)
            except curses.error:
                pass


def format_when(ts):
    try:
        return time.strftime("%Y-%m-%d %H:%M", time.localtime(ts))
    except (ValueError, OSError):
        return "?"


def manifest_size(manifest):
    return sum(e.get("bytes_compressed", 0) for e in manifest.get("entries", []))


def manifest_stale(manifest, commit):
    if manifest.get("git_dirty"):
        return True
    if commit and manifest.get("git_commit") and manifest["git_commit"] != commit:
        return True
    return False


class StartupMenu(Screen):
    """Pick a snapshot to restore, or start a fresh build."""

    def __init__(self, stdscr, store, phases, commit=None):
        super().__init__(stdscr)
        self.store = store
        self.phases = phases
        self.commit = commit
        self.selected = 0
        self.status = ""
        self.rows = []
        self.reload()

    def reload(self):
        snaps = self.store.list()
        self.rows = [{"kind": "fresh"}]
        for phase in self.phases:
            manifest = snaps.get(phase.dir_name)
            if manifest:
                self.rows.append({"kind": "snap", "phase": phase, "manifest": manifest})
        self.selected = min(self.selected, len(self.rows) - 1)

    def run(self):
        self.stdscr.nodelay(False)
        curses.curs_set(0)
        while True:
            self.h, self.w = self.stdscr.getmaxyx()
            self.draw()
            key = self.stdscr.getch()

            if key in (ord('q'), ord('Q')):
                return ("quit", None)
            if key in (curses.KEY_UP, ord('k')):
                self.selected = max(0, self.selected - 1)
            elif key in (curses.KEY_DOWN, ord('j')):
                self.selected = min(len(self.rows) - 1, self.selected + 1)
            elif key in (curses.KEY_ENTER, 10, 13):
                return self._activate(self.selected)
            elif key in (ord('r'), ord('R')):
                return self._activate(self.selected)
            elif key in (ord('f'), ord('F')):
                return ("fresh", None)
            elif key in (ord('d'), ord('D')):
                self._delete(self.selected)
            elif ord('1') <= key <= ord('9'):
                idx = key - ord('0')
                if idx < len(self.rows):
                    return self._activate(idx)
            elif key == curses.KEY_RESIZE:
                continue

    def _activate(self, idx):
        if idx <= 0 or idx >= len(self.rows):
            return ("fresh", None)
        return ("restore", self.rows[idx]["phase"].index)

    def _delete(self, idx):
        if idx <= 0 or idx >= len(self.rows):
            return
        row = self.rows[idx]
        self.store.delete(row["phase"].dir_name)
        self.status = "deleted %s" % row["phase"].dir_name
        self.reload()

    def draw(self):
        self.stdscr.bkgd(' ', curses.color_pair(CP_BG))
        self.stdscr.erase()
        y = 1

        if self.h > len(LOGO) + 12:
            for i, line in enumerate(LOGO):
                self.put(y + i, max(2, (self.w - len(LOGO[0])) // 2),
                         line, curses.color_pair(CP_DONE) | curses.A_BOLD)
            y += len(LOGO) + 1

        title = "BUILD SNAPSHOTS"
        self.put(y, max(2, (self.w - len(title)) // 2), title,
                 curses.color_pair(CP_RUN) | curses.A_BOLD)
        y += 2

        header = "   %-16s %-17s %8s %10s %7s %9s" % (
            "PHASE", "WHEN", "SIZE", "COMMIT", "STEPS", "BUILD")
        self.put(y, 2, header[:self.w - 4], curses.color_pair(CP_TITLE) | curses.A_BOLD)
        y += 1
        self.hline(y, 2, max(0, min(self.w - 4, len(header))))
        y += 1

        for idx, row in enumerate(self.rows):
            attr = curses.color_pair(CP_HL) | curses.A_BOLD if idx == self.selected \
                else curses.color_pair(CP_BG)
            if row["kind"] == "fresh":
                text = "   %-16s %s" % ("start fresh", "run every phase from scratch")
            else:
                manifest = row["manifest"]
                stale = manifest_stale(manifest, self.commit)
                commit = (manifest.get("git_commit") or "-") + ("*" if stale else "")
                if manifest.get("complete", True):
                    steps = str(manifest.get("steps", "-"))
                else:
                    steps = "%s/%s!" % (manifest.get("steps", "?"),
                                        manifest.get("total_steps", "?"))
                text = "%2d %-16s %-17s %8s %10s %7s %9s" % (
                    idx,
                    row["phase"].dir_name,
                    format_when(manifest.get("created", 0)),
                    human_bytes(manifest_size(manifest)),
                    commit,
                    steps,
                    human_time(manifest.get("duration_s")),
                )
            self.put(y, 2, text.ljust(min(self.w - 4, max(len(header), len(text)))), attr)
            y += 1

        if len(self.rows) == 1:
            self.put(y + 1, 4, "no snapshots yet — the first build will create them",
                     curses.color_pair(CP_DIM))
            y += 2

        y += 1
        if any(r["kind"] == "snap" and manifest_stale(r["manifest"], self.commit)
               for r in self.rows):
            self.put(y, 2, "* snapshot taken from a different or dirty commit",
                     curses.color_pair(CP_WARN))
            y += 1
        if any(r["kind"] == "snap" and not r["manifest"].get("complete", True)
               for r in self.rows):
            self.put(y, 2, "! partial snapshot — restoring it re-runs that phase",
                     curses.color_pair(CP_WARN))
            y += 1

        stale_restore = self.store.interrupted_restore()
        if stale_restore:
            self.put(y, 2, "INTERRUPTED RESTORE of %s — build/ is inconsistent; "
                           "restore again or 'make cleanbuild'"
                     % stale_restore.get("target", "?"),
                     curses.color_pair(CP_FAIL) | curses.A_BOLD)
            y += 1

        selected_row = self.rows[self.selected] if self.rows else None
        if selected_row and selected_row["kind"] == "snap":
            entries = selected_row["manifest"].get("entries", [])
            detail = "  ".join("%s %s" % (e["path"], human_bytes(e.get("bytes_compressed")))
                               for e in entries)
            self.put(y, 2, "restores: " + detail, curses.color_pair(CP_TIME))
            y += 1
            self.put(y, 2, "continues from: %s" %
                     (self.phases[selected_row["phase"].index + 1].dir_name
                      if selected_row["phase"].index + 1 < len(self.phases) else "nothing left"),
                     curses.color_pair(CP_TIME))
            y += 1

        if self.status:
            self.put(y + 1, 2, self.status, curses.color_pair(CP_WARN))

        keys = [("↑↓", "select"), ("ENTER", "start"), ("D", "delete"), ("Q", "quit")]
        x = 2
        fy = self.h - 2
        for key, label in keys:
            self.put(fy, x, "[%s]" % key, curses.color_pair(CP_KEY) | curses.A_BOLD)
            x += len(key) + 2
            self.put(fy, x, ":%s " % label, curses.color_pair(CP_BG))
            x += len(label) + 2

        self.put(self.h - 1, 2,
                 "codec: %s   snapshots: %s" % (self.store.codec, self.store.root),
                 curses.color_pair(CP_DIM))
        self.stdscr.refresh()


class ProgressScreen(Screen):
    """Full-screen progress for the restore that runs before the build."""

    def __init__(self, stdscr, title):
        super().__init__(stdscr)
        self.title = title
        self.state = None
        self.lines = []
        self._cancelled = False
        self.stdscr.nodelay(True)

    def log(self, text):
        self.lines.append(text)
        del self.lines[:-8]
        self.draw()

    def cancelled(self):
        """Poll for a cancel key. Called from the thread driving the restore."""
        if not self._cancelled:
            key = self.stdscr.getch()
            if key in (ord('q'), ord('Q'), 27):
                self._cancelled = True
        return self._cancelled

    def on_progress(self, state):
        self.state = state
        self.draw()

    def draw(self):
        self.h, self.w = self.stdscr.getmaxyx()
        self.stdscr.erase()
        self.stdscr.bkgd(' ', curses.color_pair(CP_BG))

        y = max(1, self.h // 2 - 5)
        self.put(y, 2, self.title, curses.color_pair(CP_RUN) | curses.A_BOLD)
        y += 2

        state = self.state
        if state:
            self.put(y, 2, "%s  %s  <- %s" % (state["action"], state["path"], state["phase"]),
                     curses.color_pair(CP_BG) | curses.A_BOLD)
            y += 1
            est = state.get("est_bytes") or 0
            pct = min(1.0, state["bytes"] / est) if est else 0.0
            width = max(10, min(self.w - 24, 60))
            self.bar(y, 2, width, pct)
            self.put(y, 2 + width + 1,
                     "%3d%%  %s  %s/s" % (int(pct * 100), human_bytes(state["bytes"]),
                                          human_bytes(state["rate"])),
                     curses.color_pair(CP_BG))
            y += 1
            self.put(y, 2, "%d files  %s" % (state["files"], state.get("current", "")[-60:]),
                     curses.color_pair(CP_DIM))
            y += 2

        for line in self.lines:
            self.put(y, 2, line, curses.color_pair(CP_LOG))
            y += 1
        self.stdscr.refresh()


class TUI(Screen):
    def __init__(self, stdscr, manager, sampler=None, timings=None, store=None):
        super().__init__(stdscr)
        self.manager = manager
        self.sampler = sampler
        self.timings = timings
        self.store = store
        self.snapshots = store.list() if store else {}
        self.selected_node = None
        self.visible_nodes = []
        self.scroll_offset = 0
        self.auto_follow = True
        self.log_scroll = 0
        self.quit_requested = False
        init_colors()
        self.stdscr.nodelay(True)
        self.stdscr.keypad(True)
        try:
            curses.curs_set(0)
        except curses.error:
            pass

    # ----- geometry -------------------------------------------------------

    def _layout(self):
        hud_h = 2 if self.h >= 20 else (1 if self.h >= 16 else 0)
        header_h = 3 if self.h >= 12 else 0
        top = 1 + header_h
        footer_y = self.h - 1
        hud_bottom = footer_y - 1
        hud_top = hud_bottom - hud_h + 1
        content_bottom = (hud_top - 2) if hud_h else (footer_y - 1)
        return {
            "header_h": header_h,
            "top": top,
            "content_bottom": content_bottom,
            "hud_top": hud_top,
            "hud_h": hud_h,
            "footer_y": footer_y,
        }

    def _flatten(self, nodes):
        out = []
        for node in nodes:
            out.append(node)
            if node.is_group and node.expanded:
                out.extend(self._flatten(node.children))
        return out

    def update(self):
        self.h, self.w = self.stdscr.getmaxyx()
        self.visible_nodes = self._flatten(self.manager.roots)

        if self.auto_follow and self.manager.current_step:
            self.selected_node = self.manager.current_step
        if not self.selected_node and self.visible_nodes:
            self.selected_node = self.visible_nodes[0]
        if not self.visible_nodes:
            return

        try:
            sel_idx = self.visible_nodes.index(self.selected_node)
        except ValueError:
            sel_idx = 0
            self.selected_node = self.visible_nodes[0]

        layout = self._layout()
        list_h = max(1, layout["content_bottom"] - layout["top"] + 1)
        if sel_idx < self.scroll_offset:
            self.scroll_offset = sel_idx
        elif sel_idx >= self.scroll_offset + list_h:
            self.scroll_offset = sel_idx - list_h + 1

    # ----- painting -------------------------------------------------------

    def draw_screen(self):
        self.stdscr.bkgd(' ', curses.color_pair(CP_BG))
        self.stdscr.erase()
        layout = self._layout()

        try:
            self.stdscr.attron(curses.color_pair(CP_TITLE) | curses.A_BOLD)
            self.stdscr.border()
            self.stdscr.attroff(curses.color_pair(CP_TITLE) | curses.A_BOLD)
        except curses.error:
            pass

        title = " KDOS BUILD SYSTEM "
        if len(title) < self.w:
            self.put(0, (self.w - len(title)) // 2, title,
                     curses.color_pair(CP_RUN) | curses.A_BOLD)

        if layout["header_h"]:
            self._draw_header()

        tree_w = self._tree_width()
        detail_x = tree_w + 1
        detail_w = self.w - tree_w - 2
        if detail_w < 5:
            self.stdscr.refresh()
            return

        try:
            self.stdscr.attron(curses.color_pair(CP_TITLE) | curses.A_BOLD)
            for y in range(layout["top"], layout["content_bottom"] + 1):
                self.stdscr.addch(y, tree_w, curses.ACS_VLINE)
            self.stdscr.attroff(curses.color_pair(CP_TITLE) | curses.A_BOLD)
        except curses.error:
            pass

        self._draw_tree(tree_w, layout)
        self._draw_detail(detail_x, detail_w, layout)
        if layout["hud_h"]:
            self._draw_hud(layout)
        self._draw_footer(layout)
        if self.manager.snapshot_activity:
            self._draw_snapshot_overlay(self.manager.snapshot_activity)
        self.stdscr.refresh()

    def _tree_width(self):
        max_text = 20
        for node in self.visible_nodes:
            width = (node.level * 2) + 2 + 1 + len(node.title) + 1 + 9
            max_text = max(max_text, width)
        tree_w = max_text + 4
        return max(25, min(tree_w, int(self.w * 0.45)))

    def _phase_progress(self):
        """(phase_group, done, total) for the phase currently executing."""
        group = self.manager.current_phase
        if group is None:
            for candidate in self.manager.roots:
                if candidate.status in (STATUS_RUNNING, STATUS_PENDING):
                    group = candidate
                    break
        if group is None:
            return None, 0, 0
        total = len(group.children)
        done = sum(1 for c in group.children if c.status in (STATUS_DONE, STATUS_FAIL))
        return group, done, total

    def _draw_header(self):
        group, done, total = self._phase_progress()
        phase_count = len(self.manager.roots)
        index = (group.meta.index + 1) if (group and group.meta) else 0

        left = " PHASE %d/%d  %s" % (index, phase_count, group.meta.label if group else "-")
        if group and group.meta:
            if group.meta.title and group.meta.title != group.meta.label:
                left += "  %s %s" % (GLYPH_SEP, group.meta.title)
            if group.meta.desc and len(left) + len(group.meta.desc) + 24 < self.w:
                left += "  %s %s" % (GLYPH_SEP, group.meta.desc)
        self.put(1, 1, left.ljust(self.w - 2)[:self.w - 2],
                 curses.color_pair(CP_TITLE) | curses.A_BOLD)

        right = ""
        if self.manager.restored_from:
            right = "restored <- %s " % self.manager.restored_from.dir_name
        elif self.manager.continued_from:
            right = "continued past %s " % self.manager.continued_from.dir_name
        if right and len(right) + len(left) + 4 < self.w:
            self.put(1, self.w - 1 - len(right), right,
                     curses.color_pair(CP_SNAP) | curses.A_BOLD)

        pct = (done / total) if total else 0.0
        eta = self.timings.eta(self.manager) if self.timings else None
        suffix = "  %3d%%  %d/%d  eta %s" % (int(pct * 100), done, total,
                                             human_time(eta) if eta else "--")
        bar_w = max(10, self.w - 4 - len(suffix))
        self.bar(2, 2, bar_w, pct,
                 curses.color_pair(CP_DONE) | curses.A_BOLD,
                 curses.color_pair(CP_DIM))
        self.put(2, 2 + bar_w, suffix, curses.color_pair(CP_BG) | curses.A_BOLD)
        self.hline(3, 1, self.w - 2)

    def _node_icon(self, node):
        if node.is_group:
            icon = " [-]" if node.expanded else " [+]"
            if node.status == STATUS_SKIPPED:
                return icon, curses.color_pair(CP_DIM) | curses.A_BOLD, \
                    curses.color_pair(CP_DIM) | curses.A_BOLD
            return icon, curses.color_pair(CP_TITLE) | curses.A_BOLD, \
                curses.color_pair(CP_TITLE) | curses.A_BOLD

        if node.status == STATUS_PENDING:
            return "    ", curses.color_pair(CP_BG), curses.color_pair(CP_BG)
        if node.status == STATUS_SKIPPED:
            return " ·· " if UNICODE_OK else " .. ", \
                curses.color_pair(CP_DIM), curses.color_pair(CP_DIM)
        if node.status == STATUS_RUNNING:
            frame = SPINNER[int(time.time() * 10) % len(SPINNER)]
            return " %s  " % frame, curses.color_pair(CP_RUN) | curses.A_BOLD, \
                curses.color_pair(CP_BG) | curses.A_BOLD
        if node.status == STATUS_DONE:
            return " OK ", curses.color_pair(CP_DONE_REV) | curses.A_BOLD, \
                curses.color_pair(CP_BG) | curses.A_BOLD
        return " !! ", curses.color_pair(CP_FAIL) | curses.A_BOLD, \
            curses.color_pair(CP_FAIL) | curses.A_BOLD

    def _draw_tree(self, width, layout):
        top = layout["top"]
        rows = layout["content_bottom"] - top + 1

        for i in range(rows):
            idx = self.scroll_offset + i
            if idx >= len(self.visible_nodes):
                break
            node = self.visible_nodes[idx]
            y = top + i

            indent = "  " * node.level
            icon_str, icon_attr, text_attr = self._node_icon(node)

            dur = node.duration()
            t_str = ""
            if node.status == STATUS_SKIPPED:
                t_str = "(skip)"
            elif dur > 0 or node.status == STATUS_RUNNING:
                t_str = "(%ds)" % int(dur) if dur < 60 else "(%dm%02ds)" % (int(dur) // 60, int(dur) % 60)

            avail_w = width - 1
            len_pre = len(indent) + len(icon_str) + 1
            space_for_title = avail_w - len_pre - 1 - len(t_str)
            disp_title = node.title
            if space_for_title < len(disp_title):
                disp_title = "…" if space_for_title < 3 else disp_title[:space_for_title - 1] + "…"
            pad_len = max(0, avail_w - (len_pre + len(disp_title) + len(t_str)))

            selected = node is self.selected_node
            if selected:
                base = curses.color_pair(CP_HL)
                icon_attr = curses.color_pair(CP_RUN_SEL) | curses.A_BOLD \
                    if (node.status == STATUS_RUNNING and not node.is_group) \
                    else base | curses.A_BOLD
                text_attr = base | curses.A_BOLD
                pad_attr = base
                time_attr = base | curses.A_BOLD
            else:
                base = curses.color_pair(CP_BG)
                pad_attr = base
                time_attr = curses.color_pair(CP_DIM) if node.status == STATUS_SKIPPED \
                    else curses.color_pair(CP_TIME) | curses.A_BOLD

            x = 1
            self.put(y, x, indent, pad_attr); x += len(indent)
            self.put(y, x, icon_str, icon_attr); x += len(icon_str)
            self.put(y, x, " ", pad_attr); x += 1
            self.put(y, x, disp_title, text_attr); x += len(disp_title)
            self.put(y, x, " " * pad_len, pad_attr); x += pad_len
            self.put(y, x, t_str, time_attr)

    def _draw_detail(self, x, w, layout):
        if not self.selected_node:
            return
        node = self.selected_node
        top = layout["top"]

        header_attr = curses.color_pair(CP_BG) | curses.A_BOLD
        if node.status == STATUS_RUNNING:
            header_attr = curses.color_pair(CP_RUN) | curses.A_BOLD
        elif node.status == STATUS_DONE:
            header_attr = curses.color_pair(CP_DONE) | curses.A_BOLD
        elif node.status == STATUS_FAIL:
            header_attr = curses.color_pair(CP_FAIL) | curses.A_BOLD
        elif node.status == STATUS_SKIPPED:
            header_attr = curses.color_pair(CP_DIM) | curses.A_BOLD

        self.put(top, x, (" STEP: %s " % node.title).ljust(w), header_attr)

        status_s = " STATUS: %s " % node.status
        if node.status == STATUS_RUNNING and node.start_time:
            status_s += "(%ds)" % int(time.time() - node.start_time)
        elif node.duration() > 0:
            status_s += "(%.1fs)" % node.duration()
        if node.note:
            status_s += "  %s %s" % (GLYPH_SNAP, node.note)
        self.put(top + 1, x, status_s.ljust(w), curses.color_pair(CP_BG) | curses.A_BOLD)

        self.hline(top + 2, x, w)

        log_y = top + 3
        log_h = layout["content_bottom"] - log_y + 1
        if log_h <= 0:
            return

        if node.is_group:
            # A group only has logs when its expansion failed — show those
            # instead of the metadata, or the failure is invisible.
            if node.logs and node.status == STATUS_FAIL:
                for i, line in enumerate(node.logs[-log_h:]):
                    self.put(log_y + i, x, line[:w].ljust(w),
                             curses.color_pair(CP_FAIL) | curses.A_BOLD)
                return
            self._draw_group_detail(node, x, w, log_y, log_h)
            return

        logs = node.logs
        if not logs:
            msg = {STATUS_PENDING: "Pending…", STATUS_RUNNING: "Starting…",
                   STATUS_SKIPPED: "Skipped (restored from snapshot)"}.get(node.status, "No logs.")
            self.put(log_y, x, msg[:w], curses.color_pair(CP_LOG) | curses.A_DIM)
            return

        end = len(logs) - self.log_scroll
        end = max(1, min(end, len(logs)))
        window = logs[max(0, end - log_h):end]
        for i, line in enumerate(window):
            self.put(log_y + i, x, line[:w].ljust(w),
                     curses.color_pair(CP_LOG) | curses.A_BOLD)

    def _draw_group_detail(self, node, x, w, y, h):
        meta = node.meta
        lines = []
        if meta:
            lines.append(("title", meta.title))
            if meta.desc:
                lines.append(("desc", meta.desc))
            lines.append(("env", os.path.basename(meta.env_file) if meta.env_file else "-"))
            lines.append(("mode", "chroot" if meta.chroot else "host"))
            lines.append(("steps", "%d" % len(node.children)))
            lines.append(("snapshot paths", " ".join(meta.snapshot_paths) or "(none declared)"))
            if meta.snapshot_exclude:
                lines.append(("excludes", " ".join(meta.snapshot_exclude)))

            manifest = self.snapshots.get(meta.dir_name)
            if manifest:
                lines.append(("last snapshot", "%s  %s  %s" % (
                    format_when(manifest.get("created", 0)),
                    human_bytes(manifest_size(manifest)),
                    manifest.get("git_commit") or "-")))
                for entry in manifest.get("entries", []):
                    ratio = (entry.get("bytes_raw") or 0) / max(1, entry.get("bytes_compressed") or 1)
                    lines.append(("  " + entry["path"], "%s -> %s  x%.1f  %s files" % (
                        human_bytes(entry.get("bytes_raw")),
                        human_bytes(entry.get("bytes_compressed")),
                        ratio, "{:,}".format(entry.get("files", 0)))))
            else:
                lines.append(("last snapshot", "none"))

        for i, (key, value) in enumerate(lines[:h]):
            self.put(y + i, x, "%-16s" % key, curses.color_pair(CP_TIME) | curses.A_BOLD)
            self.put(y + i, x + 17, str(value)[:max(0, w - 17)], curses.color_pair(CP_BG))

    def _draw_hud(self, layout):
        sampler = self.sampler
        y = layout["hud_top"]
        self.hline(y - 1, 1, self.w - 2)

        lines = self.manager.total_lines
        rate = sampler.lines_per_sec if sampler else 0.0
        spark_w = max(0, min(24, self.w - 60))
        spark = sampler.sparkline(spark_w) if sampler else ""

        left = " out %s %s  %s ln  %d ln/s" % (
            GLYPH_SEP, spark, _short_count(lines), int(rate))
        if sampler and sampler.fs_files:
            left += "  %s fs %s files %s" % (GLYPH_SEP, "{:,}".format(sampler.fs_files),
                                             human_bytes(sampler.fs_bytes))
        self.put(y, 1, left.ljust(self.w - 2)[:self.w - 2], curses.color_pair(CP_BG))

        if layout["hud_h"] < 2:
            return

        snap_note, snap_attr = "-", curses.color_pair(CP_DIM)
        if self.manager.snapshot_activity:
            act = self.manager.snapshot_activity
            snap_note = "%s %s %s" % (act["action"], act["path"], human_bytes(act["bytes"]))
            snap_attr = curses.color_pair(CP_SNAP) | curses.A_BOLD
        elif self.manager.last_snapshot:
            name, state, detail, _when = self.manager.last_snapshot
            mark = {"ok": GLYPH_OK, "partial": "!", "failed": "!!",
                    "aborted": "-", "skipped": "-"}.get(state, "?")
            snap_note = "%s %s %s" % (name, mark, detail or state)
            snap_attr = {
                "ok": curses.color_pair(CP_DONE),
                "partial": curses.color_pair(CP_WARN) | curses.A_BOLD,
                "failed": curses.color_pair(CP_FAIL) | curses.A_BOLD,
            }.get(state, curses.color_pair(CP_DIM))

        second = " sys %s %.2f load  %s/%s mem  %s free  %s snap " % (
            GLYPH_SEP,
            sampler.load1 if sampler else 0.0,
            human_bytes(sampler.mem_used if sampler else 0),
            human_bytes(sampler.mem_total if sampler else 0),
            human_bytes(sampler.disk_free if sampler else 0),
            GLYPH_SEP)
        self.put(y + 1, 1, second, curses.color_pair(CP_DIM))
        self.put(y + 1, 1 + len(second), snap_note, snap_attr)
        used = 1 + len(second) + len(snap_note)

        # Most recent build-level notice (snapshot failures land here).
        if self.manager.messages and used + 12 < self.w:
            when, text = self.manager.messages[-1]
            if time.time() - when < 120:
                attr = curses.color_pair(CP_WARN) | curses.A_BOLD \
                    if ("FAIL" in text or "PARTIAL" in text or "unsafe" in text) \
                    else curses.color_pair(CP_DIM)
                self.put(y + 1, used + 2, ("%s %s" % (GLYPH_SEP, text))[:self.w - used - 3],
                         attr)

    def _draw_footer(self, layout):
        y = layout["footer_y"]
        nodes = self.manager.execution_order
        countable = [n for n in nodes if not n.is_group and n.status != STATUS_SKIPPED]
        total = len(countable)
        done = sum(1 for n in countable if n.status in (STATUS_DONE, STATUS_FAIL))
        # A phase that has not been expanded yet contributes no steps, which
        # would otherwise read as 100% while whole phases are still pending.
        unexpanded = sum(1 for g in self.manager.roots
                         if not g.children and g.status in (STATUS_PENDING, STATUS_RUNNING))
        total += unexpanded
        pct = done / total if total else 0.0

        x = 2
        mode = "AUTO" if self.auto_follow else "MANUAL"
        for key, label in (("F", mode), ("S", "Snap"), ("Q", "Quit")):
            self.put(y, x, "[%s]" % key, curses.color_pair(CP_KEY) | curses.A_BOLD)
            x += 3
            self.put(y, x, ":%s " % label, curses.color_pair(CP_BG))
            x += len(label) + 2

        elapsed = time.time() - (self.manager.start_time or time.time())
        info = "%s  %d%% (%d/%d) " % (human_time(elapsed), int(pct * 100), done, total)
        self.put(y, x, info, curses.color_pair(CP_BG) | curses.A_BOLD)
        x += len(info)

        bar_w = (self.w - 2) - x
        if bar_w >= 6:
            self.put(y, x, "[", curses.color_pair(CP_BG) | curses.A_BOLD)
            self.put(y, x + bar_w - 1, "]", curses.color_pair(CP_BG) | curses.A_BOLD)
            self.bar(y, x + 1, bar_w - 2, pct,
                     curses.color_pair(CP_DONE_REV) | curses.A_BOLD,
                     curses.color_pair(CP_DIM))

    def _draw_snapshot_overlay(self, state):
        box_w = min(self.w - 6, 70)
        box_h = 7
        if box_w < 30 or self.h < box_h + 4:
            return
        top = (self.h - box_h) // 2
        left = (self.w - box_w) // 2

        for i in range(box_h):
            self.put(top + i, left, " " * box_w, curses.color_pair(CP_HL))

        title = " %s %s  %s " % (GLYPH_SNAP, state["action"].upper(), state["phase"])
        self.put(top, left + 2, title, curses.color_pair(CP_SNAP_REV) | curses.A_BOLD)

        est = state.get("est_bytes") or 0
        pct = min(1.0, state["bytes"] / est) if est else 0.0
        frame = SPINNER[int(time.time() * 10) % len(SPINNER)]

        self.put(top + 2, left + 2, "%s %s" % (frame, state["path"]),
                 curses.color_pair(CP_HL) | curses.A_BOLD)
        self.bar(top + 3, left + 2, box_w - 4, pct,
                 curses.color_pair(CP_SNAP_REV) | curses.A_BOLD,
                 curses.color_pair(CP_HL))
        detail = "%s%s  %s/s  %s files" % (
            human_bytes(state["bytes"]),
            ("/" + human_bytes(est)) if est else "",
            human_bytes(state["rate"]),
            "{:,}".format(state["files"]))
        self.put(top + 4, left + 2, detail.ljust(box_w - 4), curses.color_pair(CP_HL))
        current = state.get("current", "")
        if current:
            self.put(top + 5, left + 2, current[-(box_w - 4):].ljust(box_w - 4),
                     curses.color_pair(CP_HL) | curses.A_DIM)

    # ----- input ----------------------------------------------------------

    def input(self):
        key = self.stdscr.getch()
        if key == -1:
            return

        if key in (ord('q'), ord('Q')):
            self.quit_requested = True
            self.manager.stop_requested = True
        elif key in (ord('f'), ord('F')):
            self.auto_follow = True
            self.log_scroll = 0
        elif key in (ord('s'), ord('S')):
            group = self.manager.current_phase
            if group and group.meta:
                self.manager.snapshot_request = group
                self.manager.notice("snapshot of %s queued" % group.meta.dir_name)
        elif key in (curses.KEY_UP, ord('k')):
            self._move(-1)
        elif key in (curses.KEY_DOWN, ord('j')):
            self._move(1)
        elif key == curses.KEY_PPAGE:
            self._move(-10)
        elif key == curses.KEY_NPAGE:
            self._move(10)
        elif key in (curses.KEY_LEFT, ord('h')):
            if self.selected_node and self.selected_node.is_group:
                self.selected_node.expanded = False
        elif key in (curses.KEY_RIGHT, ord('l')):
            if self.selected_node and self.selected_node.is_group:
                self.selected_node.expanded = True
        elif key == ord('['):
            self.log_scroll += 5
        elif key == ord(']'):
            self.log_scroll = max(0, self.log_scroll - 5)

    def _move(self, delta):
        self.auto_follow = False
        self.log_scroll = 0
        try:
            idx = self.visible_nodes.index(self.selected_node)
        except ValueError:
            return
        new_idx = min(len(self.visible_nodes) - 1, max(0, idx + delta))
        self.selected_node = self.visible_nodes[new_idx]


def _short_count(n):
    if n < 1000:
        return str(n)
    if n < 1000000:
        return "%.1fk" % (n / 1000.0)
    return "%.1fM" % (n / 1000000.0)
