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

from buildlib import plan as plan_mod                                  # noqa: E402
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
    p.add_argument("--continue-from", metavar="PHASE",
                   help="resume at PHASE on the existing build tree without restoring "
                        "anything; earlier phases are skipped and left alone")
    p.add_argument("--no-snapshot", action="store_true",
                   help="do not write snapshots during this build")
    p.add_argument("--plan", action="store_true",
                   help="open the build-plan picker (phases, steps, ports to rebuild) "
                        "and run it on the existing tree without restoring")
    p.add_argument("--phases", metavar="LIST",
                   help="only run these phases, comma separated "
                        "(e.g. --phases 01_phase1,06_packaging)")
    p.add_argument("--steps", metavar="LIST",
                   help="only run these scripts, PHASE:script.sh, comma separated "
                        "(e.g. --steps 01_phase1:00_file_system.sh)")
    p.add_argument("--rebuild", metavar="LIST",
                   help="force-rebuild these ports even though they are installed "
                        "(e.g. --rebuild niri,noctalia-shell)")
    p.add_argument("--snapshot", action="store_true",
                   help="write snapshots even for a partial plan (off by default: "
                        "a partial re-run would be filed under a phase it no longer is)")
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


def check_restorable(store, phases, target):
    """Why `target` cannot be restored, or None if it can.

    Restoring must never silently fall back to an older phase: the plan is
    layered, so it is non-empty whenever *any* earlier phase has a snapshot,
    and marking the requested phase SKIPPED on that basis would build the
    remaining phases against a rootfs that never had the target's packages.
    """
    snaps = store.list()
    if target.dir_name not in snaps:
        available = ", ".join(sorted(snaps)) or "none"
        return "no snapshot for %s (available: %s)" % (target.dir_name, available)

    # Every path any earlier phase declares must come from somewhere, or the
    # restored tree is missing a component (e.g. cross/ when 00/01 were deleted).
    declared = set()
    for phase in phases:
        if phase.index > target.index:
            break
        declared.update(phase.snapshot_paths)
    covered = {item["path"] for item in store.plan_restore(phases, target.index)}
    missing = sorted(declared - covered)
    if missing:
        return "snapshots for %s are incomplete: no source for %s" % (
            target.dir_name, " ".join(missing))
    return None


def effective_source(phases, plan, target):
    """The newest phase that actually contributed data to `plan`.

    The skip ceiling is derived from this rather than from the requested
    target, so the header can never claim a phase that supplied nothing.
    """
    sources = {item["source"] for item in plan}
    contributing = [p for p in phases if p.dir_name in sources]
    return max(contributing, key=lambda p: p.index) if contributing else target


def do_restore(stdscr, store, phases, target, manager):
    """Restore the layered plan for `target`, then skip the phases it covers."""
    plan = store.plan_restore(phases, target.index)
    if not plan:
        return "no snapshot data for %s" % target.dir_name

    manifest = store.list().get(target.dir_name, {})
    partial = not manifest.get("complete", True)

    screen = tui_mod.ProgressScreen(stdscr, " RESTORING %s " % target.dir_name)
    screen.log("plan: " + ", ".join("%s <- %s" % (i["path"], i["source"]) for i in plan))
    if partial:
        screen.log("partial snapshot (%s/%s steps) - %s will be re-run" % (
            manifest.get("steps", "?"), manifest.get("total_steps", "?"), target.dir_name))
    screen.log("[Q] cancel")
    screen.draw()

    try:
        store.restore(plan, on_progress=screen.on_progress,
                      should_stop=screen.cancelled, target=target.dir_name)
    except BaseException as exc:                      # includes KeyboardInterrupt
        return ("restore interrupted: %s\nbuild/ is now inconsistent - restore again "
                "or run 'make cleanbuild'" % exc)

    effective = effective_source(phases, plan, target)
    manager.mark_restored(effective.index, resume_inside=partial)
    manager.notice("restored %s%s (%s)" % (
        effective.dir_name, " [partial - phase re-runs]" if partial else "",
        ", ".join(i["path"] for i in plan)))
    return None


def _bail(stdscr, message):
    """Show a fatal message on the curses screen and wait for a key."""
    stdscr.nodelay(False)
    stdscr.erase()
    for i, line in enumerate(message.splitlines()):
        try:
            stdscr.addstr(1 + i, 2, line[:max(0, curses.COLS - 4)],
                          curses.color_pair(tui_mod.CP_FAIL) | curses.A_BOLD)
        except curses.error:
            pass
    try:
        stdscr.addstr(3 + message.count("\n"), 2, "press any key to exit")
    except curses.error:
        pass
    stdscr.refresh()
    stdscr.getch()
    return 1


def make_manager(args, store, timings, plan):
    """BuildManager for `plan`, with snapshots suppressed when it narrows work."""
    enabled = not args.no_snapshot
    if plan is not None and plan.narrows_execution() and not args.snapshot:
        enabled = False
    return BuildManager(args.script_dir, build_dir=args.build_dir,
                        snapshots=store, timings=timings,
                        snapshot_enabled=enabled, plan=plan)


def run_curses(stdscr, args, phases, store, timings, preselected=None,
               continue_at=None, plan=None):
    tui_mod.init_colors()
    stdscr.keypad(True)

    repo_root = os.path.dirname(os.path.abspath(args.script_dir))
    target = preselected

    def pick_plan():
        return tui_mod.PlanMenu(stdscr, phases, repo_root,
                                preset=plan_mod.BuildPlan.load(args.build_dir)).run()

    # Interactive entry points. A plan given on the command line wins outright.
    if plan is None and args.plan:
        plan = pick_plan()
        if plan is None:
            return 130
    elif plan is None and target is None and continue_at is None and not args.fresh \
            and store.list() and sys.stdin.isatty():
        commit, _ = git_info(".")
        choice, index = tui_mod.StartupMenu(stdscr, store, phases, commit).run()
        if choice == "quit":
            return 130
        if choice == "restore":
            target = phases[index]
        elif choice == "plan":
            plan = pick_plan()
            if plan is None:
                return 130

    manager = make_manager(args, store, timings, plan)

    if plan is not None and plan.is_custom():
        plan.save(args.build_dir)
        manager.notice("plan: %s" % (plan.summary() or "everything"))
        if plan.narrows_execution() and not args.snapshot:
            manager.notice("snapshots disabled for this partial run")

    if continue_at is not None:
        manager.mark_continued(continue_at.index)
        manager.notice("continuing at %s on the existing tree (no restore)"
                       % continue_at.dir_name)
    elif target is not None:
        error = check_restorable(store, phases, target) or \
            do_restore(stdscr, store, phases, target, manager)
        if error:
            return _bail(stdscr, error)
    elif store.interrupted_restore():
        # Starting a build on a half-extracted tree would bake the damage into
        # the next snapshot; make the user resolve it deliberately.
        stale = store.interrupted_restore()
        return _bail(stdscr, "a restore of %s never finished - build/ is inconsistent.\n"
                             "restore a snapshot again, or run 'make cleanbuild'."
                             % stale.get("target", "?"))

    sampler = Sampler(manager, args.build_dir)
    return _run_build(stdscr, manager, sampler, timings, store, args)


def _run_build(stdscr, manager, sampler, timings, store, args):
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

    # Resolve and validate --restore out here: an error message is far more
    # useful on a normal terminal than flashed through curses.wrapper.
    preselected = continue_at = None
    if args.restore and args.continue_from:
        print("--restore and --continue-from are mutually exclusive", file=sys.stderr)
        return 2

    if args.continue_from:
        continue_at = resolve_phase(phases, store, args.continue_from)
        if continue_at is None:
            print("unknown phase for --continue-from: %s" % args.continue_from,
                  file=sys.stderr)
            return 2

    if args.restore:
        preselected = resolve_phase(phases, store, args.restore)
        if preselected is None:
            print("no snapshot to restore" if args.restore == "latest"
                  else "unknown phase for --restore: %s" % args.restore, file=sys.stderr)
            return 2
        problem = check_restorable(store, phases, preselected)
        if problem:
            print(problem, file=sys.stderr)
            return 2

    plan = None
    if args.phases or args.steps or args.rebuild:
        plan, problem = plan_mod.BuildPlan.from_cli(args.phases, args.steps,
                                                    args.rebuild, phases)
        if problem:
            print(problem, file=sys.stderr)
            return 2
        if plan.narrows_execution() and (args.restore or args.continue_from):
            print("--phases/--steps select what runs; combine with neither "
                  "--restore nor --continue-from", file=sys.stderr)
            return 2

    if args.plan and not sys.stdin.isatty():
        print("--plan needs a terminal; use --phases/--steps/--rebuild instead",
              file=sys.stderr)
        return 2

    timings = TimingStore(os.path.join(store.root, "timings.json"))
    return curses.wrapper(run_curses, args, phases, store, timings, preselected,
                          continue_at, plan)


if __name__ == "__main__":
    sys.exit(main() or 0)
