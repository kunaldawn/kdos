# kdos-appbox and kdos-box

One binary under two names, plus a shim per installed application. `kdos-appbox` launches
containerised software and generates its launchers; `kdos-box` manages boxes as first-class
objects. This page covers both, and the launch path in the order it actually runs.

For the format and the machinery underneath, see
[Packs and boxes](../03-architecture/packs-and-boxes.md).

## The commands

```
kdos-appbox run <app> [args...]      run an application from its pack's box
kdos-appbox open [--print|--choose] <path>
kdos-appbox warmup | status
kdos-appbox list | apps
kdos-appbox genlaunchers --packs <fs-root> | --packs --user
```

```
kdos-box list create enter run apps export unexport freeze import clone
         snapshot snapshots rollback start stop restart remove profile gc
```

**Invoked through a symlink named after an application**, it dispatches on its own name — so
`gimp photo.png` works from a terminal with no shell wrapper, using exactly the same dispatch a
multi-call binary does. That keeps the application path free of any shell.

## Two rules the whole program is written under

**The launch path is exact, ordering included.** Every launcher on the system and the login warmup
depend on its behaviour: the stuck-state recovery, the readiness wait that only runs when somebody
*else* started the box, the fire-and-forget notification, the one-time storage-driver choice. Each
one is a fix for something that broke.

**There is no shell anywhere in the program.** Application names, package names and file arguments
all arrive from desktop entries and from the command line; a shell in the middle turns any of them
into an injection point. Everything is executed through an argument-vector builder.

It links four of our libraries and nothing else.

## The launch path

In order, because the order is the design:

1. **Resolve the pack from the command.** `run <exec>` is the form every desktop entry uses, and
   the pack is looked up by **matching the command column whole and then by basename**. Without
   this, only the shim path found a pack and the Start menu launched every application into a
   default box the pack system never had — while the same application worked from a prompt.
2. **Choose the storage driver, once.** See [Storage drivers](#storage-drivers).
3. **Compose the pack stack**, if it is not composed. This is idempotent, and it is required
   because the overlay lives on a temporary filesystem — a box created before a reboot has a root
   directory the reboot deleted.
4. **Recover a stuck box.** A stopped box is often still *stopping*: stopping sends a termination
   signal and the container's init stays alive reaping, so a stop followed promptly by a start asks
   the engine to start a container it refuses, reporting an improper state and naming nothing a
   reader can act on. A hung application holding uninterruptible I/O wedges a box there for good.
   The recovery is to wait it out, then kill, then remove — and the container is **recreated over
   the same stack**, because a box is stateless: its packs are read-only and its writable layer is
   on disk. This recovery is **shared** with the box manager's own start path.
5. **Wait for readiness — but only when somebody else started the box**, and only until the
   container's init announces itself. A blind wait on the warmup lock made a click wait for the
   entire container initialisation: up to two minutes of dead-looking desktop.
6. **Build the environment.** See [The environment](#the-environment-a-box-gets).
7. **Execute**, with a terminal attached when this process's own input is a terminal — so the same
   command gives an interactive prompt at a shell and a plain execution from a launcher.
8. **Notify**, fire-and-forget and backgrounded. The notification tool's default reply timeout is
   long, and a notification must never gate a launch.

Stage timings are appended to `$XDG_RUNTIME_DIR/kdos-appbox.trace`. Measured on the reference
machine: **18.3 s** cold with no container at all, **0.3 s** warm, **0.55 s** for a second window.

## The environment a box gets

Executing inside a container inherits **nothing** — not the container's init environment, not the
caller's — so every variable is stated explicitly. The list and what each prevents is in
[The session](../03-architecture/session.md#the-environment-a-box-receives).

Three that belong here:

- **The search path includes the games directory**, which Debian uses and an inherited host path
  lacks — without it every game launcher dies reporting the program is not found.
- **The display variable is pushed in explicitly** for X11-only applications: the compositor
  exports it only to what *it* spawned, so this probes for the X socket and adds it.
- **The accessibility variables are a default, not a policy**, and are opted out of with a file in
  the configuration directory or a variable for one launch. Resolved from the home directory like
  the box profiles, for the same reason: two programs resolving it differently is the failure this
  must not have.

**Qt theming asks the pack, not the image.** A pack box has no image to inspect, so an
image-label lookup answers no to every question and exports an inert value that leaves every boxed
Qt application grey. The runtime that installs a platform theme **declares the variable in its own
metadata**, the environment walk collects them along the requirement chain, and the **nearest pack
wins** so an application can override its runtime. Same cannot-drift property as a label, stated
where the packages are.

## genlaunchers

Walks every **installed** pack, mounts it through the pack daemon, and parses the application's
own desktop entries — a pack carries the real entries, so the existing parse is reused rather than
reimplemented against the metadata.

**It reads installed packs only.** The daemon's list carries every pack on the medium as
available, and mounting one to read its entries is what makes it *installed* — so an unfiltered
pass turns the whole catalogue into mounted packs.

### Four outputs, and dropping any one breaks something visible

| Output | Without it |
|---|---|
| A desktop entry per application | No launcher |
| A MIME cache beside them | The type associations are never consulted |
| A name-to-command table | The shim cannot find what to run |
| A shim per application | The application is not a command |

The MIME cache is written here rather than by the usual tool because the host has no desktop-file
utilities.

### Two trees, and whose decision each is

| Tree | Written by | Holds |
|---|---|---|
| System | The **build**, for the recommended set | Entries, the table, and shims for everyone |
| User | `genlaunchers --packs --user`, as you | The same, for what you installed |

**Installing an application runs the user pass as its last act**, so an installed application is
in the Start menu before the command returns, with no root anywhere. Before that, installing
mounted the pack and stopped — and the menu's own install row led to an application nobody could
launch.

**An extra argument is refused.** The two forms differ by one, so passing both a directory and a
root would read the directory as the root and write the whole set underneath it: a table nothing
reads, no shims swept, and a successful exit.

Regenerating needs the packs mounted, so it runs on the target as **root** when writing the system
tree, and a run as anyone else fails rather than reporting a launcher set it did not write.

### Naming rules

- **The launcher filename is upstream's own desktop identifier**, not a KDOS-prefixed name and not
  the window-class field. A dock matches a running window to an entry by the entry's **file
  identifier**, so a mismatch shows a second generic icon beside the pinned one.
- **A window identifier is not the X11 window class.** One catalogue application's entry declares
  a versioned class while its window presents an unversioned identifier — measured with protocol
  tracing, not guessed. So pinned favourites reference upstream identifiers. The class field is
  still written, since it costs nothing and is what an X11 application under Xwayland matches by.
- **A shim is named after the program its entry runs**, through the single definition of "which
  program a command line runs" — which skips an environment prefix and reads inside a shell
  wrapper. So a reverse-DNS entry gets a shim named after the actual program. A rename table wins
  where upstream's program name is not the one people know, and a reserved or odd name falls back
  to the lowercased identifier.
- **A terminal entry stays one.** The generated launcher carries upstream's terminal flag, and the
  shell wraps such an entry in a terminal. Written as false, one catalogue application was started
  with a pipe for input and exited on a usage error — from the Start menu, with no window and no
  sentence anywhere.

### The tables

| Table | Does |
|---|---|
| `COMMANDS` | Packs whose value is a **command**, not an application: a table row and a shim, deliberately **no** desktop entry, because a launcher for a shell tool with no arguments opens nothing. Emitted only when the image really carries the binary |
| `RENAME` | Upstream's program name is not the one people know |
| `RESERVED` | Names the sweep must not delete — see below |
| `EXEC_EXTRA` | Arguments an application needs **only because it is containerised**: one sandboxing toolkit wants a privileged helper it cannot have and exits rather than falling back |
| `SKIP_NEEDS_KWIN` | Applications that ask a specific compositor's private interface and open an error dialog on any other |
| `SKIP_ROOTLESS_INERT` | Applications needing raw block devices, which a rootless container cannot give them. A launcher that opens onto "permission denied" teaches somebody the machine is broken rather than that they wanted the host tool |
| `SKIP_PREFIXES`, `SKIP_BASENAMES` | Entries that are not applications |
| `X11_FORCING` | Environment prefixes forcing X11, stripped — those applications run fine on Wayland, and forcing X11 kills them under a compositor whose X server they cannot reach |

**The sweep spares `RESERVED`.** Every shim is removed before the set is rewritten, and the marker
for one this program wrote is a **relative symlink** — on the reasoning that hand-written entries
there are real files. The box manager's own name is not a real file: it is this same binary under
a relative link, so a sweep going by the marker alone deleted the front door to every box on the
machine.

**The table grows and has no ceiling.** A fixed table dropped the tail — a warning per
application, a successful exit, and a Start menu missing whatever sorted last. It is on the heap,
so the dispatcher carries no fixed cost for a table only the generator fills.

### Exec lines

**An `Exec=` line is not a whitespace-separated list**, and treating it as one is a whole class of
application that appears not to start. It carries the format's **quoting** — a quoted absolute
path whose quotes become part of the path, a shell wrapper whose single argument gets handed over
in pieces — and it carries **field codes**, which must vanish when nothing was selected, or a media
player tries to open a file literally named after the code.

One function is the single implementation: it unquotes, substitutes the single-file and
multiple-file codes, drops the codes that carry no argument, and — with a negative count — keeps
every code verbatim for a tool that **rewrites** a line rather than running one. Its inverse
re-quotes, so the generator's output round-trips.

**Every launch path goes through it**, and the test suite asserts both directions against real
shapes taken from the catalogue.

## The open path

```sh
kdos-appbox open report.pdf
kdos-appbox open --print report.pdf     # resolve and print, do not run
kdos-appbox open --choose report.pdf
```

This is here rather than in a generic opener because this is already the program that knows what
"open with GIMP" means on this machine.

The resolution is the standard one and nothing clever: the glob table for the type — **longest
matching suffix wins**, or every compound extension opens in a decompressor — then the default
applications, the added associations, and each MIME cache. That last file is the one the generator
already writes beside a box's launchers, so **a boxed application is found by exactly the same
lookup as a host one**.

**Each of those levels is searched twice, the running desktop's list first.**
`<desktop>-mimeapps.list` — the first name in `XDG_CURRENT_DESKTOP`, lowercased, so
`kdos-console-mimeapps.list` on the console — comes before the plain `mimeapps.list` beside it.
That is what lets one picture open in `timg` inside a terminal on the console and in a boxed viewer
under the compositor **without either desktop editing the other's choices**, which one list for one
user cannot express.

**Every shipped table is at `/etc/xdg` and a home starts with none.** Three files:
`kdos-mimeapps.list` for the compositor, `kdos-console-mimeapps.list` for the console, and the
plain `mimeapps.list` for what both desktops answer the same way. A person's own choice is searched
before all three wherever they made it, so *Open With* can always change what is in force — a
default shipped into `~/.config` would have outranked it and the chooser would have appeared to do
nothing. *Open With* consults these in the opener's order and writes to the plain user list, so
what it shows as current is what the opener would actually run.

**A `Terminal=true` entry is wrapped in the desktop's own terminal.** `foot` under the compositor,
`kdos-term` inside a console session — one rule in `kb_terminal()`, because the console resolving a
handler correctly and then wrapping it in a Wayland client would look like the handler being wrong
rather than the terminal being unreachable.

**A type goes in exactly one of them.** One both desktops open the same way belongs in the plain
file; writing it in each desktop's is the same decision recorded twice, which is a decision that
drifts.

Field codes are **substituted** rather than stripped: the code *is* the file, and dropping it
opens the application with an empty document.

**The shared MIME database has to be compiled on the target.** The port ships the source
definitions and nothing else — no glob table, no cache — so every consumer asking what type a file
is got no answer at all. An install hook compiles it, which is something only an install-time hook
can do because the compiler is a target binary.

## Box profiles

`~/.config/kdos/boxes/<name>.conf`, flat `key = value`.

**An application box and a development box differ in three keys, not in kind** — the base, whether
it persists, and whether its applications are exported. That is what makes one manager over two
lanes honest rather than a wrapper over two systems.

| Key | Maps onto |
|---|---|
| `base` | `pack:<id>`, `box:<name>`, or `image:<ref>` |
| `persistence` | Whether the writable layer survives |
| `export` | Whether its applications get host launchers |
| `network`, `ipc` | Namespace flags — **create-time** |
| `devices` | Whether `/dev` and the runtime directory are shared |
| `gpu`, `audio` | Ride on `devices`; see below |
| `memory` | Enforced by the **memory daemon**, not the container engine |
| `accent` | The box's colour, which is what draws a title-bar chip |
| `autostop` | Idle timeout for the collector |
| `grant` | Compositor globals the sandbox allowlist otherwise refuses |
| `image` | The reference, for a registry base |

Three properties this list is written to keep:

- **Every key maps 1:1 onto a container-engine flag or onto something KDOS enforces itself**, and
  the profile printer names the mechanism behind each line.
- **It says out loud what it could not enforce.** `gpu` and `audio` ride on `devices`, and there
  is **no flag that grants a box a speaker and denies it a camera**.
- **An unknown key is reported by name.**

**`memory` is enforced by the memory daemon**, and that is what makes the key honest: rootless
containers on a machine with no cgroup delegation accept a memory limit and ignore it. So the
daemon reads the profiles and **prefers a box that is over its own declared budget** as a victim,
ahead of the general rule that boxed processes are preferred.

**A namespace key applies at create time.** It cannot be re-flagged on a live container, so
changing one **says to recreate the box** rather than silently doing nothing.

**`grant = screencopy, data-control` opens a global the sandbox allowlist refuses.** The
compositor consults the box's profile once per client and caches on the identifier; a reload drops
the cache. The names map onto **both generations** of each protocol, and the input-method grant has
to be spelled out because it is a keylogger by design.

**`base = image:<ref>` is an online operation and says so before doing anything.** It fetches
unsigned content from somebody else's registry; the strict-signature setting does not cover it,
and pretending otherwise would be dishonest. `pack:` and `box:` are the offline kinds.

A profile with no base reads as unset in the listing until the first launch records the pack it
used; creating a box with none is refused.

## Freeze, import and clone

**`freeze` is the flagship.** It packs the box's **writable layer** into one image through the
pack builder, with the base chain recorded as requirements — so the artefact is the **difference**,
and it diffs against a previous freeze like any other pack.

Measured on a development box with real work in it: **2.1 MB against a 432 MB merged root** —
about 195 times smaller, and about 98 times smaller than the base pack it sits on.

**There is no alternative to compare it against.** A pack box is created over an exploded root, so
the container engine **refuses to commit it** outright — there is no image to save. Freeze is the
only way to capture a pack box's state.

`import` stages the result and asks the daemon to install it, because verification happens where
the mount happens.

## Snapshots and rollback

**A snapshot is a copy of the writable layer, and the cost is stated** — on an ordinary filesystem
that is everything the box has written.

It is deliberately **not** a pack, because a pack cannot be written back into a writable layer
without being mounted, and a rollback that needed the daemon would fail exactly when a box is
broken.

## Export

**A secondary box's application gets a box-qualified desktop identifier and a box-qualified
shim**, while the **default** box keeps upstream's own identifier so nothing that works today
changes.

That is a deliberate refinement of the launcher-naming rule: the rule exists because a dock matches
a window to an entry by file identifier, and the panel now has a better key than the filename.

## Warmup and collection

**Warmup is the pinned set.** One box per application means a single login warmup covers nothing,
so the warmup reads the favourites file — which holds desktop identifiers — resolves each through
its entry's command to the shim the table is keyed by, and composes and starts that pack's box at
low priority.

**The collector runs every ten minutes from the session**, and **asks the compositor first**: a box
with a mapped window is not idle whatever its clock says, and the command socket is the one question
that answers it. It existed and nothing called it, so a warmed box was a leak.

## Storage drivers

Two, and the choice must never flip.

| Situation | Driver |
|---|---|
| A live session | The userspace overlay implementation |
| An installed system on an ordinary filesystem | The kernel's native overlay |

**A live session needs the userspace one**: the home directory sits on the boot overlay and the
kernel refuses to stack an overlay upper layer on an overlay. The engine does **not** fall back —
the container simply fails to mount.

On an installed system the kernel's implementation is much faster, so a one-time per-user
configuration is written when the home directory's filesystem supports it — **and only while the
store has no containers yet**, because the two write incompatible deletion markers into container
layers.

## See also

- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the format, the daemon and the container
- [Applications](../02-user-guide/applications.md) — using all of this
- [The daemons](daemons.md) — the pack daemon and the memory daemon
- [The session](../03-architecture/session.md) — the environment and what is shared
- [The security model](../03-architecture/security-model.md) — what a box is and is not
