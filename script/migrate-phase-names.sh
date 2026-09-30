#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------
#
# script/migrate-phase-names.sh — carry a build/ tree across the phase rename.
#
#   script/migrate-phase-names.sh [--dry-run] [--build-dir DIR] [--map FILE]
#
# Every piece of build state is keyed by a phase directory name, so a tree
# built under the numbered phase names shows the orchestrator no snapshot at
# all: the next build offers only a fresh start, and 10_bootstrap re-runs
# every step because its marks are not where it looks. This renames that state
# in place:
#
#   build/snapshots/<dir>/     renamed; manifest.json "phase_dir", "phase" and
#                              "title" rewritten; the `mark` archive of the
#                              two host phases has its directories renamed
#   build/snapshots/timings.json
#                              step and phase keys remapped, per port for a
#                              split phase
#   build/logs/<dir>/          renamed
#   build/mark/<short>/        renamed
#   build/.devplan.json        phase names and step file names remapped
#
# A SPLIT PHASE'S SNAPSHOT AND LOGS GO TO ITS LAST SUCCESSOR: the old phase-3
# tree is the state after 31_compilers, the old phase-4 tree the state after
# 44_apps. The phases before it have no snapshot until a build passes them.
# Its timings are split per port by the successors' own lists under
# script/phases/, the lists the next build installs from, so ETAs survive; a
# port no successor names goes to the last successor. --map (name<TAB>phase)
# overrides the lists for the ports it names.
#
# SNAPSHOTS ARE RENAMED, NEVER COPIED: a full set does not fit twice.
#
# IT REFUSES rather than guesses: while a build or a restore is running, and
# when a state directory exists under both its old and its new name. Run twice,
# the second run finds nothing old and changes nothing. A tree a container
# build left root-owned is migrated as root from a container, started with
# --pid=host so the running-build check can see the build's processes:
#
#   docker run --rm --pid=host -v "$PWD":/workspace -w /workspace os-dev \
#       script/migrate-phase-names.sh

set -e
cd "$(dirname "$(readlink -f "$0")")/.."
exec python3 - "$@" <<'PY'
import argparse, json, os, re, shutil, subprocess, sys, tarfile

RENAME = {
    "00_toolchain": ["00_cross"],
    "01_phase1": ["10_bootstrap"],
    "02_phase2": ["20_selfhost"],
    "03_phase3": ["30_foundation", "31_compilers"],
    "04_phase4": ["40_lang", "41_system", "42_graphics", "43_toolkits", "44_apps"],
    "05_desktop": ["50_desktop"],
    "05_phase5": ["60_kernel"],
    "06_packaging": ["70_image"],
}
MARKS = {"toolchain": "cross", "phase1": "bootstrap"}
STEP_FILES = {
    "01_phase1": {
        "00_file_system.sh": "000_file_system.sh",
        "01_linux_headers.sh": "010_linux_headers.sh",
        "02_musl_libc.sh": "020_musl_libc.sh",
        "03_libstdc++.sh": "030_libstdc++.sh",
        "04_ncurses.sh": "040_ncurses.sh",
        "05_xz.sh": "050_xz.sh",
        "06_gzip.sh": "060_gzip.sh",
        "06_tar.sh": "061_tar.sh",
        "06_toybox.sh": "062_toybox.sh",
        "07_readline.sh": "070_readline.sh",
        "08_bash.sh": "080_bash.sh",
        "09_binutils.sh": "090_binutils.sh",
        "10_gcc.sh": "100_gcc.sh",
        "11_make.sh": "110_make.sh",
        "12_kpkg.sh": "120_kpkg.sh",
        "13_kinstall.sh": "130_kinstall.sh",
    },
    "06_packaging": {
        "00_binhost.sh": "010_binhost.sh",
        "00_cleanup.sh": "020_cleanup.sh",
        "00_launchers.sh": "030_launchers.sh",
        "00_orphans.sh": "040_orphans.sh",
        "00_theme.sh": "050_theme.sh",
        "00_udev_hwdb.sh": "060_udev_hwdb.sh",
        "00_user.sh": "070_user.sh",
        "00_whatis.sh": "080_whatis.sh",
        "01_initramfs.sh": "090_initramfs.sh",
        "01_packs.sh": "100_packs.sh",
        "02_iso.sh": "110_iso.sh",
    },
}
ap = argparse.ArgumentParser(prog="script/migrate-phase-names.sh")
ap.add_argument("--dry-run", action="store_true", help="say what would change, change nothing")
ap.add_argument("--build-dir", default="build")
ap.add_argument("--map", help="name<TAB>phase for ports of a split phase, over the lists")
args = ap.parse_args()

BUILD = os.path.abspath(args.build_dir)
SNAP = os.path.join(BUILD, "snapshots")
LOGS = os.path.join(BUILD, "logs")
MARK = os.path.join(BUILD, "mark")
DRY = args.dry_run


def die(msg):
    print(f"migrate: {msg}", file=sys.stderr)
    sys.exit(1)


def say(msg):
    print(("would " if DRY else "") + msg)


def short(d):
    return d.split("_", 1)[1]


def title_of(d):
    try:
        with open(f"script/phases/{d}/phase.env") as f:
            for line in f:
                m = re.match(r'\s*export\s+KDOS_PHASE_TITLE="(.*)"\s*$', line)
                if m:
                    return m.group(1)
    except OSError:
        pass
    return short(d).replace("_", " ")


def keep_owner(src_stat, path):
    """A rewritten file keeps the owner and mode of the one it replaces, so
    a root-owned tree migrated as root stays root's."""
    os.chmod(path, src_stat.st_mode & 0o7777)
    if os.geteuid() == 0:
        os.chown(path, src_stat.st_uid, src_stat.st_gid)


def write_json(path, obj, indent):
    st = os.stat(path)
    tmp = path + ".migrate"
    with open(tmp, "w") as f:
        json.dump(obj, f, indent=indent)
    keep_owner(st, tmp)
    os.replace(tmp, path)


# ── refusals ─────────────────────────────────────────────────────────────

if not os.path.isdir(BUILD):
    die(f"no build directory at {BUILD}")

# By comm, not by command line: a pattern over command lines matches the
# search itself.
for pid in os.listdir("/proc"):
    if not pid.isdigit():
        continue
    try:
        with open(f"/proc/{pid}/comm") as f:
            comm = f.read().strip()
    except OSError:
        continue
    if comm in (".kdosbuild", "kdosbuild", "kdosbuild.sh"):
        die(f"a build is running (pid {pid}, {comm}); migrate after it exits")

with open("/proc/self/mounts") as f:
    fsroot = os.path.join(BUILD, "fs")
    for line in f:
        mp = line.split()[1]
        if mp == fsroot or mp.startswith(fsroot + "/"):
            die(f"{mp} is mounted: a chroot is live in this tree")

if os.path.exists(os.path.join(BUILD, ".restore-in-progress")):
    die("a snapshot restore did not finish; complete or clear it first")

conflicts = []
for old, new in RENAME.items():
    for root in (SNAP, LOGS):
        if os.path.isdir(os.path.join(root, old)):
            for n in new:
                if os.path.exists(os.path.join(root, n)):
                    conflicts.append(f"{root}/{old} and {root}/{n}")
for old, new in MARKS.items():
    if os.path.isdir(os.path.join(MARK, old)) and os.path.exists(os.path.join(MARK, new)):
        conflicts.append(f"{MARK}/{old} and {MARK}/{new}")


def timing_olds(steps, phases):
    return ({k.split("/", 1)[0] for k in steps if k.split("/", 1)[0] in RENAME}
            | {k for k in phases if k in RENAME})


tpath = os.path.join(SNAP, "timings.json")
if os.path.isfile(tpath):
    with open(tpath) as f:
        tm = json.load(f)
    olds_present = timing_olds(tm.get("steps", {}), tm.get("phases", {}))
    new_all = {n for o in olds_present for n in RENAME[o]}
    for k in list(tm.get("phases", {})) + [k.split("/", 1)[0] for k in tm.get("steps", {})]:
        if k in new_all:
            conflicts.append(f"{tpath}: {k} beside {', '.join(sorted(olds_present))}")
            break

if conflicts:
    die("both an old and a new name exist — which one is current is not "
        "something to guess:\n  " + "\n  ".join(conflicts))

if not DRY:
    for root in (SNAP, LOGS, MARK):
        if os.path.isdir(root) and not os.access(root, os.W_OK):
            die(f"{root} is not writable; run as root from a container")

# ── the per-port map for the split phases ────────────────────────────────

def list_files(d):
    base = f"script/phases/{d}"
    if os.path.isfile(f"{base}/packages.txt"):
        return [f"{base}/packages.txt"]
    pd = f"{base}/packages.d"
    if os.path.isdir(pd):
        return [os.path.join(pd, x) for x in sorted(os.listdir(pd)) if x.endswith(".txt")]
    return []


def split_map():
    m = {}
    for old, succ in RENAME.items():
        if len(succ) < 2:
            continue
        for d in succ:
            for path in list_files(d):
                with open(path) as f:
                    for line in f:
                        name = line.split("#", 1)[0].strip()
                        if name:
                            m.setdefault((old, name), d)
    if args.map:
        with open(args.map) as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) < 2:
                    continue
                for old, succ in RENAME.items():
                    if len(succ) > 1 and parts[1] in succ:
                        m[(old, parts[0])] = parts[1]
    return m


SPLIT = None


def new_phase_for(old, port):
    global SPLIT
    succ = RENAME[old]
    if len(succ) == 1:
        return succ[0]
    if SPLIT is None:
        SPLIT = split_map()
    return SPLIT.get((old, port), succ[-1])


changed = 0

# ── snapshots ────────────────────────────────────────────────────────────

def rewrite_mark_archive(snapdir, man):
    """Rename mark/<old> to mark/<new> inside the snapshot's `mark` archive,
    member by member, so ownership, modes and xattrs pass through untouched
    and no extraction as root is needed."""
    entry = next((e for e in man.get("entries", []) if e.get("path") == "mark"), None)
    if not entry:
        return False
    arch = os.path.join(snapdir, entry["archive"])
    codec = man.get("codec", "none")
    dec = {"zstd": ["zstd", "-dc"], "gzip": ["gzip", "-dc"]}.get(codec, ["cat"])
    enc = {"zstd": ["zstd", "-3", "-T0", "-q", "-c", "-"], "gzip": ["gzip", "-1", "-c"]}.get(codec, ["cat"])

    def renamed(name):
        parts = name.split("/")
        if len(parts) >= 2 and parts[0] == "mark" and parts[1] in MARKS:
            parts[1] = MARKS[parts[1]]
        return "/".join(parts)

    with open(arch, "rb") as src:
        p = subprocess.Popen(dec, stdin=src, stdout=subprocess.PIPE)
        with tarfile.open(fileobj=p.stdout, mode="r|") as t:
            names = [m.name for m in t]
        p.wait()
    if not any(renamed(n) != n for n in names):
        return False
    say(f"rename mark/{{{','.join(MARKS)}}} inside {arch}")
    if DRY:
        return True

    st = os.stat(arch)
    tmp = arch + ".migrate"
    with open(arch, "rb") as src, open(tmp, "wb") as dst:
        d = subprocess.Popen(dec, stdin=src, stdout=subprocess.PIPE)
        e = subprocess.Popen(enc, stdin=subprocess.PIPE, stdout=dst)
        with tarfile.open(fileobj=d.stdout, mode="r|") as tin, \
             tarfile.open(fileobj=e.stdin, mode="w|", format=tarfile.PAX_FORMAT) as tout:
            for m in tin:
                data = tin.extractfile(m) if m.isreg() else None
                m.name = renamed(m.name)
                if m.islnk():
                    m.linkname = renamed(m.linkname)
                tout.addfile(m, data)
        e.stdin.close()
        if d.wait() or e.wait():
            os.unlink(tmp)
            die(f"re-archiving {arch} failed; the original is untouched")
    keep_owner(st, tmp)
    os.replace(tmp, arch)
    entry["bytes_compressed"] = os.path.getsize(arch)
    return True


if os.path.isdir(SNAP):
    for old, new in RENAME.items():
        src = os.path.join(SNAP, old)
        if os.path.isdir(src):
            say(f"rename {src} -> {new[-1]}")
            if not DRY:
                os.rename(src, os.path.join(SNAP, new[-1]))
            changed += 1

    # Every snapshot under a new name whose manifest still carries an old
    # one — including one an interrupted run renamed and did not rewrite.
    olds = {n: o for o, ns in RENAME.items() for n in ns}
    for d in sorted(os.listdir(SNAP)):
        mpath = os.path.join(SNAP, d, "manifest.json")
        # A dry run renamed nothing, so it reads each old directory as the
        # new one it would have become.
        d_new = RENAME[d][-1] if DRY and d in RENAME else d
        if d_new not in olds or not os.path.isfile(mpath):
            continue
        with open(mpath) as f:
            man = json.load(f)
        dirty = False
        if man.get("phase_dir") != d_new:
            say(f"rewrite {mpath}: phase_dir {man.get('phase_dir')} -> {d_new}")
            man["phase_dir"] = d_new
            man["phase"] = short(d_new)
            man["title"] = title_of(d_new)
            dirty = True
        if d_new in ("00_cross", "10_bootstrap"):
            dirty = rewrite_mark_archive(os.path.dirname(mpath), man) or dirty
        if dirty:
            changed += 1
            if not DRY:
                write_json(mpath, man, 2)

# ── timings ──────────────────────────────────────────────────────────────

if os.path.isfile(tpath):
    with open(tpath) as f:
        tm = json.load(f)
    steps = tm.get("steps", {})
    phases = tm.get("phases", {})
    olds_present = timing_olds(steps, phases)
    if olds_present:
        nsteps = {}
        per_new = {}
        for k, v in steps.items():
            ph, _, title = k.partition("/")
            if ph in RENAME:
                ph_new = new_phase_for(ph, title)
                per_new.setdefault(ph, {}).setdefault(ph_new, 0.0)
                per_new[ph][ph_new] += v
                ph = ph_new
            nsteps[f"{ph}/{title}"] = v

        # A split phase's measured total is shared out in proportion to the
        # step time each successor inherited.
        nphases = {}
        for k, v in phases.items():
            if k not in RENAME:
                nphases[k] = v
                continue
            share = per_new.get(k, {})
            whole = sum(share.values())
            if len(RENAME[k]) == 1 or whole <= 0:
                nphases[RENAME[k][-1]] = v
            else:
                for n in RENAME[k]:
                    if share.get(n):
                        nphases[n] = round(v * share[n] / whole, 2)

        say(f"remap {tpath}: {len(steps)} step keys, phases "
            f"{', '.join(sorted(olds_present))}")
        tm["steps"], tm["phases"] = nsteps, nphases
        changed += 1
        if not DRY:
            write_json(tpath, tm, 1)

# ── logs and marks ───────────────────────────────────────────────────────

for old, new in RENAME.items():
    src = os.path.join(LOGS, old)
    if os.path.isdir(src):
        say(f"rename {src} -> {new[-1]}")
        if not DRY:
            os.rename(src, os.path.join(LOGS, new[-1]))
        changed += 1
for old, new in MARKS.items():
    src = os.path.join(MARK, old)
    if os.path.isdir(src):
        say(f"rename {src} -> {new}")
        if not DRY:
            os.rename(src, os.path.join(MARK, new))
        changed += 1

# ── the saved dev plan ───────────────────────────────────────────────────

ppath = os.path.join(BUILD, ".devplan.json")
if os.path.isfile(ppath):
    try:
        with open(ppath) as f:
            plan = json.load(f)
    except ValueError:
        plan = None
    if isinstance(plan, dict):
        def phase_tokens(tok):
            for old, new in RENAME.items():
                if tok in (old, short(old)):
                    return new
            return [tok]
        dirty = False
        if isinstance(plan.get("phases"), list):
            np_ = []
            for t in plan["phases"]:
                for n in phase_tokens(t):
                    if n not in np_:
                        np_.append(n)
            dirty |= np_ != plan["phases"]
            plan["phases"] = np_
        if isinstance(plan.get("steps"), dict):
            ns = {}
            for k, v in plan["steps"].items():
                ks = phase_tokens(k)
                old = next((o for o in RENAME if k in (o, short(o))), None)
                files = [STEP_FILES.get(old, {}).get(s, s) for s in v]
                # A step list belongs to one phase; a split phase's steps are
                # package names, and the last successor is where its old
                # state went.
                ns[ks[-1]] = files
            dirty |= ns != plan["steps"]
            plan["steps"] = ns
        if dirty:
            say(f"remap {ppath}")
            changed += 1
            if not DRY:
                write_json(ppath, plan, 2)

print(f"migrate: {changed} change(s){' planned' if DRY else ''}"
      + ("" if changed else " — nothing uses an old phase name"))
PY
