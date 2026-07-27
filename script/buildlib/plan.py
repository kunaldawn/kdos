# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

"""Developer build plan: which phases/steps re-run, which ports get rebuilt.

A plan is the "work on the existing build/ tree" counterpart to snapshots. It
never restores anything — it just narrows what the next run executes:

  * phases      — the phase directories to run (others are marked SKIPPED)
  * steps       — for script phases, the individual *.sh files to run
  * rebuild     — ports to force-rebuild even though they are installed
                  (build.py passes `kpkg install -f` for exactly these)

A plan that narrows anything suppresses snapshot writes, because a snapshot
taken from a partially re-run tree would be filed under a phase name it no
longer represents.
"""

import glob
import json
import os

PLAN_FILE = ".devplan.json"


def discover_steps(phase):
    """Script basenames a script-phase would run, in execution order."""
    if os.path.isfile(os.path.join(phase.dir_path, "packages.txt")):
        return []
    return [os.path.basename(p)
            for p in sorted(glob.glob(os.path.join(phase.dir_path, "*.sh")))]


def read_packages(phase):
    """Package names listed in a phase's packages.txt (comments stripped)."""
    path = os.path.join(phase.dir_path, "packages.txt")
    if not os.path.isfile(path):
        return []
    out = []
    try:
        with open(path, "r") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    out.append(line)
    except OSError:
        return []
    return out


def discover_ports(repo_root):
    """Every port name in the repo, so a dependency-only port is selectable too."""
    names = set()
    for repo in ("ports/core", os.path.join("src", "packages")):
        base = os.path.join(repo_root, repo)
        try:
            for entry in os.listdir(base):
                if os.path.isfile(os.path.join(base, entry, "kpkgbuild")):
                    names.add(entry)
        except OSError:
            continue
    return names


def package_index(phases, repo_root):
    """{package: phase dir_name or None} for everything selectable."""
    index = {}
    for name in sorted(discover_ports(repo_root)):
        index[name] = None
    for phase in phases:
        for pkg in read_packages(phase):
            index[pkg] = phase.dir_name
    return index


class BuildPlan:
    """Selection of phases / steps / forced rebuilds for one run."""

    def __init__(self, phases=None, steps=None, rebuild=None):
        # None means "everything"; an empty set means "nothing".
        self.phases = set(phases) if phases is not None else None
        self.steps = {k: set(v) for k, v in (steps or {}).items()}
        self.rebuild = set(rebuild or ())

    # ----- queries --------------------------------------------------------

    def is_custom(self):
        return self.phases is not None or bool(self.steps) or bool(self.rebuild)

    def narrows_execution(self):
        """True when the plan skips work — the reason snapshots are suppressed."""
        return self.phases is not None or bool(self.steps)

    def phase_selected(self, dir_name):
        return self.phases is None or dir_name in self.phases

    def step_selected(self, dir_name, basename):
        chosen = self.steps.get(dir_name)
        return chosen is None or basename in chosen

    def forced(self, package):
        return package in self.rebuild

    def summary(self):
        bits = []
        if self.phases is not None:
            bits.append("phases: %s" % (", ".join(sorted(self.phases)) or "none"))
        for dir_name in sorted(self.steps):
            bits.append("%s steps: %s" % (dir_name, ", ".join(sorted(self.steps[dir_name]))))
        if self.rebuild:
            bits.append("rebuild: %s" % ", ".join(sorted(self.rebuild)))
        return "; ".join(bits)

    # ----- persistence ----------------------------------------------------

    def to_dict(self):
        return {
            "phases": sorted(self.phases) if self.phases is not None else None,
            "steps": {k: sorted(v) for k, v in self.steps.items() if v is not None},
            "rebuild": sorted(self.rebuild),
        }

    def save(self, build_dir):
        try:
            os.makedirs(build_dir, exist_ok=True)
            with open(os.path.join(build_dir, PLAN_FILE), "w") as f:
                json.dump(self.to_dict(), f, indent=2)
        except OSError:
            pass

    @classmethod
    def load(cls, build_dir):
        try:
            with open(os.path.join(build_dir, PLAN_FILE), "r") as f:
                data = json.load(f)
        except (OSError, ValueError):
            return None
        if not isinstance(data, dict):
            return None
        return cls(phases=data.get("phases"), steps=data.get("steps"),
                   rebuild=data.get("rebuild"))

    # ----- construction ---------------------------------------------------

    @classmethod
    def from_cli(cls, phases_arg, steps_arg, rebuild_arg, phases):
        """Build a plan from --phases / --steps / --rebuild.

        Returns (plan, error). Tokens are matched against phase dir names and
        short names so `--phases phase4` and `--phases 04_phase4` both work.
        """
        by_token = {}
        for phase in phases:
            by_token[phase.dir_name] = phase
            by_token[phase.name] = phase

        selected = None
        if phases_arg:
            selected = set()
            for token in _split(phases_arg):
                phase = by_token.get(token)
                if phase is None:
                    return None, "unknown phase: %s" % token
                selected.add(phase.dir_name)

        steps = {}
        if steps_arg:
            for token in _split(steps_arg):
                if ":" not in token:
                    return None, "--steps wants PHASE:script.sh, got %r" % token
                phase_token, script = token.split(":", 1)
                phase = by_token.get(phase_token)
                if phase is None:
                    return None, "unknown phase: %s" % phase_token
                known = discover_steps(phase)
                if known and script not in known:
                    return None, "%s has no step %r (has: %s)" % (
                        phase.dir_name, script, ", ".join(known))
                steps.setdefault(phase.dir_name, set()).add(script)
                if selected is not None:
                    selected.add(phase.dir_name)
            # Naming steps of a phase implies running that phase.
            if selected is None:
                selected = set(steps)

        return cls(phases=selected, steps=steps, rebuild=_split(rebuild_arg)), None


def _split(value):
    if not value:
        return []
    return [tok for tok in (v.strip() for v in value.replace(" ", ",").split(","))
            if tok]
