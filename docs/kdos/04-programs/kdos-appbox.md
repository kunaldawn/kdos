# kdos-appbox, kdos-box and xdg-open

This page is the reference for the program that runs containerised applications on KDOS. One
binary answers to three names:

| Name | What it is for |
|---|---|
| `kdos-appbox` | Installing applications from the catalogue, launching them, and writing their launchers |
| `kdos-box` | Managing boxes as objects of their own: create, enter, freeze, snapshot, clone, remove |
| `xdg-open` | Opening a file or a link in whatever handles it, boxed or native |

It is for anyone who wants to know exactly what happens between a click and a window, who runs a
development box, who administers a machine with boxed software on it, or who is changing the launch
path. If you only want to install and use applications, read
[Applications](../02-user-guide/applications.md) first: it covers the everyday commands, and this
page is the detail behind them. The pack format and the pack daemon are in
[Packs and boxes](../03-architecture/packs-and-boxes.md).

Two terms recur. A **box** is a rootless container one application runs in; a **pack** is a signed
filesystem image that a box is composed from. Both are in the [glossary](../06-reference/glossary.md).

## Quick start

```sh
kdos app install app.gimp             # the usual front door; runs kdos-appbox install
kdos-appbox catalogue                 # everything the catalogue offers, and what is installed
kdos-appbox install app.gimp          # build and install one application
gimp photo.png                        # every installed application is also a command
kdos-appbox open report.pdf           # open a file in whatever handles its type
kdos-box clone app.gimp work          # a box of your own: GIMP's software and a copy of its work
kdos-box enter work                   # a terminal inside it
kdos-box profile work                 # what the box is allowed, and how that is enforced
```

## Synopsis

```
kdos-appbox [-b BOX] [-v] run <app> [args...]
kdos-appbox open [--print] [--choose] <path|uri>...
kdos-appbox catalogue [--groups | --selftest]
kdos-appbox install <id|group>... [--dry-run]
kdos-appbox uninstall <id|group>...
kdos-appbox export <file.ktar> <id|group>...
kdos-appbox import <file.ktar> [<id>...]
kdos-appbox list | apps | warmup | status
kdos-appbox store --selftest
kdos-appbox genlaunchers --packs <fs-root>
kdos-appbox genlaunchers --packs --user
kdos-appbox genlaunchers --packs-dir <dir> <fs-root>
kdos-appbox genlaunchers <desktop-dir> <fs-root>

kdos-box list | ls
kdos-box create <name> [base=pack:<id>|image:<ref>] [key=value ...]
kdos-box enter <name> [command ...]
kdos-box run <name> <app> [args ...]
kdos-box apps <name>
kdos-box export <name> <app>
kdos-box unexport <name> <app>
kdos-box freeze <name> [out.kpack]
kdos-box import <file.kpack> [as <name>]
kdos-box clone <src> <dst>
kdos-box snapshot <name> [tag]
kdos-box snapshots <name>
kdos-box rollback <name> <tag>
kdos-box start | stop | restart <name>
kdos-box remove <name> [--force]
kdos-box profile <name> [key=value ...]
kdos-box gc [--dry-run]

xdg-open <path|uri>...
<app-name> [args...]
```

The last form is a **shim**: a symbolic link named after an application, pointing at this binary.
`gimp photo.png` runs GIMP in its box exactly as the Start menu would.

### Global options for kdos-appbox

These come before the command word.

| Option | Effect |
|---|---|
| `-b NAME`, `--box NAME` | Run in the box `NAME` instead of the application's own box |
| `-v`, `--verbose` | Let the container engine print to standard error. By default its output is discarded, so a click never sprays container messages at the desktop |
| `-h`, `--help` | Print the usage summary and exit 0 |

An unknown option exits 1 with a message. With no command, `kdos-appbox` prints its usage and
exits 1; `kdos-box` with an unknown or incomplete command prints its own usage and exits 2.

## Description

### Two ways an application reaches a box

| Lane | How the application arrives | What the box is | Profile `base` |
|---|---|---|---|
| Store | `kdos-appbox install` builds it on this machine from the catalogue, as a stack of container images `kdos/base`, `kdos/rt-gtk`, `kdos/app.gimp` … | A container created by `distrobox create` over the top image | `image:kdos/<id>` |
| Pack | A signed pack is installed through `kdos-packd` — by `kdos-appbox import`, `kdos-box import`, or an image that carries packs | A container created with `podman create --rootfs` over an overlay that `kdos-packd` composes from the pack and everything it requires | `pack:<id>` |

Both lanes share one launch path, one environment, one set of launchers and one profile format.
What differs is how the box's root filesystem is made.

The box for an application is named after it — `app.gimp` — and a box created without a profile
records its base in one the first time it is launched.

### No shell anywhere

Application names, package names and file names all arrive from desktop entries and from command
lines, and a shell between them and the program would turn any of them into a way to run arbitrary
commands. Everything is executed through an argument-vector builder instead. The one shell command
in the program runs *inside* a box, over a fixed string (`kdos-box apps` listing a directory).

The program links four KDOS libraries — `libkbase`, `libktui`, `libkcolor` and `libkxdg` — and no
other library. Desktop notifications go over the session bus through `gdbus`.

### Where it is installed

| Path | What |
|---|---|
| `/usr/local/bin/kdos-appbox` | The binary |
| `/usr/local/bin/kdos-box` | A symbolic link to it |
| `/usr/local/bin/xdg-open` | A symbolic link to it |
| `/usr/share/kdos/appstore/catalogue` | The catalogue it reads |

`/usr/local/bin` comes before `/usr/bin` on the shipped `PATH`, which is why this `xdg-open` answers
ahead of the xdg-utils script. The recipe depends on `podman`, `distrobox`, `shared-mime-info` and
`xdg-utils`.

## Commands

### run

```sh
kdos-appbox run gimp-3.0 photo.png
kdos-appbox -b scratch run bash
```

Starts a program in its box. See [The launch path](#the-launch-path) for everything that happens
between the command and the window.

Without `-b`, the box is found from the command: the name-to-command table is searched for a row
whose command matches the whole argument list, then by the program's basename. A command no
installed application carries is refused with a sentence naming it and suggesting `kdos app list`.

### open

```sh
kdos-appbox open report.pdf
kdos-appbox open --print report.pdf     # resolve and print; do not run anything
kdos-appbox open --choose report.pdf    # always ask which application
```

Resolves a file or a URI to the application that handles it and runs that application. See
[The open path](#the-open-path).

`--print` writes the resolution as tab-separated lines instead of running it:

```
mime        application/pdf
candidates  org.pwmt.zathura    org.gnome.Evince
default     yes
entry       /usr/share/applications/org.pwmt.zathura.desktop
exec        zathura    /home/kdos/report.pdf
```

A `choose` line appears when the chooser would be asked; with no handler at all, `entry` is `-` and
`exec` names `/usr/bin/xdg-open`.

### catalogue

```sh
kdos-appbox catalogue
kdos-appbox catalogue --groups
kdos-appbox catalogue --selftest
```

With no option it prints one tab-separated line for each `app` and `data` row:

```
<id>  <name>  <category>  <bytes>  <parent>  <installed|available>  <tagline>
```

Tab, because a tagline contains spaces and every other table in this tree splits on tab. `--groups`
prints one line per named group instead: `<id>`, tab, `<description>`, tab, then the member ids
separated by spaces.

This command is the only place that decides what is installed. The store, `kinstall` and
`kdos app` all ask it, so they cannot disagree. A box the container engine lists under a catalogue
id *is* that application — nothing else could be launched — and a machine with no container engine
reports everything as `available`.

`--selftest` runs the catalogue parser's own assertions against the file named by
`$KDOS_CATALOGUE`. It needs no daemon and no display, and it is checked before anything that does,
so it runs in a build container. See [Testing](../05-developer/testing.md).

The shipped catalogue carries 180 applications and 2 data rows, built on 2 base rows (`alpine` and
`base`) and 7 runtimes (`rt-gtk`, `rt-qt`, `rt-kde`, `rt-media`, `rt-sci`, `rt-electron`,
`rt-wine`), grouped into 7 named groups: `essential`, `office`, `creative`, `dev`, `science`,
`make` and `games`.

### install and uninstall

```sh
kdos-appbox install <id|group>... [--dry-run]
kdos-appbox uninstall <id|group>...
```

An argument is a catalogue id (`app.gimp`) or a group name (`creative`); a group expands to its
members and duplicates are dropped.

Install builds each application's chain bottom-up as a stack of images, one per catalogue row, each
`FROM` the one below: `kdos/base`, then `kdos/rt-gtk`, then `kdos/app.gimp`. A second GTK
application is therefore one apt pass rather than three, because the base and the runtime images
already exist and are skipped. The image build context is `/var/empty`, since nothing is copied in.

- **An image that exists is never rebuilt.** `podman image exists` is the whole check: the
  catalogue carries no version per row, so an image is current until somebody removes it.
- **A data row an application needs comes with it.** `needs app.kicad data.kicad-packages3d` makes
  installing KiCad build the 3D model library too.
- **`--dry-run` prints the generated Containerfiles** instead of building. It tracks what the same
  run has already covered, so a preview of two GTK applications shows the runtime once, as a real
  install would build it.
- **Nothing rolls back.** Six applications where the fourth fails leaves five installed and names
  the fourth. An installed application is not harmed by a later one failing, and undoing it would
  throw away twenty minutes of apt.
- **Progress is one flushed line per step** (`==> building kdos/app.gimp`, `==> app.gimp
  installed`), so a program showing install progress can read standard output without parsing the
  container engine. Such a program should start `kdos-appbox` itself and read its output directly.
  Reading it through the output of a service that `ksvc` supervises never ends, because the
  supervisor keeps the pipe open.

When the images are built, install creates the box with `kdos-box create <id>
base=image:kdos/<id>` and regenerates your launchers from every store box, so the application is in
the Start menu before the command returns. The exit status is the number of applications that
failed.

`uninstall` removes the box, then the application's own image, then each runtime above the base
that no remaining box's chain still names, and then regenerates the launchers from what remains. The
base image is never removed: it is every chain's floor. Which runtimes are still wanted is asked of
the catalogue rather than of the engine, because a dangling-image sweep cannot tell a runtime
nothing uses from one whose only application is being installed right now.

**The Debian snapshot.** The catalogue's `snapshot` line pins the Debian archive the packages come
from. `snapshot = auto`, which is what ships, reads the date from the base image's own
`/etc/apt/sources.list.d/debian.sources`, so the packages installed on top cannot disagree with the
root filesystem under them. The date is taken from a comment line of this form in that file:

```
# https://snapshot.debian.org/archive/debian/20260824T000000Z
```

A literal date such as `20260824T000000Z` pins explicitly, and `off` reads the live archive. If the
comment line is missing or has a different form, `auto` cannot be resolved: the install warns that
it is building against the live archive and carries on unpinned rather than refusing to install.
That warning is the only way to tell an unpinned build from a pinned one afterwards.

### export and import

```sh
kdos-appbox export <file.ktar> <id|group>...
kdos-appbox import <file.ktar> [<id>...]
```

A set of installed applications as signed packs in one file, for a machine with no network or for
keeping a known-good set. `export` needs a selection and skips any id that is not installed here.
`import` does not need one, because the archive carries its own; naming ids narrows it.

```
apps-2026-09-18.ktar
  SELECTION        the groups and ids this set was exported as
  PACKAGES         id, version, size and payload hash per pack
  PACKAGES.sig     present when a signing key was readable
  app.gimp.kpack
  app.inkscape.kpack
```

Four choices shape the format:

- **Packs, not a container-engine save.** A store install is unsigned content from a registry; a
  pack is hashed and signature-checked by `kdos-packd` at the moment it is mounted. An imported
  application is therefore verified where a store-built one is not.
- **`podman export`, not the overlay store.** An image's content is spread across its layers, and
  only an export flattens them. It also keeps overlay whiteouts and `trusted.overlay.*` extended
  attributes out of play, which is what lets this run without root.
- **`kdos-pack build`, not `mkfs.erofs` directly.** The build command already carries the
  reproducible flag set, so there is one answer to how a pack is made. Its `--force-uid=1000` is
  what lets an export run unprivileged: a box runs with `--userns keep-id`, so the process inside is
  uid 1000, and a tree owned by real root is one it could create nothing in.
- **An unsigned index says so.** With no `KDOS_PACK_KEY` the set still indexes and imports, and
  every hash is still checked at the mount, but the export prints that it was not signed.

Import stages each pack into the pack daemon's own staging directory and hands the daemon a file
name, never a path. That rule is what stops a request reachable from the `wheel` group from turning
into a mount of an arbitrary device. A pack the daemon refuses is named and skipped; the rest of the
archive still imports, and the last line reports how many imported and how many failed.

`SELECTION` is plain text and can be edited or compared. `#` lines are comments, a `group` line
records what was picked, and the bare id lines are what the archive carries — so a group whose
membership changes later still imports exactly the software in the file.

### list, apps, warmup, status

| Command | Prints or does |
|---|---|
| `list` | Every container, its image, its state, and whether its profile is `custom` or `default` |
| `apps` | The applications this machine knows, from the name-to-command table |
| `warmup` | Composes and starts the boxes behind your pinned favourites. See [Warmup and collection](#warmup-and-collection) |
| `status` | What the pack daemon reports, then whether the default box exists and its state |

### store --selftest

`kdos-appbox store --selftest` checks the Containerfile generator and the `SELECTION` reader
offline. Like the catalogue self-test, it needs no container engine.

## The launch path

In order, because every launcher on the system and the login warmup depend on the order:

1. **Choose the storage driver, once.** Every invocation checks this first; it settles only once
   per user. See [Storage drivers](#storage-drivers).
2. **Resolve the box.** A generated launcher for a pack application writes
   `Exec=kdos-appbox -b <pack> run <exec>`, so the box is named outright. Without `-b` — a prompt, a
   shim, an entry naming no box — the box is resolved from the command as described under
   [run](#run).
3. **Say that a box is starting**, if it is not already running: a desktop notification, "Starting
   app", sent in the background with a two-second reply timeout so it can never hold up the launch.
4. **Recover a stuck box.** A stopped box is often still *stopping*: the container's init stays
   alive reaping after a stop, and asking the engine to start it then fails with "container state
   improper", which names nothing a person can act on. An application hung in uninterruptible I/O
   can leave a box there for good. The recovery waits up to fifteen seconds, then kills the
   container and waits two more, then removes it (with a "Resetting app container" notification).
   Nothing is lost: a box's packs are read-only and its writable layer is on disk, so the container
   is recreated over the same stack. `kdos-box start` shares this recovery.
5. **Create the box, if it does not exist.** For a pack box this asks `kdos-packd` to compose the
   stack and runs `podman create --rootfs` over it, under a lock
   (`$XDG_RUNTIME_DIR/kdos-appbox.create.lock`) so a launch and the login warmup cannot race to
   create the same container. A first launch prints `==> First launch: composing '<box>' from
   packs...`.
6. **Start it, composing first.** A pack box's overlay lives under `$XDG_RUNTIME_DIR`, which is a
   temporary filesystem, so after a reboot the container still exists but the root it was created
   over is gone. Composing is idempotent — the daemon counts references — so a box that is already
   composed costs one round trip.
7. **Wait for readiness.** The box's init prints `container_setup_done` when the user account inside
   exists. If that line is not yet in the container's log, the launch waits for it, for at most 30
   seconds. Running a program in a box before that point runs it with no user.
8. **Build the environment.** See [The environment a box gets](#the-environment-a-box-gets). This
   includes starting `kdos-boxsock` for the box and waiting up to one second for its socket.
9. **Execute.** `podman exec --interactive --user <uid>:<gid> --workdir $HOME <box> env … <program>`,
   with `--tty` added when this process's own input is a terminal. The same command therefore gives
   an interactive prompt at a shell and a plain execution from a launcher.

Stage timings are appended to `$XDG_RUNTIME_DIR/kdos-appbox.trace`, one timestamped line per stage.
On the reference machine a cold launch with no container at all takes about 18 seconds, and a
launch into a running box about 0.3 seconds.

None of those stages is visible to the compositor. What a person sees during a cold start is the
"Starting app" notification and then nothing until the application connects, because there is no
window to map before that.

## The environment a box gets

A program run with `podman exec` inherits nothing — neither the container init's environment nor
the caller's — so every variable is set explicitly. The reasons behind each one are in
[The session](../03-architecture/session.md#the-environment-a-box-receives).

| Variable | Value |
|---|---|
| `WAYLAND_DISPLAY` | The box's own tagged socket, `$XDG_RUNTIME_DIR/kdos-box-<box>@<display>.sock`, when `kdos-boxsock` provides one; otherwise unset, and the client finds the session's socket |
| `PATH` | `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games` — the games directories are where Debian puts its games |
| `XDG_RUNTIME_DIR` | `/run/user/<uid>` |
| `DBUS_SESSION_BUS_ADDRESS` | `unix:path=/run/user/<uid>/bus` |
| `XDG_SESSION_TYPE` | `wayland` |
| `HOME`, `USER`, `LOGNAME` | Yours |
| `LANG` | Yours if it is a UTF-8 locale, otherwise `C.UTF-8` |
| `GSETTINGS_BACKEND` | `keyfile`, because no dconf service is reachable |
| `NO_AT_BRIDGE`, `GTK_A11Y` | `1` and `none`, unless you opt in to accessibility (below) |
| `GTK_USE_PORTAL`, `XDG_CURRENT_DESKTOP` | `1` and `KDOS`, so toolkits use the KDOS file chooser and settings portals |
| `CUPS_SERVER` | `/run/cups/cups.sock`, when that socket exists |
| `QT_IM_MODULE` | `wayland`: input methods reach a box through the compositor |
| Qt theming and anything else a runtime needs | From the pack or the catalogue (below) |
| `LIBGL_ALWAYS_SOFTWARE` | `1` only when the profile's `render` key resolves to software. See [render](#render) |
| `DISPLAY` | Yours, or `:<n>` from the first `/tmp/.X11-unix/X<n>` socket, so an X11-only application under Xwayland finds the server |

**Accessibility** is a default, not a policy. The host runs no accessibility bus, so by default
every boxed GTK application is told not to look for one — a probe that could only time out. Create
`~/.config/kdos/a11y` (an empty file is enough) to opt in for good, or set `KDOS_A11Y=1` for one
launch; `KDOS_A11Y=0` opts out for one launch. The path resolves from `$HOME/.config`, the same way
the box profiles do.

**Qt theming and other per-runtime variables** come from the software, not from a list in this
program, so the variable and the package that makes it work cannot drift apart:

- A pack box exports every `env =` line in its pack's metadata and in every pack it requires. The
  nearest pack wins, so an application can override its runtime.
- A store box exports the catalogue's `env <row> NAME=VALUE` lines for its chain. `rt-qt` sets
  `QT_QPA_PLATFORMTHEME=gtk3` and `QT_STYLE_OVERRIDE=Fusion`; `rt-kde` sets
  `QT_QPA_PLATFORMTHEME=kde`, which reads the `kdeglobals` that `kdos theme` writes.
- Any other image box is asked for its `kdos.qt-kde-theme` and `kdos.qt-gtk-theme` labels.

A value may begin with `$HOME`, which is replaced with your home directory; the catalogue cannot
know your user name. `GTK_THEME` is never set, because it would override the theme for the life of
the process and an accent change could never reach a running application.

## Launcher generation

```sh
kdos-appbox genlaunchers --packs <fs-root>             # every installed pack, into the system tree
kdos-appbox genlaunchers --packs --user                # every installed pack, into your tree
kdos-appbox genlaunchers --packs-dir <dir> <fs-root>   # extracted packs, one per subdirectory
kdos-appbox genlaunchers <desktop-dir> <fs-root>       # one applications directory
```

`genlaunchers` reads applications' own desktop entries and writes everything the host needs to
present them as native. You rarely run it by hand: installing and uninstalling run it for you.

| Source form | Reads |
|---|---|
| `--packs` | Every app pack `kdos-packd` lists as `installed` or `mounted`, mounted through the daemon. Available packs are skipped: mounting one to read it is what installs it |
| `--packs-dir <dir>` | `<dir>/<pack-id>/usr/share/applications` for each subdirectory. The directory name is the box the launchers dispatch to |
| `<desktop-dir>` | One applications directory |

A pack or directory with no `usr/share/applications` contributes nothing; such an application is
reached with `kdos-appbox -b <pack> run <command>`, and `kdos app show <pack>` names its commands.

An extra argument is refused rather than ignored. The `--packs` and two-argument forms differ by one
argument, so passing both a directory and a root would read the directory as the root and write the
whole set underneath it, exit 0, and leave the real table untouched.

A write that fails stops the run with the path that could not be written, so a run without the
permission for the tree it targets never reports a launcher set it did not write. The summary on
standard error reads `<n> launchers, <n> command-only, <n> mime types, <n> shims`.

### Four outputs

| Output | Without it |
|---|---|
| A desktop entry per application, marked `X-KDOS-Alien=true` | No launcher |
| `mimeinfo.cache` beside them | No type association is ever consulted, so no boxed application appears in Open With |
| The name-to-command table, `alien-apps` | A shim cannot find what to run |
| A shim per application | The application is not a command |

The MIME cache is written here rather than by `update-desktop-database`, because the host carries no
desktop-file utilities.

### Two trees

| Tree | Entries | Table | Shims | Written by |
|---|---|---|---|---|
| System | `<fs-root>/etc/skel/.local/share/applications` | `<fs-root>/usr/share/kdos/alien-apps` | `<fs-root>/usr/local/bin`, relative links | The image build, and `--packs <fs-root>` as root |
| User | `~/.local/share/applications` | `~/.local/share/kdos/alien-apps` | `~/.local/bin`, absolute links to `/usr/local/bin/kdos-appbox` | `install`, `uninstall`, and `--packs --user`, as you |

Every reader looks in your tree first: the Start menu reads your applications directory, the
dispatcher reads your table before the system one, and `~/.local/bin` is on the `PATH` the skeleton
profile sets. That is why an install needs no root.

The system tree is reconciled at image-build time by `script/06_packaging/00_launchers.sh`, which
runs `kdos-appbox genlaunchers --packs-dir "$KDOS_PACK_EXTRACT" /` (default
`/var/tmp/kdos-pack-extract`) and then fails the build if alien desktop entries survive a run that
found no packs. The generated set is not under `fs/`, so nothing else would remove it. The step runs
before `00_user.sh`, which copies `/etc/skel` into every home; a skeleton cleaned after that step
would leave stale launchers in `/home/kdos`, which the Start menu reads first.

`genlaunchers` reconciles rather than appends. It removes every `.desktop` carrying
`X-KDOS-Alien=true` and every shim it recognises as its own, then rewrites the table and the MIME
cache whole. One call is therefore the whole clean-up.

A launcher can still outlive its application when the application is removed by a path that does
not run `genlaunchers` — a pack removed through `kdos-packd` directly, for example. The Start menu
and the Open With chooser drop such a row at runtime (`sh_box_missing()` in `kdos-shell`) when its
box is neither an installed `.kpack` nor a box profile. Absence has to be proven: an unnamed box, a
name that is not an id, and a machine with no pack store all count as present, because hiding an
application somebody installed is worse than showing one whose pack has gone.

### Naming rules

- **The launcher carries its box.** An application belonging to a pack gets
  `Exec=kdos-appbox -b <pack> run <exec>`; one with no pack gets `Exec=kdos-appbox run <exec>`. The
  pack id is also the box name and the stem of the box profile, so anything reading the command line
  learns the box's policy key directly, and `run` skips the table lookup.
- **The launcher's file name is upstream's own desktop id**, not a KDOS-prefixed name and not the
  window-class field. The panel matches a running window to an entry by the entry's file id, so a
  mismatch shows a second, generic icon beside the pinned one.
- **A Wayland app id is not the X11 window class.** GIMP's entry declares
  `StartupWMClass=gimp-3.0` while its window announces the app id `gimp`, which is its upstream file
  name. Pinned favourites therefore name upstream ids. `StartupWMClass` is still written, since an
  X11 application under Xwayland matches by it.
- **A shim is named after the program its entry runs**, through the one function that decides which
  program a command line runs (it skips an `env` prefix and looks inside an `sh -c` wrapper). A
  reverse-DNS entry therefore gets a shim named after the real program. The `RENAME` table wins
  where upstream's name is not the one people use, and a reserved or odd name falls back to the
  lower-cased id.
- **A terminal entry stays one.** The generated launcher keeps upstream's `Terminal=` flag, and the
  shell wraps such an entry in a terminal. Written as false, such a program would start with a pipe
  for input and exit on a usage error, with no window and no message.

### The tables

These live in `launchers.c`.

| Table | Holds |
|---|---|
| `COMMANDS` | `wine`, `winecfg`, `winetricks`: programs used as commands, which get a table row and a shim but deliberately no desktop entry, because a launcher for `wine` with no arguments opens nothing. Written only where the source really carries the binary. The catalogue's `cmd <pack> <name>` rows add more |
| `ENTRIES` | Applications that ship no desktop entry at all, with the entry written here: `surf`. A browser with no launcher claims no URL scheme, so installing it would change nothing about what opens a link. Written only where the binary is present |
| `RENAME` | Upstream names people do not use: `firefox-esr` → `firefox`, `org.inkscape.Inkscape` → `inkscape`, `codium` → `vscodium`, and 57 more |
| `RESERVED` | Names no shim may take and the sweep must not delete: shell and system tools, `kdos`, `foot`, the native `git`, `gnuplot` and `mpv`, and this program's own three names |
| `EXEC_EXTRA` | Arguments needed only because the application is containerised: VSCodium gets `--no-sandbox --ozone-platform-hint=auto --disable-gpu-compositing` |
| `SKIP_NEEDS_KWIN` | `org.kde.spectacle`, which opens an error dialog on any compositor but KWin. Screenshots on KDOS are `kdos-shot` |
| `SKIP_ROOTLESS_INERT` | GParted, GSmartControl, GNOME Disks, TestDisk, Baobab-as-root and Timeshift: they need raw block devices, which a rootless container cannot give. Use the native recovery tools instead |
| `SKIP_PREFIXES`, `SKIP_BASENAMES` | Entries that are not applications: settings panels, helpers, URL handlers |
| `X11_FORCING` | `GDK_BACKEND=x11`, `CLUTTER_BACKEND=x11`, `QT_QPA_PLATFORM=xcb`, `SDL_VIDEODRIVER=x11`, `MOZ_ENABLE_WAYLAND=0`, `ELECTRON_OZONE_PLATFORM_HINT=x11` — stripped from an `Exec` line. An application that genuinely needs X11 says so with an `env` row in the catalogue, which this table cannot strip |

`RESERVED` is consulted when shims are swept as well as when they are written. The sweep removes
every link that points at this binary by a relative path (the system tree) or by
`/usr/local/bin/kdos-appbox` (your tree); `kdos-box` and `xdg-open` are such links too, and without
the reserved check a regeneration would delete the front door to every box on the machine.

### Exec lines

An `Exec=` line is not a whitespace-separated list. It carries the desktop-entry format's quoting —
`Exec="/usr/bin/gsmartcontrol-root"`, or `sh -c "wesnoth-1.18 >/dev/null 2>&1"` whose shell argument
must reach the shell in one piece — and it carries field codes such as `%f`, which must disappear
when no file was selected or a media player tries to open a file literally named `%f`.

`kxdg_exec_split()` in `libkxdg` is the single implementation. It removes the quoting, substitutes
the single-file and multiple-file codes, drops the codes that carry no argument, and — when asked to
— keeps every code verbatim for a tool that rewrites a line instead of running it.
`kxdg_exec_quote()` is its inverse, so what the generator writes reads back as the same arguments.
Every launch path goes through it, and the test suite checks both directions against real lines from
the catalogue.

A shim run from a terminal expands the field codes to nothing and appends your own arguments after
the command.

## The open path

`xdg-open` on KDOS is this binary. Everything that opens a link says that word — a mail client's
open-link command, a portal, anything reading `$BROWSER` — and means "whatever this machine opens it
with". The xdg-utils script stays installed at `/usr/bin/xdg-open` and is the last resort, reached by
absolute path, because calling it by name would find this binary again and loop.

Resolution runs in four steps:

1. **What the argument is.** `kxdg_mime_for_arg()` types an argument with a scheme as
   `x-scheme-handler/<scheme>` and unwraps `file:` to the path it names; anything else is a path,
   typed from the shared MIME database's glob table (longest matching suffix wins). Deciding by the
   basename alone would type `mailto:a@b.c` by its `*.c` glob and offer a mail address to a C
   editor. The chooser asks the same function, so both sides agree.
2. **Who handles that type.** The search, in order:

   | Order | File | Section |
   |---|---|---|
   | 1 | `~/.config/kdos-mimeapps.list` | Default Applications |
   | 2 | `~/.config/mimeapps.list` | Default Applications |
   | 3 | `~/.config/kdos-mimeapps.list`, then `~/.config/mimeapps.list` | Added Associations |
   | 4 | `/etc/xdg/kdos-mimeapps.list`, then `/etc/xdg/mimeapps.list` | Default Applications |
   | 5 | `applications/mimeinfo.cache` in `$XDG_DATA_HOME`, then each of `$XDG_DATA_DIRS` | MIME Cache |

   `~/.config` is `$XDG_CONFIG_HOME` where that is set. The `kdos-` prefix is the first name in
   `XDG_CURRENT_DESKTOP`, lower-cased, as the specification asks. A match in rows 1, 2 or 4 is a
   *default*. The MIME cache in row 5 is the file `genlaunchers` writes, so a boxed application is
   found by exactly the same lookup as a native one.
3. **Ask, or open.** With `--choose`, or with more than one candidate and no default, the Open With
   chooser (`kdos-openwith`, a `kdos-shell` surface) is run with the files. With one candidate or a
   default, the first candidate opens. Where the chooser is not installed, the first candidate
   opens.
4. **Run it.** The entry's `Exec` line is split by `kxdg_exec_split()` with field codes
   *substituted* — the code is the document, and dropping it opens the application with nothing —
   and a line with no `%f`, `%F`, `%u` or `%U` gets the documents appended, the same decision the
   panel's launcher makes. At most four documents are passed. The process then replaces itself with
   the application.

With no handler at all, the arguments are passed unchanged to `/usr/bin/xdg-open`; if that is
missing too, the command fails with "nothing on this machine opens <type>".

Every file opened by absolute path is added to the recent-files store under the entry's name
(`kxdg_recent_add()`), since every open on the desktop passes through here. A failed write is
ignored: a convenience list is no reason to refuse to open a file.

**The shipped tables.** Both are in `/etc/xdg`, and a new home has none of its own:
`kdos-mimeapps.list` for types that answer differently on this desktop, and plain `mimeapps.list`
for types that answer the same way on a bare virtual terminal. A type belongs in exactly one of
them. Your own choice in `~/.config` is searched before both, so Open With can always change what
is in force; it writes to `~/.config/mimeapps.list`, and it reads the files in this same order, so
what it shows as current is what the opener would run.

**The MIME database** is compiled on the target. The `shared-mime-info` port ships the source
definitions only, and `kpkg`'s install trigger runs `update-mime-database`, which only works on the
target because the compiler is a target binary.

### Terminal entries

A `Terminal=true` entry is wrapped in the desktop's terminal, which `kb_terminal()` names: `foot`
under a Wayland session.

On a bare virtual terminal it is not wrapped at all. `Ctrl+Alt+F2`, a serial console and an ssh
login have no session for an emulator to open in, and you are already at a terminal, so
`kb_terminal()` answers nothing and the program runs in place.

An entry can ask for a particular emulator with `X-KDOS-Term`, which is honoured whatever the
session uses. Use it for a program that needs pictures drawn in the terminal's cell grid
(`X-KDOS-Term=kdos-term`); see [kdos-term](kdos-term.md). It is a name, not a program: only `kdos-term` and `foot` are accepted, and anything
else falls back to the session's terminal. A desktop entry is a file anything can write, and a key
that named any program would be a second `Exec` line without the field-code rules.

## Box profiles

A box's profile is `~/.config/kdos/boxes/<name>.conf`, flat `key = value` lines, with `#` comments.
It is read from `$HOME/.config` and not from `$XDG_CONFIG_HOME`, because the compositor reads the
same file for the same box and the two must never resolve it differently. `kdos-box create` and
`kdos-box profile <name> key=value` write it; a box with no file behaves exactly like one with the
defaults.

An application box and a development box differ in `base`, which chooses the lane, and in two keys
that only describe the box, `persistence` and `export`. They do not differ in kind, which is what
lets one manager serve both.

| Key | Values | Default | What enforces it |
|---|---|---|---|
| `base` | `pack:<id>`, `image:<ref>` | none (the first launch records the pack) | Which lane creates the box. `box:<name>` is also accepted, but `kdos-box create` does not complete with it; see [kdos-box](#kdos-box) |
| `image` | an image reference | — | The image for an image-lane box |
| `persistence` | `persistent`, `ephemeral`, `frozen` | `persistent` | Nothing: recorded only. See below |
| `network` | `host`, `private`, `none` | `host` | `--network host`; the rootless private namespace; `--network none` |
| `ipc` | `shared`, `private` | `shared` | `--ipc host` when shared |
| `devices` | `shared`, `private` | `shared` | `/dev` and `/sys` bind-mounted when shared |
| `processes` | `shared`, `private` | `shared` | `--pid host` when shared |
| `home` | `shared`, `private` | `shared` | Private gives the box its own home under `~/.local/share/kdos/boxes/<name>` (or `$XDG_DATA_HOME/kdos/boxes/<name>`) |
| `init` | `yes`, `no` | `no` | `--init` on the image lane. A pack box always runs `kdos-boxinit` as its init |
| `wayland` | `yes`, `no` | `yes` | Nothing can take the display away. See below |
| `audio` | `yes`, `no` | `yes` | Follows `devices` |
| `gpu` | `yes`, `no` | `yes` | With private devices, binds `/dev/dri` back into the box |
| `render` | `auto`, `gpu`, `software` | `auto` | Which renderer the box's applications use. See [render](#render) |
| `memory` | a size, such as `4G` | unlimited | `--memory`, enforced by `kdos-oomd` |
| `cpus` | a number | every core | `--cpus` |
| `pids` | a number | unlimited | `--pids-limit` |
| `accent` | an accent name | the session's | The box's colour: its title-bar chip and the palette of `kdos-box enter`'s terminal |
| `autostop` | `90s`, `30m`, `2h`, or seconds | `0` (never) | `kdos-box gc` stops the box once it has been running this long, counted from when it started, and has no window open |
| `grant` | comma-separated: `screencopy`, `data-control`, `input-method` | none | The compositor lets this box's clients use those protocols |
| `export` | `auto`, `manual` | `manual` | Nothing: recorded only. See below |
| `display` | free text | `window` | Nothing: carried through untouched |

For the sharing keys, `private`, `yes`, `on`, `1` and `true` all mean private (or yes); anything else
means shared (or no). An unknown key is reported by name on a `!` line under the profile.

**How the profile reports itself.** `kdos-box profile <name>` prints each key beside the mechanism
that enforces it, and a `!` line under any setting it cannot deliver:

- **`wayland = no`** cannot take a display away. The box shares `$XDG_RUNTIME_DIR`, so a client that
  opens the default `wayland-0` reaches the session's socket anyway. What a launch adds is the
  per-box `kdos-boxsock` socket, whose tag is what the compositor's sandbox allowlist filters on,
  and that is handed over whatever the key says.
- **`export = auto`** triggers nothing. `genlaunchers` writes launchers for every installed pack and
  every store box at once, and `kdos-box export <box> <app>` is the per-application route.
- **`persistence = ephemeral` or `frozen`** is remembered, not imposed. Every launch keeps the
  writable layer on disk, and on the pack lane `kdos-packd` decides from the pack itself.
- **`audio = no` or `gpu = no` with `devices = shared`** cannot be enforced separately. A shared
  `/dev` cannot have a hole cut in it, and no container flag grants a speaker while denying a camera.
  `gpu` is enforceable in one direction only: with `devices = private` the box has no `/dev`, and
  `gpu = yes` binds `/dev/dri` back.

`display` means nothing to a container flag and nothing on this desktop reads it, but every writer of
the file carries it through a rewrite unchanged. A profile writer that kept only the keys it knew
would delete everybody else's, and a setting that disappears when an unrelated one changes is worse
than no setting.

**Namespaces apply at create time.** A namespace or a volume cannot be changed on a running
container, so `kdos-box profile <name> key=value` writes the file and then tells you to
`kdos-box remove <name>` and create it again for those keys to take effect.

### render

`render` decides whether a box's applications draw with the graphics card or on the processor. The
card is the default: the render nodes are in every box, and the drivers and `libva` are in the base
pack, so a box drawing with llvmpipe on a machine with a working card is paying for nothing.

| Value | Resolves to |
|---|---|
| `auto` (and an absent key) | The card if a `/dev/dri/renderD*` node opens for you, otherwise software |
| `gpu` | The same as `auto`: a request, not a guarantee |
| `software` (also `pixman`, `no`, `off`, `0`, `false`) | Software, whatever is plugged in |

The node is *opened*, not just looked for: it is owned by the `render` group, and a node you cannot
open is the same dead end as no card. Under QEMU, virtio-gpu publishes a render node only with
virgl, so `make run` resolves to software.

The software answer becomes `LIBGL_ALWAYS_SOFTWARE=1` in the launch environment. Nothing is set for
the hardware answer, because Mesa already loads the right driver and falls back by itself, and a
variable pinning hardware would take that fallback away. The variable governs GL and EGL only;
VA-API and Vulkan find the render node on their own, and denying those is the `gpu` key's job. It is
advisory — an application may unset it — and `kdos-box profile` says so.

A box with `devices = private` and `gpu = no` has no `/dev/dri` inside it, so the profile answers
software without probing: the host's node is a path that does not exist in the box.

```
render      = auto        /dev/dri/renderD128
render      = software    LIBGL_ALWAYS_SOFTWARE=1 (advisory) — the profile refuses the card
render      = auto        LIBGL_ALWAYS_SOFTWARE=1 (advisory) — no render node on this machine
render      = auto        LIBGL_ALWAYS_SOFTWARE=1 (advisory) — no /dev/dri inside this box
```

### memory and grants

`memory` is passed to the container engine, but rootless containers on a machine with no cgroup
delegation accept a memory limit and ignore it. `kdos-oomd` is what makes the key real: it reads the
profiles and, under memory pressure, picks a box that is over its own declared budget first, ahead
of its general preference for boxed processes.

`grant = screencopy, data-control` opens protocols the compositor's sandbox allowlist otherwise
refuses to a box's clients. The compositor reads the box's profile once per client and caches the
answer; reloading the compositor drops the cache. Each name covers both generations of its protocol.
`input-method` has to be named explicitly because an input method sees every key typed.

`base = image:<ref>` reaches the network and says so before it does anything: it fetches unsigned
content from a registry, and the strict signature setting does not cover it. `pack:` is the offline
base.

## kdos-box

`kdos-box` is the same binary under a second name, managing boxes directly. A box name, and a
snapshot tag, is 1 to 63 characters from `A-Z a-z 0-9 . _ -` and does not start with `.` or `-`.

| Command | Does |
|---|---|
| `list`, `ls` | Every box with its base, state, persistence, disk use and accent, including boxes that have a profile but no container |
| `create <name> [key=value ...]` | Writes the profile and creates the box. A base is required: `base=pack:<id>` or `base=image:<ref>` |
| `enter <name> [command ...]` | Starts the box and opens a `foot` terminal titled `<name> — KDOS box`, in the box's accent colour, running a login shell inside. From a terminal, or with a command, it runs in place instead |
| `run <name> <app> [args ...]` | The launch path, in this box |
| `apps <name>` | The desktop ids inside the box |
| `export <name> <app>` | A launcher `~/.local/share/applications/<app>.<name>.desktop` named `<app> (<name>)`, and a shim `~/.local/bin/<app>@<name>` |
| `unexport <name> <app>` | Removes both |
| `freeze <name> [out.kpack]` | The writable layer as one pack. See below |
| `import <file.kpack> [as <name>]` | Installs a pack through `kdos-packd`, and with `as` creates a box on it |
| `clone <src> <dst>` | A new box on the same base, with a copy of the source's writable layer |
| `snapshot <name> [tag]` | Copies the writable layer to `snapshots/<tag>`. The default tag is the UTC time, `YYYYMMDD-HHMMSS` |
| `snapshots <name>` | Lists them with their sizes |
| `rollback <name> <tag>` | Replaces the writable layer with a snapshot. Refused while the box is running |
| `start`, `stop`, `restart <name>` | Stop waits up to 10 seconds before killing |
| `remove <name> [--force]` | Removes the container and releases its packs. The profile and the writable layer stay in `~/.local/share/kdos/boxes/<name>` |
| `profile <name> [key=value ...]` | Prints the profile, or sets keys and then prints it |
| `gc [--dry-run]` | Stops idle boxes. See [Warmup and collection](#warmup-and-collection) |

`create` and the commands a person runs by hand let the container engine print its errors, since
its message is the diagnosis. A create that fails on a live session says why: an overlay's writable
layer cannot sit on overlayfs, which is what `$HOME` is on a booted ISO, so a persistent box needs an
installed system.

`enter` sets `KDOS_BOX=<name>` inside the box, which a prompt such as starship can show. Set
`KDOS_BOX_NOTERM` (to any value, even `0`) to make `enter` run in the current terminal instead of
opening a window; only whether the variable exists is checked.

**`create` with `base=box:<name>` does not work.** The command is meant to make a box on another
box's software with an empty writable layer, but it never completes: it repeats its own base lookup
until the program crashes. Use `kdos-box clone` instead, which copies the software and the work, or
read the other box's base with `kdos-box profile <name>` and pass that base to `create`.

### freeze, import and clone

`freeze` packs the box's writable layer — only what you changed — into one pack with `kdos-pack
build`, with the box's `pack:` base recorded as a requirement so an import knows what it sits on.
The pack id is `box.<name>` and its version is the time of the freeze. The result is a difference,
and it can be compared against a previous freeze like any other pack. Measured on a development box
with real work in it: 2.1 MB against a 432 MB merged root.

A pack box is created over an exploded root, so the container engine has no image to commit it to.
Freezing is the way to capture a pack box's state. Sign the result with
`kdos-pack sign <file> <key>` and bring it back with `kdos-box import`.

`import` copies the pack into the daemon's staging directory and asks the daemon to install it,
because verification happens where the mount happens.

`clone` copies the software *and* the work. To start from the same software with an empty
workspace, read the source box's `base` with `kdos-box profile <src>` and create the new box on it.

### Snapshots and rollback

A snapshot is a full copy of the writable layer, so on an ordinary filesystem it costs as much space
as the box has written. It is deliberately not a pack: writing a pack back into a writable layer
needs it mounted, and a rollback that needed the daemon would fail exactly when a box is broken.

### export

A secondary box's application gets a box-qualified desktop id and shim, while the default box keeps
upstream's own id. That refines the launcher-naming rule rather than breaking it: the rule exists
because the panel matches a window to an entry, and for a box's windows the panel has a better key
than the file name — the box named in the window's security context.

## Warmup and collection

At login the session runs `nice -n 10 kdos-appbox warmup` in the background. With one box per
application, starting a single shared box would help nothing, so the warmup reads your favourites,
`~/.config/kdos/favorites` (one desktop id per line), and starts the box behind each, up to eight.

For each favourite it reads the desktop entry — from `~/.local/share/applications`, then
`/usr/share/applications` — and takes the pack from the `Exec` line: `-b <pack>` where it is named,
otherwise the command after `run`, resolved as `run` resolves it. A hand-written entry naming a shim
is looked up in the table by that name. A lock
(`$XDG_RUNTIME_DIR/kdos-appbox.warmup.lock`) makes a second warmup a no-op rather than a queue.

The session also runs `kdos-box gc` every ten minutes at `nice -n 10`. It stops only boxes whose
profile sets `autostop`, and the default is never. The time is counted from when the box started,
not from when its last window closed. A box past its time is stopped at the next run, but only after
asking the compositor (`kdos hey boxes`) whether the box has a window open. A box with a window is
left alone, and so is every box when there is no session to ask — "cannot tell" is not "no window".

Boxes that a launch or the warmup creates get a default profile with no `autostop`, so with the
defaults a warmed box stays running for the rest of the session. To have warmed boxes given back
when idle, set `autostop` on your favourites' boxes, for example
`kdos-box profile app.gimp autostop=30m`.

## Storage drivers

This governs the container engine's own store: the store lane's images and containers, and
development boxes with an `image:` base. Pack boxes are composed by `kdos-packd` and are not in it.

| Situation | Driver |
|---|---|
| A live session | `fuse-overlayfs`, pinned in `/etc/containers/storage.conf` |
| An installed system whose home is on ext2, ext3, ext4, btrfs, xfs or f2fs | The kernel's native overlay |

A live session needs the userspace driver because the home directory sits on the boot overlay, and
the kernel refuses to stack an overlay's writable layer on an overlay. The engine does not fall back:
the container simply fails to mount.

On an installed system the native driver is much faster, so the first run writes
`~/.config/containers/storage.conf` with `driver = "overlay"` — but only when that file does not
exist and the store holds no containers yet. The two drivers write incompatible deletion markers into
container layers, so the choice must never change once a container exists. To change it, remove every
container first.

## Files and variables

| Path | What |
|---|---|
| `/usr/share/kdos/appstore/catalogue` | The catalogue |
| `/usr/share/kdos/alien-apps`, `~/.local/share/kdos/alien-apps` | The name-to-command tables, system and yours |
| `~/.config/kdos/boxes/<name>.conf` | Box profiles |
| `~/.local/share/kdos/boxes/<name>/` | A box's writable layer (`upper`), snapshots, and private home |
| `~/.config/kdos/favorites` | What the warmup starts |
| `~/.config/kdos/a11y` | Opts boxes in to accessibility |
| `~/.config/containers/storage.conf` | The storage driver choice |
| `$XDG_RUNTIME_DIR/kdos-appbox.trace` | Launch stage timings |
| `/run/kdos-packd.sock` | The pack daemon |
| `/var/lib/kdos/packs` | The pack store, and its `staging` directory |

| Variable | Effect |
|---|---|
| `KDOS_CATALOGUE` | Read this catalogue instead of the shipped one |
| `KDOS_PACKD_SOCKET` | Talk to the pack daemon on this socket |
| `KDOS_PACK_STORE` | The pack store, default `/var/lib/kdos/packs` |
| `KDOS_PACK_KEY` | The key `export` signs its index with |
| `KDOS_A11Y` | `1` opts one launch in to accessibility, `0` out |
| `KDOS_BOX_NOTERM` | Set to any value (even `0`): make `kdos-box enter` run in the current terminal |

## See also

- [Applications](../02-user-guide/applications.md) — using all of this day to day
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the format, the daemon and the container
- [The daemons](daemons.md) — the pack daemon, the memory daemon and `kdos-boxsock`
- [The session](../03-architecture/session.md) — the environment and what is shared
- [The security model](../03-architecture/security-model.md) — what a box is and is not
- [The kdos command](kdos-command.md) — `kdos app`, the everyday front end
