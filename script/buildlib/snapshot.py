# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

"""Phase snapshots: create, list, plan a restore, restore, delete.

One directory per phase under `build/snapshots/`, one archive per declared
path, plus a manifest. Retention is one snapshot per phase: re-running a phase
overwrites its snapshot.
"""

import json
import os
import shutil
import subprocess
import threading
import time

SCHEMA = 3
MANIFEST = "manifest.json"
# Written at the build/ root (not under snapshots/) while a restore is
# rewriting the tree, so `make cleanbuild` discards the marker together with
# the truncated tree it describes.
RESTORE_MARKER = ".restore-in-progress"

# Flags we would like tar to have; probed against `tar --help` because Alpine's
# GNU tar may be built without acl/xattr support and busybox tar has almost none
# of them.
WANTED_TAR_FLAGS = (
    "--numeric-owner",
    "--one-file-system",
    "--sparse",
    "--xattrs",
    "--acls",
)
EXTRACT_TAR_FLAGS = ("--numeric-owner", "--xattrs", "--acls")
# Root-only: tar refuses to chown as a normal user, and drops setuid/setgid
# bits unless told to keep them. The build runs as root, so this is the normal
# path; it is skipped when someone inspects a snapshot unprivileged.
EXTRACT_TAR_FLAGS_ROOT = ("--same-owner", "--same-permissions")


def is_safe_relpath(path):
    """A snapshot path must stay inside $BUILD_DIR.

    Snapshot and restore delete and re-extract these paths as root, so anything
    absolute, empty, or climbing out with `..` is refused rather than sanitised.
    """
    if not path or path in (".", "/"):
        return False
    if os.path.isabs(path) or path.startswith("~"):
        return False
    return ".." not in path.split("/")


def git_info(repo_root):
    """Commit + dirtiness of the source tree, if discoverable.

    Inside the build container `.git` is not mounted, so the Makefile passes
    the values through the environment; the git commands are the fallback for
    running build.py directly on the host.
    """
    commit = os.environ.get("KDOS_GIT_COMMIT") or None
    dirty_env = os.environ.get("KDOS_GIT_DIRTY")
    dirty = None if dirty_env is None else dirty_env not in ("", "0", "false")

    if commit is None:
        try:
            commit = subprocess.check_output(
                ["git", "-C", repo_root, "rev-parse", "--short", "HEAD"],
                text=True, stderr=subprocess.DEVNULL).strip() or None
        except Exception:
            commit = None
    if dirty is None and commit is not None:
        try:
            out = subprocess.check_output(
                ["git", "-C", repo_root, "status", "--porcelain"],
                text=True, stderr=subprocess.DEVNULL)
            dirty = bool(out.strip())
        except Exception:
            dirty = None
    return commit, dirty


def dir_usage(path):
    """(bytes, files) for `path`, staying on one filesystem."""
    if not os.path.exists(path):
        return 0, 0
    if not os.path.isdir(path) or os.path.islink(path):
        try:
            return os.lstat(path).st_size, 1
        except OSError:
            return 0, 0

    try:
        root_dev = os.lstat(path).st_dev
    except OSError:
        return 0, 0

    total = 0
    files = 0
    stack = [path]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as it:
                for entry in it:
                    try:
                        st = entry.stat(follow_symlinks=False)
                    except OSError:
                        continue
                    if st.st_dev != root_dev:
                        continue          # separate mount, not ours to count
                    if entry.is_dir(follow_symlinks=False):
                        stack.append(entry.path)
                    else:
                        files += 1
                        total += st.st_size
        except OSError:
            continue
    return total, files


class SnapshotStore:
    def __init__(self, build_dir, repo_root=".", root=None):
        self.build_dir = os.path.abspath(build_dir)
        self.repo_root = repo_root
        self.root = root or os.path.join(self.build_dir, "snapshots")
        self.codec, self.compress_cmd, self.decompress_cmd, self.suffix = self._probe_codec()
        self.tar_help = self._probe_tar_help()
        self.tar_flags = [f for f in WANTED_TAR_FLAGS if f in self.tar_help]

    # ----- probing --------------------------------------------------------

    @staticmethod
    def _have(tool):
        return shutil.which(tool) is not None

    def _probe_codec(self):
        if self._have("zstd"):
            return "zstd", ["zstd", "-3", "-T0", "-q", "-c", "-"], ["zstd", "-dc"], ".tar.zst"
        if self._have("gzip"):
            return "gzip", ["gzip", "-1", "-c"], ["gzip", "-dc"], ".tar.gz"
        return "none", ["cat"], ["cat"], ".tar"

    def _probe_tar_help(self):
        try:
            out = subprocess.run(["tar", "--help"], capture_output=True, text=True,
                                 timeout=15)
            return (out.stdout or "") + (out.stderr or "")
        except Exception:
            return ""

    def _extract_flags(self):
        wanted = list(EXTRACT_TAR_FLAGS)
        if os.geteuid() == 0:
            wanted += list(EXTRACT_TAR_FLAGS_ROOT)
        return [f for f in wanted if f in self.tar_help]

    def archive_suffix_for(self, manifest):
        return {"zstd": ".tar.zst", "gzip": ".tar.gz"}.get(manifest.get("codec"), ".tar")

    def decompressor_for(self, manifest):
        codec = manifest.get("codec")
        if codec == "zstd":
            return ["zstd", "-dc"]
        if codec == "gzip":
            return ["gzip", "-dc"]
        return ["cat"]

    # ----- inventory ------------------------------------------------------

    def phase_dir(self, dir_name):
        return os.path.join(self.root, dir_name)

    def load(self, dir_name):
        path = os.path.join(self.phase_dir(dir_name), MANIFEST)
        try:
            with open(path, 'r') as f:
                manifest = json.load(f)
        except Exception:
            return None
        if not isinstance(manifest, dict) or "entries" not in manifest:
            return None
        # Snapshots written before schema 3 are always whole-phase snapshots.
        manifest.setdefault("complete", True)
        for entry in manifest["entries"]:
            archive = os.path.join(self.phase_dir(dir_name), entry.get("archive", ""))
            if not os.path.isfile(archive):
                return None               # incomplete: treat as absent
        return manifest

    def list(self):
        out = {}
        try:
            names = sorted(os.listdir(self.root))
        except OSError:
            return out
        for name in names:
            if not os.path.isdir(os.path.join(self.root, name)):
                continue
            manifest = self.load(name)
            if manifest:
                out[name] = manifest
        return out

    def delete(self, dir_name):
        target = self.phase_dir(dir_name)
        if os.path.isdir(target):
            shutil.rmtree(target, ignore_errors=True)
            return True
        return False

    # ----- guards ---------------------------------------------------------

    def mounts_under(self, path):
        path = os.path.abspath(path)
        found = []
        try:
            with open("/proc/mounts", 'r') as f:
                for line in f:
                    parts = line.split()
                    if len(parts) < 2:
                        continue
                    mnt = parts[1].replace("\\040", " ")
                    if mnt == path or mnt.startswith(path + os.sep):
                        found.append(mnt)
        except OSError:
            pass
        return found

    def marker_path(self):
        return os.path.join(self.build_dir, RESTORE_MARKER)

    def interrupted_restore(self):
        """Details of a restore that never finished, or None."""
        try:
            with open(self.marker_path(), 'r') as f:
                return json.load(f)
        except (OSError, ValueError):
            return None

    def _write_marker(self, target, paths):
        try:
            with open(self.marker_path(), 'w') as f:
                json.dump({"target": target, "paths": paths, "started": time.time()}, f)
        except OSError:
            pass

    def clear_marker(self):
        _unlink(self.marker_path())

    def free_bytes(self):
        try:
            st = os.statvfs(self.build_dir)
            return st.f_bavail * st.f_frsize
        except OSError:
            return None

    # ----- create ---------------------------------------------------------

    def create(self, meta, steps=0, duration=0.0, on_progress=None, should_stop=None,
               complete=True, total_steps=None, done_steps=None):
        """Snapshot one phase. Returns the manifest, or None when skipped.

        `complete` is False for a snapshot taken part-way through a phase (the
        [S] hotkey). Restoring one of those resumes inside the phase instead of
        skipping it, so the remaining steps still run.
        """
        interrupted = self.interrupted_restore()
        if interrupted:
            raise RuntimeError("build/ is mid-restore of %s; refusing to snapshot it"
                               % interrupted.get("target", "?"))

        unsafe = [p for p in meta.snapshot_paths if not is_safe_relpath(p)]
        if unsafe:
            raise RuntimeError("refusing unsafe snapshot path(s): %s" % " ".join(unsafe))

        paths = [p for p in meta.snapshot_paths
                 if os.path.exists(os.path.join(self.build_dir, p))]
        if not paths:
            return None

        blockers = self.mounts_under(os.path.join(self.build_dir, "fs"))
        if blockers:
            raise RuntimeError("mounts still active under build/fs: %s" % ", ".join(blockers[:3]))

        previous = self.load(meta.dir_name) or {}
        prev_entries = {e["path"]: e for e in previous.get("entries", [])}
        prev_total = sum(e.get("bytes_compressed", 0) for e in prev_entries.values())

        raw_sizes = {}
        for path in paths:
            raw_sizes[path] = dir_usage(os.path.join(self.build_dir, path))[0]
        # No history to go on for the first snapshot of a phase: zstd -3 on a
        # rootfs lands near 3x, so a third of the raw size is the estimate.
        needed = prev_total or int(sum(raw_sizes.values()) / 3)
        free = self.free_bytes()
        if needed and free is not None and free < needed * 1.2:
            raise RuntimeError("only %s free, need ~%s" %
                               (_hb(free), _hb(int(needed * 1.2))))

        dest = self.phase_dir(meta.dir_name)
        os.makedirs(dest, exist_ok=True)

        commit, dirty = git_info(self.repo_root)
        entries = []
        pending = []          # (tmp, final) — swapped in only once all succeed
        started = time.time()

        try:
            for path in paths:
                if should_stop and should_stop():
                    raise RuntimeError("aborted")
                entry, tmp_path = self._archive_path(
                    meta, path, dest, prev_entries.get(path, {}),
                    raw_sizes.get(path, 0), on_progress, should_stop)
                entries.append(entry)
                pending.append((tmp_path, os.path.join(dest, entry["archive"])))
        except BaseException:
            # Leave the previous snapshot intact: drop every partial archive,
            # including the one that was mid-write when this blew up.
            for name in os.listdir(dest):
                if name.endswith(".tmp"):
                    _unlink(os.path.join(dest, name))
            raise

        # Past this point the old archives are replaced; the manifest is
        # rewritten immediately after, so the window of inconsistency is a
        # handful of renames rather than the whole compression run.
        try:
            os.remove(os.path.join(dest, MANIFEST))
        except OSError:
            pass
        for tmp_path, final in pending:
            os.replace(tmp_path, final)

        manifest = {
            "schema": SCHEMA,
            "phase_dir": meta.dir_name,
            "phase": meta.name,
            "title": meta.title,
            "created": time.time(),
            "created_iso": time.strftime("%Y-%m-%dT%H:%M:%S"),
            "git_commit": commit,
            "git_dirty": dirty,
            "duration_s": round(duration, 1),
            "snapshot_s": round(time.time() - started, 1),
            "steps": steps,
            "complete": bool(complete),
            "total_steps": total_steps if total_steps is not None else steps,
            "done_steps": list(done_steps or [])[-200:],
            "codec": self.codec,
            "entries": entries,
        }

        tmp = os.path.join(dest, MANIFEST + ".tmp")
        with open(tmp, 'w') as f:
            json.dump(manifest, f, indent=2)
        os.replace(tmp, os.path.join(dest, MANIFEST))

        # Drop archives left over from a previous set of declared paths.
        keep = {e["archive"] for e in entries} | {MANIFEST}
        for name in os.listdir(dest):
            if name not in keep:
                target = os.path.join(dest, name)
                if os.path.isdir(target):
                    shutil.rmtree(target, ignore_errors=True)
                else:
                    os.remove(target)
        return manifest

    def _archive_name(self, path):
        return path.replace(os.sep, "_") + self.suffix

    def _archive_path(self, meta, path, dest, prev_entry, raw_bytes,
                      on_progress, should_stop):
        archive = self._archive_name(path)
        out_tmp = os.path.join(dest, archive + ".tmp")
        est = prev_entry.get("bytes_compressed") or 0

        cmd = ["tar"] + self.tar_flags + ["-C", self.build_dir]
        for pattern in meta.snapshot_exclude:
            if pattern == path or pattern.startswith(path + "/"):
                cmd += ["--exclude", pattern]
        cmd += ["-cvf", "-", path]

        state = {
            "action": "snapshot",
            "phase": meta.dir_name,
            "path": path,
            "bytes": 0,
            "est_bytes": est,
            "files": 0,
            "est_files": prev_entry.get("files") or 0,
            "rate": 0.0,
            "current": "",
            "started": time.time(),
        }

        files, _ = self._pipe(cmd, self.compress_cmd, out_tmp, state, on_progress,
                              should_stop)
        return {
            "path": path,
            "archive": archive,
            "bytes_raw": raw_bytes,
            "bytes_compressed": os.path.getsize(out_tmp),
            "files": files,
        }, out_tmp

    # ----- restore --------------------------------------------------------

    def plan_restore(self, phases, target_index):
        """Layered, newest-wins selection of paths across snapshots 0..N.

        Returns a list of {path, archive, manifest, entry, source} in
        restoration order; each path appears exactly once, taken from the
        newest snapshot at or below `target_index` that contains it.
        """
        snaps = self.list()
        chosen = {}
        for phase in phases:
            if phase.index > target_index:
                break
            manifest = snaps.get(phase.dir_name)
            if not manifest:
                continue
            for entry in manifest["entries"]:
                chosen[entry["path"]] = {
                    "path": entry["path"],
                    "archive": os.path.join(self.phase_dir(phase.dir_name),
                                            entry["archive"]),
                    "manifest": manifest,
                    "entry": entry,
                    "source": phase.dir_name,
                }
        return [chosen[k] for k in sorted(chosen)]

    def restore(self, plan, on_progress=None, should_stop=None, target=None):
        # Validate everything before touching the tree, so a rejected plan
        # leaves build/ exactly as it was — and unmarked.
        for item in plan:
            if not is_safe_relpath(item["path"]):
                raise RuntimeError("refusing unsafe restore path: %r" % item["path"])
            if not os.path.isfile(item["archive"]):
                raise RuntimeError("missing archive: %s" % item["archive"])

        # From the first rmtree until the last member is extracted the tree is
        # inconsistent; the marker is what lets the next run know that.
        self._write_marker(target or (plan[0]["source"] if plan else "?"),
                           [i["path"] for i in plan])
        for item in plan:
            if should_stop and should_stop():
                raise RuntimeError("aborted")

            target = os.path.join(self.build_dir, item["path"])
            if os.path.isdir(target) and not os.path.islink(target):
                blockers = self.mounts_under(target)
                if blockers:
                    raise RuntimeError("mounts active under %s: %s" %
                                       (item["path"], ", ".join(blockers[:3])))
                shutil.rmtree(target)
            elif os.path.lexists(target):
                os.remove(target)

            parent = os.path.dirname(target)
            if parent:
                os.makedirs(parent, exist_ok=True)

            state = {
                "action": "restore",
                "phase": item["source"],
                "path": item["path"],
                "bytes": 0,
                "est_bytes": item["entry"].get("bytes_compressed") or 0,
                "files": 0,
                "est_files": item["entry"].get("files") or 0,
                "rate": 0.0,
                "current": "",
                "started": time.time(),
            }

            decomp = self.decompressor_for(item["manifest"]) + [item["archive"]]
            tar_cmd = ["tar"] + self._extract_flags() + ["-C", self.build_dir, "-xvf", "-"]
            self._pipe(decomp, tar_cmd, None, state, on_progress, should_stop,
                       names_from="second-stdout", input_size=_size(item["archive"]))

        self.clear_marker()

    # ----- plumbing -------------------------------------------------------

    def _pipe(self, first_cmd, second_cmd, out_path, state, on_progress,
              should_stop, names_from="first-stderr", input_size=None):
        """Run `first | second`, optionally to a file, reporting progress.

        Which stream carries tar's member names depends on the direction:
        creating an archive puts the archive itself on stdout so `-v` names go
        to stderr, while extracting puts the names on stdout. Getting this
        backwards costs both the file counter and, worse, tar's diagnostics —
        so the name stream is named explicitly and every other stream is
        collected as error output.

        Byte progress is the output file's size when writing an archive, and
        how much of the input archive the decompressor has read when
        extracting. Rendering happens on the caller's thread: the ticker only
        updates `state`, so a curses on_progress can never run concurrently
        with the caller's own drawing.
        """
        counter = {"files": 0, "current": "", "err": [], "names": []}
        names_from_stdout = names_from == "second-stdout"
        if out_path and names_from_stdout:
            raise ValueError("cannot read names from stdout while writing it to a file")

        out_fh = open(out_path, 'wb') if out_path else None
        stop_ticker = threading.Event()
        threads = []
        tick = None

        try:
            first = subprocess.Popen(first_cmd, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE)
            second_stdout = out_fh if out_path else (
                subprocess.PIPE if names_from_stdout else subprocess.DEVNULL)
            second = subprocess.Popen(second_cmd, stdin=first.stdout,
                                      stdout=second_stdout, stderr=subprocess.PIPE)
            first.stdout.close()

            def drain(stream, collect_names):
                for raw in iter(stream.readline, b""):
                    line = raw.decode("utf-8", "replace").rstrip()
                    if not line:
                        continue
                    if collect_names:
                        counter["files"] += 1
                        counter["current"] = line
                        # tar interleaves its own errors with -v names on the
                        # same stream, so keep a tail for diagnostics.
                        counter["names"].append(line)
                        del counter["names"][:-5]
                    else:
                        counter["err"].append(line)
                        del counter["err"][:-20]
                stream.close()

            threads.append(threading.Thread(
                target=drain, args=(first.stderr, names_from == "first-stderr"),
                daemon=True))
            threads.append(threading.Thread(
                target=drain, args=(second.stderr, False), daemon=True))
            if names_from_stdout:
                threads.append(threading.Thread(
                    target=drain, args=(second.stdout, True), daemon=True))
            for t in threads:
                t.start()

            def ticker():
                last_bytes, last_t = 0, time.time()
                while not stop_ticker.wait(0.25):
                    if out_path:
                        done = _size(out_path)
                    elif input_size:
                        done = _consumed(first.pid, input_size)
                    else:
                        done = 0
                    now = time.time()
                    if now > last_t:
                        state["rate"] = max(0.0, (done - last_bytes) / (now - last_t))
                    last_bytes, last_t = done, now
                    state["bytes"] = done
                    state["files"] = counter["files"]
                    state["current"] = counter["current"]

            tick = threading.Thread(target=ticker, daemon=True)
            tick.start()

            while True:
                try:
                    second.wait(timeout=0.25)
                    break
                except subprocess.TimeoutExpired:
                    if on_progress:
                        on_progress(dict(state))
                    if should_stop and should_stop():
                        second.terminate()
                        first.terminate()
                        raise RuntimeError("aborted")

            first.wait()
        finally:
            stop_ticker.set()
            if tick is not None:
                tick.join()
            for t in threads:
                t.join(timeout=5)
            if out_fh is not None:
                out_fh.close()

        # GNU tar exits 1 for warnings ("file changed as we read it"); the
        # build is paused during a snapshot so treat that as non-fatal.
        for proc, name in ((first, first_cmd[0]), (second, second_cmd[0])):
            rc = proc.returncode
            if rc not in (0, None) and not (name == "tar" and rc == 1):
                tail = "; ".join((counter["err"] + counter["names"])[-3:])
                raise RuntimeError("%s exited %s%s" % (name, rc, ": " + tail if tail else ""))

        # The ticker may never have run for a short path, so publish the final
        # counts here rather than leaving the last sample empty.
        state["files"] = counter["files"]
        state["current"] = counter["current"]
        if on_progress:
            state["bytes"] = state.get("est_bytes") or state["bytes"]
            on_progress(dict(state))
        return counter["files"], counter["err"]


def _unlink(path):
    try:
        if path:
            os.remove(path)
    except OSError:
        pass


def _size(path):
    try:
        return os.path.getsize(path)
    except OSError:
        return 0


def _consumed(pid, total):
    """Bytes the decompressor has read from its input file, via /proc.

    `rchar` rather than `read_bytes`: the latter counts physical reads only and
    stays at zero for a page-cached archive.
    """
    try:
        with open("/proc/%d/io" % pid, 'r') as f:
            for line in f:
                if line.startswith("rchar:"):
                    return min(total, int(line.split()[1]))
    except OSError:
        pass
    return 0


def _hb(n):
    n = float(n or 0)
    for unit in ("B", "K", "M", "G", "T"):
        if n < 1024 or unit == "T":
            return "%.0f%s" % (n, unit) if unit == "B" else "%.1f%s" % (n, unit)
        n /= 1024.0
    return "%.1fT" % n
