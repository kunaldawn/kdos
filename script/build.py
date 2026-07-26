#!/usr/bin/env python3

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

"""KDOS build orchestrator: runs the phases, snapshots them, restores them."""

import argparse
import curses
import locale
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from buildlib import tui as tui_mod                                    # noqa: E402
from buildlib.phases import BuildManager, discover_phases, human_bytes  # noqa: E402
from buildlib.snapshot import SnapshotStore, git_info                   # noqa: E402
from buildlib.stats import Sampler, TimingStore                         # noqa: E402


def parse_args(argv=None):
    p = argparse.ArgumentParser(description="KDOS build orchestrator")
    p.add_argument("--build-dir", default=os.environ.get("KDOS_BUILD_DIR", "build"),
                   help="build output directory (default: build)")
    p.add_argument("--script-dir", default=os.path.join(os.path.dirname(
        os.path.abspath(__file__))), help=argparse.SUPPRESS)
    p.add_argument("--fresh", action="store_true",
                   help="skip the snapshot picker and run every phase")
    p.add_argument("--restore", metavar="PHASE",
                   help="restore a snapshot and continue after it "
                        "(phase name, directory name, 1-based index, or 'latest')")
    p.add_argument("--no-snapshot", action="store_true",
                   help="do not write snapshots during this build")
    p.add_argument("--list", action="store_true", help="list snapshots and exit")
    p.add_argument("--delete", metavar="PHASE", help="delete one snapshot and exit")
    return p.parse_args(argv)


def resolve_phase(phases, store, token):
    """Map a --restore/--delete token onto a PhaseMeta."""
    if not token:
        return None
    token = token.strip()
    snaps = store.list()

    if token == "latest":
        candidates = [p for p in phases if p.dir_name in snaps]
        return candidates[-1] if candidates else None

    for phase in phases:
        if token in (phase.dir_name, phase.name):
            return phase
    if token.isdigit():
        idx = int(token) - 1
        if 0 <= idx < len(phases):
            return phases[idx]
    return None


def cmd_list(phases, store):
    snaps = store.list()
    if not snaps:
        print("no snapshots in %s" % store.root)
        return 0
    commit, _ = git_info(".")
    print("%-3s %-16s %-17s %9s %10s %6s" % ("#", "PHASE", "WHEN", "SIZE", "COMMIT", "STEPS"))
    for i, phase in enumerate(phases, 1):
        manifest = snaps.get(phase.dir_name)
        if not manifest:
            continue
        total = sum(e.get("bytes_compressed", 0) for e in manifest["entries"])
        stale = tui_mod.manifest_stale(manifest, commit)
        print("%-3d %-16s %-17s %9s %10s %6s" % (
            i, phase.dir_name, tui_mod.format_when(manifest.get("created", 0)),
            human_bytes(total),
            (manifest.get("git_commit") or "-") + ("*" if stale else ""),
            manifest.get("steps", "-")))
        for entry in manifest["entries"]:
            print("      %-12s %9s <- %9s  %s files" % (
                entry["path"], human_bytes(entry.get("bytes_compressed")),
                human_bytes(entry.get("bytes_raw")),
                "{:,}".format(entry.get("files", 0))))
    return 0


def do_restore(stdscr, store, phases, target, manager):
    """Extract the layered restore plan for `target`, then skip those phases."""
    plan = store.plan_restore(phases, target.index)
    if not plan:
        return "no snapshot data for %s" % target.dir_name

    screen = tui_mod.ProgressScreen(stdscr, " RESTORING %s " % target.dir_name)
    screen.log("plan: " + ", ".join("%s <- %s" % (i["path"], i["source"]) for i in plan))
    screen.draw()
    try:
        store.restore(plan, on_progress=screen.on_progress)
    except Exception as exc:
        return "restore failed: %s" % exc

    manager.mark_restored(target.index)
    manager.notice("restored %s (%s)" % (
        target.dir_name, ", ".join(i["path"] for i in plan)))
    return None


def run_curses(stdscr, args, phases, store, timings):
    tui_mod.init_colors()
    stdscr.keypad(True)

    manager = BuildManager(args.script_dir, build_dir=args.build_dir,
                           snapshots=store, timings=timings,
                           snapshot_enabled=not args.no_snapshot)

    target = None
    if args.restore:
        target = resolve_phase(phases, store, args.restore)
        if target is None:
            raise SystemExit("no snapshot to restore" if args.restore == "latest"
                             else "unknown phase for --restore: %s" % args.restore)
    elif not args.fresh and store.list() and sys.stdin.isatty():
        commit, _ = git_info(".")
        choice, index = tui_mod.StartupMenu(stdscr, store, phases, commit).run()
        if choice == "quit":
            return 130
        if choice == "restore":
            target = phases[index]

    if target is not None:
        error = do_restore(stdscr, store, phases, target, manager)
        if error:
            stdscr.nodelay(False)
            stdscr.erase()
            stdscr.addstr(1, 2, error[:curses.COLS - 4],
                          curses.color_pair(tui_mod.CP_FAIL) | curses.A_BOLD)
            stdscr.addstr(3, 2, "press any key to exit")
            stdscr.refresh()
            stdscr.getch()
            return 1

    sampler = Sampler(manager, args.build_dir)
    screen = tui_mod.TUI(stdscr, manager, sampler=sampler, timings=timings, store=store)

    manager.start_build()
    sampler.start()

    try:
        while True:
            screen.update()
            screen.draw_screen()
            screen.input()

            if not manager.is_running:
                if screen.quit_requested:
                    break
                if manager.error_step:
                    # Hold the failed step on screen until the user quits.
                    if screen.auto_follow:
                        screen.auto_follow = False
                        screen.selected_node = manager.error_step
                    curses.napms(30)
                    continue
                if manager.stop_requested:
                    break

                msg = " BUILD COMPLETE - PRESS Q TO EXIT "
                h, w = stdscr.getmaxyx()
                try:
                    stdscr.addstr(h // 2, max(0, (w - len(msg)) // 2), msg,
                                  curses.color_pair(tui_mod.CP_DONE_REV) | curses.A_BOLD)
                except curses.error:
                    pass
                stdscr.refresh()
                while stdscr.getch() not in (ord('q'), ord('Q')):
                    curses.napms(50)
                break

            curses.napms(30)
    except KeyboardInterrupt:
        manager.stop_requested = True
    finally:
        sampler.stop()
        timings.save()

    return 1 if manager.error_step else 0


def main():
    locale.setlocale(locale.LC_ALL, "")
    args = parse_args()
    os.environ.setdefault('ESCDELAY', '25')

    phases = discover_phases(args.script_dir)
    store = SnapshotStore(args.build_dir, repo_root=".")
    os.makedirs(args.build_dir, exist_ok=True)

    if args.list:
        return cmd_list(phases, store)

    if args.delete:
        target = resolve_phase(phases, store, args.delete)
        if target is None:
            print("unknown phase: %s" % args.delete, file=sys.stderr)
            return 2
        print("deleted %s" % target.dir_name if store.delete(target.dir_name)
              else "no snapshot for %s" % target.dir_name)
        return 0

    timings = TimingStore(os.path.join(store.root, "timings.json"))
    return curses.wrapper(run_curses, args, phases, store, timings)


if __name__ == "__main__":
    sys.exit(main() or 0)
