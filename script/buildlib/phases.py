# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

"""Phase discovery, env-file metadata parsing and build execution."""

import glob
import os
import re
import select
import subprocess
import threading
import time

from .snapshot import is_safe_relpath

RE_TITLE = re.compile(r'^#\s*Title:\s*(.+)$', re.IGNORECASE)
RE_ANSI = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
RE_PHASE_DIR = re.compile(r'^[0-9]+_')
RE_EXPORT = re.compile(r'^[ \t]*export[ \t]+([A-Za-z_][A-Za-z0-9_]*)=(.*)$')
# Every port under ports/core matches this; anything else on kpkgdepends'
# stdout is noise, not a package.
RE_PORT_NAME = re.compile(r'^[A-Za-z0-9][A-Za-z0-9._+-]*$')

# Keys build.py cares about. CHROOT decides the execution wrapper; the KDOS_*
# keys carry the snapshot contract and the UI labels.
META_KEYS = (
    "CHROOT",
    "KDOS_PHASE_TITLE",
    "KDOS_PHASE_DESC",
    "KDOS_SNAPSHOT_PATHS",
    "KDOS_SNAPSHOT_EXCLUDE",
)

STATUS_PENDING = "PENDING"
STATUS_RUNNING = "RUNNING"
STATUS_DONE = "DONE"
STATUS_FAIL = "FAIL"
STATUS_SKIPPED = "SKIPPED"


def _unquote(raw):
    """Extract a literal value from the right-hand side of an `export` line.

    No shell expansion is performed: metadata values are required to be
    literals. Returns "" for anything containing an unsupported construct.
    """
    raw = raw.strip()
    if not raw:
        return ""
    quote = raw[0]
    if quote in ('"', "'"):
        end = raw.find(quote, 1)
        if end < 0:
            return ""
        return raw[1:end]
    # Unquoted: value ends at whitespace or a comment.
    return raw.split('#', 1)[0].split()[0] if raw.split('#', 1)[0].split() else ""


def parse_env_metadata(path):
    """Parse `export NAME=VALUE` lines out of a phase env file.

    The env files are never sourced: several of them run
    `rm -rf /var/cache/kpkg/work` at source time, which would hit the build
    container's own filesystem.
    """
    out = {}
    try:
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                m = RE_EXPORT.match(line)
                if not m:
                    continue
                key = m.group(1)
                if key not in META_KEYS:
                    continue
                out[key] = _unquote(m.group(2))
    except OSError:
        pass
    return out


def _sanitize_paths(paths):
    good, bad = [], []
    for path in paths:
        path = path.rstrip("/")
        (good if is_safe_relpath(path) else bad).append(path)
    return good, bad


class PhaseMeta:
    """Everything build.py knows about one phase directory."""

    def __init__(self, index, dir_name, dir_path, env_file):
        self.index = index
        self.dir_name = dir_name            # e.g. "03_phase3"
        self.dir_path = dir_path
        self.env_file = env_file if env_file and os.path.isfile(env_file) else None
        self.name = re.sub(r'^[0-9]+_', '', dir_name)   # e.g. "phase3"

        self.vars = parse_env_metadata(self.env_file) if self.env_file else {}
        self.chroot = self.vars.get("CHROOT", "") == "1"

        nice = self.name.replace('_', ' ').replace('-', ' ')
        self.title = self.vars.get("KDOS_PHASE_TITLE") or nice
        self.desc = self.vars.get("KDOS_PHASE_DESC", "")
        self.snapshot_paths, self.rejected_paths = _sanitize_paths(
            self.vars.get("KDOS_SNAPSHOT_PATHS", "").split())
        self.snapshot_exclude = self.vars.get("KDOS_SNAPSHOT_EXCLUDE", "").split()

    @property
    def label(self):
        base = self.name.replace('_', ' ').replace('-', ' ')
        return base + " (Chroot)" if self.chroot else base

    @property
    def snapshottable(self):
        return bool(self.snapshot_paths)


def discover_phases(root_dir):
    """Return the ordered PhaseMeta list for `script/`."""
    try:
        entries = sorted(os.listdir(root_dir))
    except OSError:
        return []

    phases = []
    for entry in entries:
        full = os.path.join(root_dir, entry)
        if not os.path.isdir(full):
            continue
        if not RE_PHASE_DIR.match(entry):
            continue        # buildlib/, util/, __pycache__/ ...
        name = re.sub(r'^[0-9]+_', '', entry)
        env_file = os.path.join(root_dir, "%s.env.sh" % name)
        phases.append(PhaseMeta(len(phases), entry, full, env_file))
    return phases


class BuildStep:
    def __init__(self, path, is_group=False, level=0):
        self.path = path
        self.is_group = is_group
        self.level = level
        self.children = []
        self.parent = None
        self.status = STATUS_PENDING
        self.logs = []
        self.start_time = None
        self.end_time = None
        self.return_code = 0
        self.step_number = 0
        self.raw_title = self._derive_title()
        self.step_type = "HOST"          # HOST, CHROOT, CUSTOM
        self.custom_cmd = None
        self.packages_file = None
        self.script_dir = None
        self.env_file = None
        self.meta = None                 # PhaseMeta, on group nodes
        self.note = ""                   # short UI annotation (snapshot state)
        self.snap_state = None           # ok | partial | failed | aborted | skipped
        self.snap_detail = ""
        self.expanded = True

    @property
    def title(self):
        if not self.is_group:
            return "%4d %s" % (self.step_number, self.raw_title)
        return self.raw_title

    def _derive_title(self):
        if not self.is_group:
            try:
                with open(self.path, 'r', encoding='utf-8', errors='ignore') as f:
                    for _ in range(5):
                        line = f.readline()
                        if not line:
                            break
                        m = RE_TITLE.match(line.strip())
                        if m:
                            return m.group(1)
            except OSError:
                pass

        name = os.path.basename(self.path)
        name = os.path.splitext(name)[0]
        name = re.sub(r'^[0-9]+_', '', name)
        return name.replace('_', ' ').replace('-', ' ')

    def add_log(self, line):
        line = RE_ANSI.sub('', line)
        line = line.replace('\t', '    ')
        line = "".join(ch for ch in line if ord(ch) >= 32)
        self.logs.append(line)
        if len(self.logs) > 2000:
            self.logs = self.logs[-2000:]

    def duration(self):
        if self.is_group:
            return sum(c.duration() for c in self.children)
        if self.start_time is None:
            return 0
        end = self.end_time if self.end_time else time.time()
        return end - self.start_time

    def timing_key(self):
        parent = self.parent.meta.dir_name if (self.parent and self.parent.meta) else "root"
        return "%s/%s" % (parent, self.raw_title)


class BuildManager:
    """Owns the execution order and runs it on a worker thread."""

    def __init__(self, root_dir, build_dir="build", snapshots=None, timings=None,
                 snapshot_enabled=True, repo_root=None, plan=None):
        self.root_dir = root_dir
        self.build_dir = build_dir
        self.plan = plan
        # chroot_exec.sh bind-mounts the repo root at /kdos and cds there, so
        # anything handed to a chroot command must be repo-relative: a host
        # path like /workspace/script/phase2.env.sh does not exist inside.
        self.repo_root = os.path.abspath(
            repo_root if repo_root else os.path.dirname(os.path.abspath(root_dir)))
        self.snapshots = snapshots
        self.timings = timings
        self.snapshot_enabled = snapshot_enabled

        self.roots = []
        self.execution_order = []
        self.current_step = None
        self.current_phase = None
        self.is_running = False
        self.stop_requested = False
        self.error_step = None
        self.start_time = None
        self.restored_from = None        # PhaseMeta the build was resumed from
        self.resumed_inside = None       # PhaseMeta being re-run after a partial restore
        self.continued_from = None       # set by --continue-from (no restore)
        self.total_lines = 0
        self.messages = []               # build-level notices for the UI
        self.last_snapshot = None        # (dir_name, state, detail, when)

        # Live snapshot progress, read by the TUI.
        self.snapshot_activity = None
        self.snapshot_request = None     # set by the TUI to force a snapshot
        self.forced_seen = set()         # plan rebuilds that actually matched

        self.phases = discover_phases(root_dir)
        self._build_tree()

    # ----- construction ---------------------------------------------------

    def _build_tree(self):
        for meta in self.phases:
            group = BuildStep(meta.label, is_group=True, level=0)
            group.meta = meta
            group.env_file = meta.env_file
            group.step_type = "CHROOT" if meta.chroot else "HOST"

            packages_file = os.path.join(meta.dir_path, "packages.txt")
            if os.path.isfile(packages_file):
                group.packages_file = packages_file
            else:
                group.script_dir = meta.dir_path

            # A plan deselects whole phases up front; mark_continued and
            # mark_restored only ever add more skips on top of this.
            if self.plan and not self.plan.phase_selected(meta.dir_name):
                group.status = STATUS_SKIPPED
                group.note = "not in plan"

            self.roots.append(group)
            self.execution_order.append(group)
        self._renumber_steps()

    def _renumber_steps(self):
        for i, root in enumerate(self.roots):
            root.step_number = i + 1
            for j, child in enumerate(root.children):
                child.step_number = j

    def notice(self, text):
        self.messages.append((time.time(), text))
        if len(self.messages) > 50:
            self.messages = self.messages[-50:]
        # The in-process list dies with curses, so keep a durable copy: a
        # snapshot that failed on the last phase must still be discoverable.
        try:
            log_dir = os.path.join(self.build_dir, "logs")
            os.makedirs(log_dir, exist_ok=True)
            with open(os.path.join(log_dir, "snapshots.log"), 'a') as f:
                f.write("%s %s\n" % (time.strftime("%Y-%m-%d %H:%M:%S"), text))
        except OSError:
            pass

    def mark_continued(self, phase_index):
        """Resume at `phase_index` on the existing tree, without restoring.

        Earlier phases are skipped, so they are not re-run and — importantly —
        not re-snapshotted: snapshotting phase 2 again from a tree that already
        holds phase 3 packages would file a mislabelled archive.
        """
        for group in self.roots:
            if group.meta.index < phase_index:
                group.status = STATUS_SKIPPED
                self.continued_from = group.meta

    def mark_restored(self, phase_index, resume_inside=False):
        """Skip the phases covered by a restored snapshot.

        `resume_inside` is for a partial snapshot: the phase it was taken from
        still has steps left, so it is left PENDING and re-runs. kpkg skips
        packages that are already installed, so re-running is cheap.
        """
        ceiling = phase_index - 1 if resume_inside else phase_index
        for group in self.roots:
            if group.meta.index <= ceiling:
                group.status = STATUS_SKIPPED
                self.restored_from = group.meta
            elif resume_inside and group.meta.index == phase_index:
                self.resumed_inside = group.meta

    # ----- helpers --------------------------------------------------------

    def _cmd_prefix(self, step_type):
        if step_type == "CHROOT":
            return [os.path.abspath(os.path.join(self.root_dir, "chroot_exec.sh")), "bash", "-c"]
        return ["bash", "-c"]

    def _path_for(self, path, step_type):
        """Path as the executing context sees it: repo-relative inside a chroot."""
        if not path:
            return path
        if step_type == "CHROOT":
            return os.path.relpath(os.path.abspath(path), self.repo_root)
        return path

    def _env_source(self, step):
        if not step.env_file:
            return ""
        return "source %s && " % self._path_for(step.env_file, step.step_type)

    def phase_of(self, step):
        if step is None:
            return None
        if step.is_group:
            return step
        return step.parent

    def pending_steps(self):
        return [s for s in self.execution_order
                if not s.is_group and s.status in (STATUS_PENDING, STATUS_RUNNING)]

    # ----- execution ------------------------------------------------------

    def start_build(self):
        self.start_time = time.time()
        self.is_running = True
        t = threading.Thread(target=self._run_loop, daemon=True)
        t.start()
        return t

    def _run_loop(self):
        try:
            self._run_loop_inner()
        except Exception as exc:          # never let the worker die silently
            self.notice("internal error: %s" % exc)
            self.is_running = False

    def _run_loop_inner(self):
        os.makedirs(os.path.join(self.build_dir, "logs"), exist_ok=True)

        idx = 0
        while idx < len(self.execution_order):
            if self.stop_requested:
                break

            step = self.execution_order[idx]

            if step.status == STATUS_SKIPPED:
                idx += 1
                continue

            self.current_step = step
            if step.is_group:
                self.current_phase = step

            if step.is_group and step.packages_file and not step.children:
                if not self._expand_packages(step, idx):
                    return
                if not step.children:
                    self._finish_phase(step)     # phase resolved to no work
                idx += 1
                continue

            if step.is_group and step.script_dir and not step.children:
                if not self._expand_scripts(step, idx):
                    return
                if not step.children:
                    self._finish_phase(step)
                idx += 1
                continue

            if step.status == STATUS_DONE or step.is_group:
                idx += 1
                continue

            self._run_step(step)

            if step.return_code == 0:
                self._update_family_status(step, STATUS_DONE)
                parent = step.parent
                if parent and parent.children and \
                        all(c.status == STATUS_DONE for c in parent.children):
                    self._finish_phase(parent)
            else:
                self._update_family_status(step, STATUS_FAIL)
                self.error_step = step
                self.stop_requested = True
                self.is_running = False
                return

            if self.snapshot_request is not None:
                target, self.snapshot_request = self.snapshot_request, None
                self._snapshot(target, forced=True)

            idx += 1

        # A rebuild the user asked for that never showed up is a silent no-op
        # otherwise: the port is not in any selected phase's dependency closure.
        if self.plan and self.plan.rebuild:
            missed = sorted(self.plan.rebuild - self.forced_seen)
            if missed:
                self.notice("NOT rebuilt (not reached by the selected phases): %s"
                            % " ".join(missed))

        self.is_running = False

    def _resolve_packages(self, cmd):
        """Run kpkgdepends and return the install order.

        kpkgdepends prints the resolved order on stdout and nothing else, so
        stderr is kept separate — merging it means any diagnostic written by
        the chroot wrapper (or by a sourced env file) gets split on whitespace
        and installed as a package. Tokens are validated for the same reason.
        """
        proc = subprocess.run(cmd, capture_output=True, text=True)
        stdout = (proc.stdout or "").strip()
        stderr = (proc.stderr or "").strip()

        if proc.returncode != 0:
            raise RuntimeError("kpkgdepends exited %d\n%s" %
                               (proc.returncode, (stderr or stdout)[-2000:]))

        resolved = stdout.split()
        bad = [t for t in resolved if not RE_PORT_NAME.match(t)]
        if bad:
            raise RuntimeError(
                "kpkgdepends returned %d token(s) that are not package names: %s\n"
                "full stdout:\n%s%s" % (len(bad), " ".join(bad[:5]), stdout[:1500],
                                        "\nstderr:\n" + stderr[:500] if stderr else ""))
        if not resolved:
            raise RuntimeError("kpkgdepends returned nothing%s" %
                               ("\nstderr:\n" + stderr[:500] if stderr else ""))
        return resolved

    def _expand_packages(self, step, idx):
        step.status = STATUS_RUNNING
        try:
            with open(step.packages_file, 'r') as f:
                pkgs = [line.strip() for line in f
                        if line.strip() and not line.strip().startswith('#')]

            if pkgs:
                cmd_prefix = self._cmd_prefix(step.step_type)
                env_src = self._env_source(step)
                resolve_cmd = "%sexport PKGDB_DIR=/dev/null && kpkgdepends %s" % (
                    env_src, ' '.join(pkgs))
                resolved = self._resolve_packages(cmd_prefix + [resolve_cmd])

                parent_dir = os.path.dirname(step.packages_file)
                new_nodes = []
                for i, pkg in enumerate(resolved):
                    node_name = "%02d_%s.install" % (i, pkg)
                    node = BuildStep(os.path.join(parent_dir, node_name), level=1)
                    node.parent = step
                    node.raw_title = pkg
                    node.step_type = "CUSTOM"
                    # -f only for ports the plan asked to rebuild: kpkg's -f
                    # really does force now, so passing it blanketly would
                    # rebuild the entire tree on every run.
                    forced = bool(self.plan and self.plan.forced(pkg))
                    if forced:
                        self.forced_seen.add(pkg)
                        node.note = "rebuild"
                    node.custom_cmd = cmd_prefix + ["%skpkg install%s %s" % (
                        env_src, " -f" if forced else "", pkg)]
                    new_nodes.append(node)

                step.children.extend(new_nodes)
                self.execution_order[idx + 1:idx + 1] = new_nodes
                self._renumber_steps()
            step.status = STATUS_DONE
            return True
        except Exception as exc:
            self._fail_expansion(step, "package resolution", exc)
            return False

    def _fail_expansion(self, step, what, exc):
        """Record an expansion failure where the user can actually see it."""
        step.status = STATUS_FAIL
        step.add_log("%s failed for %s:" % (what, step.raw_title))
        for line in str(exc).splitlines():
            step.add_log(line)
        try:
            log_dir = os.path.join(self.build_dir, "logs",
                                   os.path.basename(step.meta.dir_path) if step.meta else "")
            os.makedirs(log_dir, exist_ok=True)
            with open(os.path.join(log_dir, "expansion.log"), 'w') as f:
                f.write("%s failed\n%s\n" % (what, exc))
        except OSError:
            pass
        self.notice("%s: %s failed - see build/logs/.../expansion.log" %
                    (step.meta.dir_name if step.meta else step.raw_title, what))
        self.error_step = step
        self.stop_requested = True
        self.is_running = False

    def _expand_scripts(self, step, idx):
        step.status = STATUS_RUNNING
        try:
            files = sorted(glob.glob(os.path.join(step.script_dir, "*.sh")))
            if self.plan and step.meta:
                files = [f for f in files
                         if self.plan.step_selected(step.meta.dir_name,
                                                    os.path.basename(f))]
            if files:
                new_nodes = []
                for f in files:
                    node = BuildStep(f, level=1)
                    node.parent = step
                    node.step_type = step.step_type
                    new_nodes.append(node)
                step.children.extend(new_nodes)
                self.execution_order[idx + 1:idx + 1] = new_nodes
                self._renumber_steps()
            step.status = STATUS_DONE
            return True
        except Exception as exc:
            self._fail_expansion(step, "script discovery", exc)
            return False

    def _explicitly_selected(self, step):
        """True when the plan named this exact script (not just its phase)."""
        if not self.plan or step.is_group or not step.parent or not step.parent.meta:
            return False
        chosen = self.plan.steps.get(step.parent.meta.dir_name)
        return bool(chosen) and os.path.basename(step.path) in chosen

    def _log_path(self, step):
        rel_dir = os.path.dirname(os.path.relpath(step.path, self.root_dir))
        clean_name = re.sub(r'^[0-9]+_', '', os.path.basename(step.path))
        log_name = "%04d_%s.log" % (step.step_number, clean_name)
        return os.path.join(self.build_dir, "logs", rel_dir, log_name)

    def _run_step(self, step):
        self._update_family_status(step, STATUS_RUNNING)
        step.start_time = time.time()

        log_file_path = self._log_path(step)
        os.makedirs(os.path.dirname(log_file_path), exist_ok=True)

        cmd = ["bash", step.path]
        if step.custom_cmd:
            cmd = step.custom_cmd
        elif step.step_type == "CHROOT":
            wrapper = os.path.abspath(os.path.join(self.root_dir, "chroot_exec.sh"))
            cmd = [wrapper, "bash", self._path_for(step.path, step.step_type)]

        # A step the plan named explicitly must actually run: many steps guard
        # themselves with a mark file and exit 0 on a second pass.
        env = os.environ.copy()
        env["KDOS_REPLAY"] = "1" if self._explicitly_selected(step) else "0"

        with open(log_file_path, 'w') as lf:
            try:
                proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT,
                                        text=True, bufsize=1, env=env)
                while True:
                    if self.stop_requested:
                        proc.terminate()
                        break

                    ret = select.select([proc.stdout.fileno()], [], [], 0.05)
                    if ret[0]:
                        line = proc.stdout.readline()
                        if line:
                            clean = line.rstrip()
                            step.add_log(clean)
                            self.total_lines += 1
                            lf.write(clean + "\n")
                        else:
                            break
                    elif proc.poll() is not None:
                        rest = proc.stdout.read()
                        if rest:
                            for l in rest.splitlines():
                                step.add_log(l)
                                self.total_lines += 1
                                lf.write(l + "\n")
                        break

                step.return_code = proc.wait()
            except Exception as exc:
                step.add_log("INTERNAL ERROR: %s" % exc)
                step.return_code = 999

        step.end_time = time.time()
        if self.timings and step.return_code == 0:
            self.timings.record_step(step.timing_key(), step.duration())

    def _finish_phase(self, group):
        if self.timings:
            self.timings.record_phase(group.meta.dir_name, group.duration())
            self.timings.save()
        self._snapshot(group)

    def _set_snap_state(self, group, state, detail=""):
        group.snap_state = state
        group.snap_detail = detail
        group.note = detail or state
        self.last_snapshot = (group.meta.dir_name if group.meta else "?",
                              state, detail, time.time())

    def _snapshot(self, group, forced=False):
        if not self.snapshots or not (self.snapshot_enabled or forced):
            return
        meta = group.meta
        if meta and meta.rejected_paths:
            self.notice("%s: ignoring unsafe snapshot path(s): %s" %
                        (meta.dir_name, " ".join(meta.rejected_paths)))
        if not meta or not meta.snapshottable:
            if meta:
                self._set_snap_state(group, "skipped", "no snapshot paths")
            return

        done = [c.raw_title for c in group.children if c.status == STATUS_DONE]
        # A phase that resolved to no work is complete, not partial.
        complete = not group.children or len(done) == len(group.children)

        def progress(state):
            self.snapshot_activity = state

        try:
            manifest = self.snapshots.create(
                meta,
                steps=len(done),
                duration=group.duration(),
                on_progress=progress,
                should_stop=lambda: self.stop_requested,
                complete=complete,
                total_steps=len(group.children),
                done_steps=done,
                log=self.notice,
            )
        except Exception as exc:
            aborted = "abort" in str(exc).lower()
            self.notice("snapshot %s %s: %s" %
                        (meta.dir_name, "aborted" if aborted else "FAILED", exc))
            self._set_snap_state(group, "aborted" if aborted else "failed", str(exc)[:60])
            self.snapshot_activity = None
            return

        self.snapshot_activity = None
        if manifest is None:
            self.notice("snapshot %s skipped: declared paths do not exist" % meta.dir_name)
            self._set_snap_state(group, "skipped", "nothing to archive")
            return

        total = sum(e["bytes_compressed"] for e in manifest["entries"])
        if complete:
            self._set_snap_state(group, "ok", human_bytes(total))
            self.notice("snapshot %s -> %s" % (meta.dir_name, human_bytes(total)))
        else:
            self._set_snap_state(group, "partial", "%s @ %d/%d" %
                                 (human_bytes(total), len(done), len(group.children)))
            self.notice("PARTIAL snapshot %s at step %d/%d -> %s (restoring it re-runs "
                        "the phase)" % (meta.dir_name, len(done), len(group.children),
                                        human_bytes(total)))

    def _update_family_status(self, step, status):
        step.status = status
        curr = step.parent
        while curr:
            active = [c for c in curr.children if c.status != STATUS_SKIPPED]
            has_running = any(c.status == STATUS_RUNNING for c in active)
            has_fail = any(c.status == STATUS_FAIL for c in active)
            all_done = bool(active) and all(c.status == STATUS_DONE for c in active)
            has_started = any(c.status != STATUS_PENDING for c in active)

            if has_fail:
                curr.status = STATUS_FAIL
            elif has_running:
                curr.status = STATUS_RUNNING
            elif all_done:
                curr.status = STATUS_DONE
            elif has_started:
                curr.status = STATUS_RUNNING
            else:
                curr.status = STATUS_PENDING
            curr = curr.parent


def human_bytes(n):
    if n is None:
        return "-"
    n = float(n)
    for unit in ("B", "K", "M", "G", "T"):
        if n < 1024 or unit == "T":
            if unit == "B":
                return "%dB" % int(n)
            return "%.1f%s" % (n, unit)
        n /= 1024.0
    return "%.1fT" % n


def human_time(seconds):
    if seconds is None:
        return "--"
    seconds = int(seconds)
    if seconds < 60:
        return "%ds" % seconds
    if seconds < 3600:
        return "%dm%02ds" % (seconds // 60, seconds % 60)
    return "%dh%02dm" % (seconds // 3600, (seconds % 3600) // 60)
