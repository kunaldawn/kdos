# kdos-appbox, kdos-box and xdg-open

One binary answers to three names, plus one shim per installed application. `kdos-appbox` launches
containerised software and generates its launchers. `kdos-box` manages boxes as first-class
objects. `xdg-open` is the name every link-opener says, and it answers ahead of xdg-utils' script
because `/usr/local/bin` comes first on `PATH`.

This page covers all three, and the launch path in the order it runs. For the pack format and the
machinery underneath, see [Packs and boxes](../03-architecture/packs-and-boxes.md).

## Synopsis

```
kdos-appbox [-b BOX] run <app> [args...]
kdos-appbox open [--print | --choose] <path>...
kdos-appbox catalogue [--groups]
kdos-appbox install|uninstall <id|group>... [--dry-run]
kdos-appbox export <file.ktar> <id|group>...
kdos-appbox import <file.ktar> [<id>...]
kdos-appbox list | apps | warmup | status
kdos-appbox genlaunchers --packs <fs-root>
kdos-appbox genlaunchers --packs --user
kdos-appbox genlaunchers --packs-dir <dir> <fs-root>
kdos-appbox genlaunchers <desktop-dir> <fs-root>

kdos-box list | create | enter | run | apps | export | unexport | freeze
        | import | clone | snapshot | snapshots | rollback | start | stop
        | restart | remove | profile | gc

xdg-open <path|uri>
```

Global options: `-b`, `--box <name>` runs in a named box instead of the application's own pack box,
and is parsed before the verb. `-v`, `--verbose` lets the container engine print to standard error.

## Description

Two rules run through the whole program.

The launch path is exact, ordering included. Every launcher on the system and the login warmup
depend on its behaviour — the stuck-state recovery, the readiness wait that only runs when
something else started the box, the fire-and-forget notification, the one-time storage-driver
choice. Reordering any of them changes what a click does.

There is no shell anywhere in the program. Application names, package names and file arguments all
arrive from desktop entries and from command lines, and a shell in the middle turns any of them
into an injection point. Everything is executed through an argument-vector builder. The program
links four KDOS libraries — `libkbase`, `libktui`, `libkcolor` and `libkxdg` — and nothing else.

Invoked through a symlink named after an application, the binary dispatches on its own name, so
`gimp photo.png` works from a terminal with no shell wrapper. That is the same dispatch a
multi-call binary does, and it keeps the application path free of any shell.

## Commands

### run

```sh
kdos-appbox run gimp-3.0 photo.png
kdos-appbox -b scratch run bash
```

Starts an application in its pack's box. See [The launch path](#the-launch-path) for what happens
between the command and the window.

### open

```sh
kdos-appbox open report.pdf
kdos-appbox open --print report.pdf     # resolve and print, do not run
kdos-appbox open --choose report.pdf    # ask which application
```

Resolves a file or URI to the application that handles it. See [The open path](#the-open-path).

### catalogue

```sh
kdos-appbox catalogue
kdos-appbox catalogue --groups
kdos-appbox catalogue --selftest
```

Prints one tab-separated line per `app` and `data` row — `<id> <name> <category> <bytes>
<parent> <installed|available> <tagline>`. Tab, because a tagline contains spaces and every other
table in this tree splits on tab. `--groups` prints `<id> <description> <member> <member> …`
instead.

The install state is reported here and nowhere else. Three surfaces ask what is installed — the
store, `kinstall` and `kdos app` — and three joins against the container engine would be three
chances to disagree about what installed means. A box the engine lists under a catalogue id *is*
that application; nothing else counts, because nothing else could be launched, and a machine with
no container engine reports everything `available`.

`--selftest` runs the parser's own assertions against `$KDOS_CATALOGUE`, offline and with no
daemon. It is checked before anything that needs a daemon or a display, so it stays runnable where
neither exists. See [Testing](../05-developer/testing.md).

The shipped catalogue is `/usr/share/kdos/appstore/catalogue`, and it carries 183 applications and
2 data rows on 2 base rows and 7 runtimes, grouped into 7 named bundles.

### install and uninstall

```sh
kdos-appbox install <id|group>... [--dry-run]
kdos-appbox uninstall <id|group>...
```

A chain is built bottom-up as a stack of images, one per catalogue row, each `FROM` the one below:
`kdos/base`, then `kdos/rt-gtk`, then `kdos/app.gimp`. That is what makes a second GTK application
one apt pass instead of three, because the base and the runtime are already images and are
skipped. Flattening a chain into one image per application would rebuild and re-store every shared
layer per application.

An image already present is never rebuilt. `podman image exists` is the whole check: the catalogue
carries no version per row, so an image is current by definition until somebody removes it.
`--dry-run` tracks the same thing for itself rather than asking the engine, or a preview would
show four builds where an install does two.

Nothing rolls back. Six applications where the fourth fails leaves five installed and names the
fourth. An installed application is not damaged by a later one failing, and unwinding throws away
twenty minutes of apt.

Progress is one flushed line per step, so a surface reading this process's standard output shows it
without parsing the container engine — and the process is a direct child of that surface, never a
supervised one, because a pipeline reading a supervised service's output never returns.

`uninstall` removes the box, then the application's own image, then any runtime above the base that
no remaining box's chain names. The base is never removed: it is every chain's floor. Which
runtimes are still wanted is asked of the catalogue, not of the engine — a dangling-image sweep
cannot tell a runtime nothing uses yet from one whose only application is mid-install.

`snapshot = auto` in the catalogue is resolved against the base image, by reading the date Debian
records in its own sources file, so the packages installed on top cannot disagree with the rootfs
under them. A resolution that fails warns and builds unpinned: refusing to install because a
comment moved would be worse, but an unpinned Containerfile is indistinguishable from a pinned one
once written, and the warning is the only thing that tells them apart.

### export and import

```sh
kdos-appbox export <file.ktar> <id|group>...
kdos-appbox import <file.ktar> [<id>...]
```

A set of applications as signed packs in one file. `export` needs a selection; `import` does not,
because an archive carries its own.

```
apps-2026-09-18.ktar
  SELECTION        the groups and ids this set was exported as
  PACKAGES         id, version, size and payload hash per pack
  PACKAGES.sig     when a key was readable
  app.gimp.kpack
  app.inkscape.kpack
```

Four decisions shape the format:

- **Packs, not a container-engine save.** A store install is unsigned content from somebody else's
  registry; a pack is hashed and signature-checked by `kdos-packd` where it mounts it. That makes
  an imported application verified where a store-installed one is not, and it is the only route to
  software on a machine with no network.
- **`podman export`, not the overlay store.** An image's content is spread across its layers and
  only the export flattens them. It also keeps overlay whiteouts and `trusted.overlay.*` xattrs out
  of play, which is the one thing that would force this to run as root.
- **`kdos-pack build`, not `mkfs.erofs` followed by an assemble.** The build verb already runs
  `mkfs` with the reproducible flag set, and a second copy of that flag list is a second answer to
  how a pack is made. Its `--force-uid=1000` is what lets export run unprivileged: a box runs
  `--userns keep-id`, so the process inside it is uid 1000 and a tree owned by real root is one it
  can create nothing in.
- **An unsigned index says so.** With no `KDOS_PACK_KEY` the set still indexes and still imports,
  and every hash is still checked at the mount, but the export prints that it was not signed. An
  unsigned archive is otherwise indistinguishable from a signed one.

Import hands the daemon a filename inside the daemon's own staging directory, never a path. That is
the rule the daemon is built on, and what keeps something reachable from `wheel` from being
`mount /dev/sda2 /etc`. A pack the daemon refuses is named and skipped; the rest of the archive
still imports.

The `SELECTION` manifest is flat and commentable, so a set can be diffed and hand-edited. A `group`
line records what was picked; the id lines are what is installed, so a group whose membership
changes later still imports the software the archive actually carries.

### list, apps, warmup, status

`list` prints the boxes and their profiles. `apps` prints the alien applications this machine
knows. `warmup` composes and starts the boxes behind the pinned favourites. `status` reports what
is composed and running.

## The launch path

In order, because the order is the design:

1. **Resolve the box.** A generated launcher for an application that belongs to a pack writes
   `Exec=kdos-appbox -b <pack> run <exec>`, so the box is named outright and no lookup happens.
   `<pack>` is the same string the box profile is filed under. A command with no `-b` — a prompt, a
   shim, an entry naming no box — resolves the pack from the command instead, by matching the
   command column whole and then by basename. An exec no installed pack carries is refused by name:
   composing a box out of a name no pack answers to fails a step later, with a sentence about a box
   nobody asked for.
2. **Choose the storage driver, once.** See [Storage drivers](#storage-drivers).
3. **Compose the pack stack**, if it is not composed. This is idempotent, and it is required
   because the overlay lives on a temporary filesystem: a box created before a reboot has a root
   directory the reboot deleted.
4. **Recover a stuck box.** A stopped box is often still *stopping*: stopping sends a termination
   signal and the container's init stays alive reaping, so a stop followed promptly by a start asks
   the engine to start a container it refuses, reporting an improper state and naming nothing a
   reader can act on. A hung application holding uninterruptible I/O wedges a box there for good.
   The recovery is to wait it out, then kill, then remove — and the container is recreated over the
   same stack, because a box is stateless: its packs are read-only and its writable layer is on
   disk. This recovery is shared with the box manager's own start path.
5. **Wait for readiness, but only when something else started the box**, and only until the
   container's init announces itself. A blind wait on the warmup lock makes a click wait for the
   entire container initialisation, which is up to two minutes of dead-looking desktop.
6. **Build the environment.** See [The environment a box gets](#the-environment-a-box-gets).
7. **Execute**, with a terminal attached when this process's own input is a terminal, so the same
   command gives an interactive prompt at a shell and a plain execution from a launcher.
8. **Notify**, fire-and-forget and backgrounded. The notification tool's default reply timeout is
   long, and a notification must never gate a launch.

Stage timings are appended to `$XDG_RUNTIME_DIR/kdos-appbox.trace`. Measured on the reference
machine: 18.3 s cold with no container at all, 0.3 s warm, 0.55 s for a second window.

None of those stages is visible to the desktop. What a person watches for those eighteen seconds is
a launcher with nothing on the screen yet, because the compositor has no window to map until the
client connects and this whole path runs before it does.

## The environment a box gets

Executing inside a container inherits nothing — not the container's init environment, not the
caller's — so every variable is stated explicitly. The full list and what each one prevents is in
[The session](../03-architecture/session.md#the-environment-a-box-receives). Three belong here:

- The search path includes the games directory, which Debian uses and an inherited host path lacks.
  Without it every game launcher dies reporting that the program is not found.
- The display variable is pushed in explicitly for X11-only applications. The compositor exports it
  only to what it spawned, so this probes for the X socket and adds it.
- The accessibility variables are a default rather than a policy, and are opted out of with a file
  in the configuration directory or a variable for one launch. They resolve from the home directory
  the same way the box profiles do, because two programs resolving one path differently is the
  failure this must not have.

Qt theming asks the pack, not the image. A pack box has no image to inspect, so an image-label
lookup answers no to every question and exports an inert value that leaves every boxed Qt
application grey. The runtime that installs a platform theme declares the variable in its own pack
metadata, the environment walk collects those along the requirement chain, and the nearest pack
wins, so an application can override its runtime. That gives the same cannot-drift property a label
has, stated where the packages are.

## Launcher generation

```sh
kdos-appbox genlaunchers --packs <fs-root>      # every installed pack, system tree
kdos-appbox genlaunchers --packs --user         # every installed pack, your tree
kdos-appbox genlaunchers --packs-dir <dir> <fs-root>   # the build's form
kdos-appbox genlaunchers <desktop-dir> <fs-root>
```

`genlaunchers` walks every installed pack, mounts it through the pack daemon, and parses the
application's own desktop entries. A pack carries the real entries, so the existing parse is reused
rather than reimplemented against the metadata.

It reads installed packs only. The daemon's list carries every pack on the medium as available, and
mounting one to read its entries is what makes it installed — so an unfiltered pass turns the whole
catalogue into mounted packs.

And only a pack that carries desktop entries of its own. A pack with no `/usr/share/applications`
has nothing for the parse to read and contributes nothing to the set. That is the case for a pack
whose value is a command: `kdos-appbox -b <pack> run <command>` reaches those, and
`kdos app show <pack>` names them from the metadata.

An extra argument is refused rather than ignored. The `--packs` and two-argument forms differ by
one argument, so passing both a directory and a root reads the directory as the root and writes the
whole set underneath it: a table nothing reads, no shims swept, and a successful exit.

Regenerating needs the packs mounted, so writing the system tree runs on the target as root. A run
as anyone else fails rather than reporting a launcher set it did not write.

### Four outputs

| Output | Without it |
|---|---|
| A desktop entry per application | No launcher |
| A MIME cache beside them | The type associations are never consulted |
| A name-to-command table | The shim cannot find what to run |
| A shim per application | The application is not a command |

The MIME cache is written here rather than by the usual tool, because the host carries no
desktop-file utilities.

### Two trees

| Tree | Written by | Holds |
|---|---|---|
| System | The build, for the recommended set | Entries, the table and shims for everyone |
| User | `genlaunchers --packs --user`, as you | The same, for what you installed |

Installing an application runs the user pass as its last act, so an installed application is in the
Start menu before the command returns, with no root anywhere. Without that step an install mounts
the pack and stops, and the menu's own install row leads to an application nobody can launch.

The system tree is reconciled at packaging time by `script/06_packaging/00_launchers.sh`, which
runs `genlaunchers --packs-dir` over the extraction root. That call is the only thing standing
between a medium that bakes no packs and a Start menu still offering the ones a previous bake left
behind: the generated set is not under `fs/`, so `var/lib/kdos/fs-manifest` does not own it and
nothing else removes it. It runs before `00_user.sh`, which materialises every home with
`cp -r /etc/skel/.` — a skel cleaned after that step leaves the launchers in `/home/kdos`, and the
Start menu reads the home first.

`genlaunchers` reconciles rather than appends, which is why one call is the whole cleanup. It
sweeps every `.desktop` carrying `X-KDOS-Alien=true`, every `/usr/local/bin` symlink it recognises
as its own, and rewrites the table and the MIME cache whole. A second sweep written elsewhere would
be a second answer to which launchers are ours.

A launcher can still outlive its pack, because only the paths that run `genlaunchers` reconcile: a
pack removed through `kdos-packd` rather than through `kdos app remove` leaves a row behind.
`sh_box_missing()` in `kdos-shell` is the runtime half — a row whose box is neither an installed
`.kpack` nor a box profile is dropped from the Start menu and the Open With chooser. Absence has to
be proved: an unnamed box, a name that is not an id, and a machine with no pack store all count as
present, because hiding an application somebody installed is a worse failure than showing one whose
pack has gone.

### Naming rules

- **The launcher carries its box.** An application belonging to a pack gets
  `Exec=kdos-appbox -b <pack> run <exec>`; one with no pack keeps the bare
  `Exec=kdos-appbox run <exec>`. `<pack>` is the pack identifier, which is also the box name and the
  stem of the box profile, so a reader takes a guest's policy key straight off the argument vector
  instead of reversing the box layout out of an absolute `Exec`, and `run` skips the command table
  entirely. A guest launched without `-b` resolves to the program's basename, which names a window
  but no profile.
- **The launcher filename is upstream's own desktop identifier**, not a KDOS-prefixed name and not
  the window-class field. A dock matches a running window to an entry by the entry's file
  identifier, so a mismatch shows a second generic icon beside the pinned one.
- **A window identifier is not the X11 window class.** One catalogue application's entry declares a
  versioned class while its window presents an unversioned identifier, measured with protocol
  tracing rather than guessed. Pinned favourites therefore reference upstream identifiers. The
  class field is still written, since it costs nothing and is what an X11 application under
  Xwayland matches by.
- **A shim is named after the program its entry runs**, through the single definition of which
  program a command line runs — which skips an environment prefix and reads inside a shell wrapper.
  A reverse-DNS entry therefore gets a shim named after the actual program. A rename table wins
  where upstream's program name is not the one people know, and a reserved or odd name falls back
  to the lowercased identifier.
- **A terminal entry stays one.** The generated launcher carries upstream's terminal flag, and the
  shell wraps such an entry in a terminal. Written as false, such an application starts with a pipe
  for input and exits on a usage error — from the Start menu, with no window and no sentence
  anywhere.

### The tables

| Table | Holds |
|---|---|
| `COMMANDS` | Program names whose value is a command rather than an application: a table row and a shim, deliberately no desktop entry, because a launcher for a shell tool with no arguments opens nothing. Emitted only when the source really carries the binary, so a set baked before a segment existed gets no shim that dies on "not found" |
| `ENTRIES` | The opposite case: software that is an application and ships no desktop entry at all, so there is nothing to parse. The name, category, MIME types and `Exec` are written here, because a shipped entry can be replaced by apt and its `Exec` has to name `kdos-appbox run`. `surf` is such a row — a browser with no launcher claims no scheme, so installing it would change nothing about what opens a link |
| `RENAME` | Upstream's program name is not the one people know |
| `RESERVED` | Names the sweep must not delete |
| `EXEC_EXTRA` | Arguments an application needs only because it is containerised: one sandboxing toolkit wants a privileged helper it cannot have and exits rather than falling back |
| `SKIP_NEEDS_KWIN` | Applications that ask a specific compositor's private interface and open an error dialog on any other |
| `SKIP_ROOTLESS_INERT` | Applications needing raw block devices, which a rootless container cannot give them. A launcher that opens onto "permission denied" teaches somebody the machine is broken rather than that they wanted the host tool |
| `SKIP_PREFIXES`, `SKIP_BASENAMES` | Entries that are not applications |
| `X11_FORCING` | Environment prefixes forcing X11, stripped from an `Exec`. Those applications run fine on Wayland and the prefix is upstream's habit rather than a requirement. An application that genuinely is an X11 client says so with an `env` row in the catalogue instead, which travels as pack metadata and is not an `Exec` this table can strip |

The sweep spares `RESERVED`. Every shim is removed before the set is rewritten, and the marker for
one this program wrote is a relative symlink, on the reasoning that hand-written entries there are
real files. The box manager's own name is not a real file — it is this same binary under a relative
link — so a sweep going by the marker alone removes the front door to every box on the machine.

The table grows and has no ceiling. A fixed-size table drops its tail: one warning per application,
a successful exit, and a Start menu missing whatever sorted last. It lives on the heap, so the
dispatcher carries no fixed cost for a table only the generator fills.

### Exec lines

An `Exec=` line is not a whitespace-separated list, and treating it as one is a whole class of
application that appears not to start. It carries the format's quoting — a quoted absolute path
whose quotes would otherwise become part of the path, a shell wrapper whose single argument gets
handed over in pieces — and it carries field codes, which must vanish when nothing was selected, or
a media player tries to open a file literally named after the code.

One function is the single implementation. It unquotes, substitutes the single-file and
multiple-file codes, drops the codes that carry no argument, and — with a negative count — keeps
every code verbatim for a tool that rewrites a line rather than running one. Its inverse re-quotes,
so the generator's output round-trips. Every launch path goes through it, and the test suite
asserts both directions against real shapes taken from the catalogue.

## The open path

`/usr/local/bin/xdg-open` is this binary under a third name. Everything that opens a link says that
word and means "whatever this machine opens it with": a mail client's `:open-link`, a portal,
anything reading `$BROWSER`. `/usr/local/bin` comes first on the shipped `PATH`, so this answers
before xdg-utils' script — which stays installed and is still where an unclaimed type ends up,
reached by absolute path because naming it otherwise would find this binary again and recurse.

A URL is not a file name, and one function decides which it is. `kxdg_mime_for_arg()` types an
argument carrying a scheme as `x-scheme-handler/<scheme>` and unwraps `file:` to the path it names;
anything else is a path. Where the basename decides instead, `mailto:a@b.c` matches the `*.C` glob
and a mail address resolves to C++ source. The chooser asks the same function, so both sides mean
the same thing by the same word.

Resolution is otherwise the standard one: the glob table for the type — longest matching suffix
wins, or every compound extension opens in a decompressor — then the default applications, the
added associations, and each MIME cache. That last file is the one the generator already writes
beside a box's launchers, so a boxed application is found by exactly the same lookup as a host one.

Each of those levels is searched twice, this desktop's list first. `<desktop>-mimeapps.list` — the
first name in `XDG_CURRENT_DESKTOP`, lowercased, so `kdos-mimeapps.list` — comes before the plain
`mimeapps.list` beside it, which is what the specification asks for.

Every shipped table is at `/etc/xdg` and a home starts with none. Two files: `kdos-mimeapps.list`,
which names this desktop, and the plain `mimeapps.list` under it for the types that answer the same
way on a bare virtual terminal. A person's own choice is searched before both wherever they made
it, so Open With can always change what is in force; a default shipped into `~/.config` would
outrank it and the chooser would appear to do nothing. Open With consults these in the opener's
order and writes to the plain user list, so what it shows as current is what the opener would run.

A type goes in exactly one of them. One that answers the same way with no session belongs in the
plain file, and writing it in the desktop's list as well is the same decision recorded twice, which
is a decision that drifts.

### Terminal entries

A `Terminal=true` entry is wrapped in the desktop's own terminal, named once in `kb_terminal()`.

On a bare virtual terminal it is not wrapped at all. `Ctrl+Alt+F2`, a serial console and an ssh
login are shells no session started: the emulator cannot open there, and the caller is already
sitting at a terminal. `kb_terminal()` answers NULL and the entry runs in place.

Unless the entry asked for one by name. `X-KDOS-Term` names the emulator a program needs — the one
that draws pictures in the cell grid — and is honoured whatever the session runs. It is a name and
never a program: only an emulator this image ships is accepted and anything else falls back to the
session's own, because an entry is a file anything can write and a key naming a program would be a
second `Exec` line with none of the field-code rules.

The handler's `Exec` is split by `kxdg_exec_split()`, which is the launcher's own split. Split on
whitespace instead, `Exec=foot --title="Install KDOS" -- sudo kinstall` reaches `foot` as
`--title="Install` with a stray `KDOS"` after it, and `--open=%f` cannot be expressed at all
because the code is not a word of its own. Field codes are substituted rather than stripped — the
code *is* the file, and dropping it opens the application with an empty document — and a line
carrying no code takes its documents appended, which is the same decision `sh_launch()` reads off
the same line. The opener and the launcher cannot disagree about where a document goes.

The shared MIME database is compiled on the target. The port ships the source definitions and
nothing else — no glob table, no cache — so every consumer asking what type a file is gets no
answer. An install hook compiles it, which only an install-time hook can do, because the compiler
is a target binary.

## Box profiles

`~/.config/kdos/boxes/<name>.conf`, flat `key = value`.

An application box and a development box differ in one key that changes a launch — `base`, which
picks the lane — and in two that only describe the box, `persistence` and `export`. They do not
differ in kind, which is what makes one manager over two lanes honest rather than a wrapper over
two systems.

| Key | Maps onto |
|---|---|
| `base` | `pack:<id>`, `box:<name>` or `image:<ref>` |
| `persistence` | Descriptive — the writable layer lands where the runtime puts it |
| `export` | Descriptive — nothing exports on the strength of it |
| `network`, `ipc` | Namespace flags — create-time |
| `devices` | Whether `/dev` and `/sys` are bind-mounted in |
| `processes` | `shared` is `--pid host`; private is a process namespace of the box's own |
| `home` | `private` gives the box a home of its own; shared is the user's own `$HOME` |
| `init` | `--init` on the image lane. A pack box always runs `kdos-boxinit` |
| `wayland` | Descriptive — a launch tags every box through `kdos-boxsock` either way |
| `audio` | Rides on `devices` |
| `gpu` | The card's device nodes. Subtracts nothing from a shared `/dev`; binds `/dev/dri` back into a box whose devices are private |
| `render` | `auto` (the default), `gpu` or `software` — which graphics this box's applications get |
| `memory` | Passed as `--memory` and enforced by `kdos-oomd`, because rootless has no cgroup to enforce it with |
| `cpus` | `--cpus`. Absent is every core |
| `pids` | `--pids-limit`. Absent is unlimited |
| `accent` | The box's colour, which is what draws a title-bar chip |
| `autostop` | Idle timeout for the collector |
| `grant` | Compositor globals the sandbox allowlist otherwise refuses |
| `image` | The reference, for a registry base |

Three properties the list is written to keep. Every key that changes a launch maps one-to-one onto
a container-engine flag or onto something KDOS enforces itself, and the profile printer names the
mechanism behind each line. A key that enforces nothing is printed as such rather than left to read
as a switch. And an unknown key is reported by name.

`export`, `wayland` and `persistence` are the three in that state, and `kdos-box profile` prints a
`!` line under any one of them set to something it cannot deliver:

- **`wayland`** cannot take a display away. The box shares `$XDG_RUNTIME_DIR`, so a client that
  opens the default `wayland-0` reaches the session's own socket; withholding `WAYLAND_DISPLAY`
  would advertise a confinement the sandbox does not have. The per-box `kdos-boxsock` socket a
  launch hands over is what carries the tag the compositor's allowlist filters on, and it is handed
  over whatever the key says.
- **`export`** triggers nothing. `kdos-appbox genlaunchers` writes launchers, shims and the MIME
  cache for every installed pack and every store box at once, and `kdos-box export <box> <app>` is
  the per-application route.
- **`persistence`** is remembered rather than imposed. `distrobox create` makes a named container
  whose writes land on disk, and on the pack lane kdos-packd decides ephemeral-or-not from the pack
  itself; no launch reads the key. `persistent` therefore describes what every box already does,
  and `ephemeral` and `frozen` are a preference the profile carries, not a confinement.

A shared `/dev` cannot have a hole cut in it, so with `devices = shared` both `gpu` and `audio`
ride on that key and there is no flag that grants a box a speaker and denies it a camera. `gpu` is
enforceable in the other direction only: with `devices = private` the box has no `/dev` at all, and
`gpu = yes` is the `--volume /dev/dri` that binds the card back.

`display` is carried and not interpreted. Nothing on this desktop reads it and it means nothing to
a container flag; `kdos-box profile` carries it through a rewrite untouched, because a profile
writer that knows only its own keys deletes everybody else's — a setting that disappears the next
time an unrelated one is changed.

### render

`render` reaches the guest's own Mesa, and it defaults to the card. The render nodes are bound into
every box, the DRI drivers and `libva` are in the base pack and both renderers are built, so a box
drawing with llvmpipe on a machine that has all three is paying for nothing.

`auto` — the default, and what an absent key means — asks the machine. `profile_render_gpu()`
resolves it by opening a `/dev/dri/renderD*` node, because a node owned by the `render` group that
this session cannot open is the same dead end as a machine with no card. `gpu` is a request for the
same thing, and `software` is the one value that refuses it whatever is plugged in.

The resolved answer becomes `LIBGL_ALWAYS_SOFTWARE=1` in the launch environment, and only for the
software answer. For the hardware answer Mesa already asks the right question and falls back by
itself, and a variable pinning hardware would take that fallback away. It is advisory — an
application may unset it — and `kdos-box profile` prints it as the renderer line rather than as
confinement.

`kdos-box profile` prints the key and what it resolves to on this machine, because a profile states
a wish and the hardware answers it. A box that can see no node is refused without opening anything:
`devices = private` with `gpu = no` leaves `/dev/dri` out of the box, and this process's own `/dev`
is not that box's, so the host path it could open is one that does not exist inside.

```
render      = auto        /dev/dri/renderD128
render      = software    LIBGL_ALWAYS_SOFTWARE=1 (advisory) — the profile refuses the card
render      = auto        LIBGL_ALWAYS_SOFTWARE=1 (advisory) — no render node on this machine
render      = auto        LIBGL_ALWAYS_SOFTWARE=1 (advisory) — no /dev/dri inside this box
```

It is a different question from `gpu`, which is about device nodes rather than about who draws. A
profile rewrite that dropped the key would change what the application draws with, which is why
every writer of this file carries keys that mean nothing to a container flag.

### memory, namespaces and grants

`memory` is enforced by `kdos-oomd`, and that is what makes the key honest: rootless containers on
a machine with no cgroup delegation accept a memory limit and ignore it. The daemon reads the
profiles and prefers a box that is over its own declared budget as a victim, ahead of the general
rule that boxed processes are preferred.

A namespace key applies at create time. It cannot be re-flagged on a live container, so changing
one prints an instruction to recreate the box rather than silently doing nothing.

`grant = screencopy, data-control` opens a global the sandbox allowlist refuses. The compositor
consults the box's profile once per client and caches on the identifier; a reload drops the cache.
The names map onto both generations of each protocol, and the input-method grant has to be spelled
out because it is a keylogger by design.

`base = image:<ref>` is an online operation and says so before doing anything. It fetches unsigned
content from somebody else's registry, the strict-signature setting does not cover it, and
pretending otherwise would be dishonest. `pack:` and `box:` are the offline kinds.

A profile with no base reads as unset in the listing until the first launch records the pack it
used. Creating a box with none is refused.

## kdos-box

### freeze, import and clone

`freeze` packs the box's writable layer into one image through the pack builder, with the base
chain recorded as requirements. The artefact is therefore the difference, and it diffs against a
previous freeze like any other pack.

Measured on a development box with real work in it: 2.1 MB against a 432 MB merged root — about 195
times smaller, and about 98 times smaller than the base pack it sits on.

There is no alternative to compare it against. A pack box is created over an exploded root, so the
container engine refuses to commit it outright: there is no image to save. Freeze is the only way
to capture a pack box's state.

`import` stages the result and asks the daemon to install it, because verification happens where
the mount happens.

### Snapshots and rollback

A snapshot is a copy of the writable layer, and the cost is stated: on an ordinary filesystem that
is everything the box has written.

It is deliberately not a pack. A pack cannot be written back into a writable layer without being
mounted, and a rollback needing the daemon would fail exactly when a box is broken.

### export

A secondary box's application gets a box-qualified desktop identifier and a box-qualified shim,
while the default box keeps upstream's own identifier so nothing that works today changes.

That is a refinement of the launcher-naming rule rather than an exception to it. The rule exists
because a dock matches a window to an entry by file identifier, and the panel has a better key than
the filename.

## Warmup and collection

Warmup is the pinned set. One box per application means a single login warmup covers nothing, so
the warmup reads the favourites file — which holds desktop identifiers — takes the pack from each
entry's `Exec` (`-b <pack>` where it is named, otherwise the command resolved through the table),
and composes and starts that pack's box at low priority. The first word of a generated entry is
this binary and never a shim, so reading a shim out of it warms nothing and exits 0.

The collector runs every ten minutes from the session, at `nice -n 10`, and asks the compositor
first: a box with a mapped window is not idle whatever its clock says, and the command socket is
the one question that answers it. A warmed box with no collector calling it is a leak.

## Storage drivers

Two, and the choice must never flip.

| Situation | Driver |
|---|---|
| A live session | The userspace overlay implementation |
| An installed system on an ordinary filesystem | The kernel's native overlay |

A live session needs the userspace one, because the home directory sits on the boot overlay and the
kernel refuses to stack an overlay upper layer on an overlay. The engine does not fall back: the
container fails to mount.

On an installed system the kernel's implementation is much faster, so a one-time per-user
configuration is written when the home directory's filesystem supports it, and only while the store
has no containers yet. The two write incompatible deletion markers into container layers.

## See also

- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the format, the daemon and the container
- [Applications](../02-user-guide/applications.md) — using all of this
- [The daemons](daemons.md) — the pack daemon and the memory daemon
- [The session](../03-architecture/session.md) — the environment and what is shared
- [The security model](../03-architecture/security-model.md) — what a box is and is not
