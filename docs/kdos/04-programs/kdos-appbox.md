# kdos-appbox

`kdos-appbox` is the program that runs containerised applications on KDOS: it builds them from the
catalogue, creates and starts the containers they run in, writes the launchers that make them look
native, and opens files and links in whatever handles them. This chapter is its reference. It is
written for anyone who wants to know exactly what happens between a click and a window, who runs a
development box, who administers a machine with boxed software on it, or who is changing the launch
path. If you only want to install and use applications, read
[Applications](../02-user-guide/applications.md) first: it covers the everyday commands, and this
chapter is the detail behind them. The pack format and the pack daemon are described in
[Packs and boxes](../03-architecture/packs-and-boxes.md), which is worth reading before the sections
on packs. How this way of running applications compares with distribution packages, Flatpak and
Snap is in [How KDOS differs](../01-philosophy/how-kdos-differs.md#applications).

One binary answers to three names, dispatching on the name it was started under:

| Name | What it is for |
|---|---|
| `kdos-appbox` | Installing applications from the catalogue, launching them, and writing their launchers |
| `kdos-box` | Managing boxes as objects of their own: create, enter, freeze, snapshot, clone, remove |
| `xdg-open` | Opening a file or a link in whatever handles it, boxed or native |

Any other name is a **shim**: a symbolic link named after an application, pointing at this binary,
so that `hugin shots.pto` runs Hugin in its box exactly as the Start menu would.

Five terms recur. A **box** is a rootless Podman container that one application, or one
development environment, runs in. A **pack** is a signed, read-only filesystem image that a box's
root filesystem is composed from. A **profile** is the small file that records what a box is made
of and what it may share with the host. The **catalogue** is the shipped list of applications KDOS
can build, `/usr/share/kdos/appstore/catalogue`. An **alien app** is a graphical application that
this repository does not compile and that runs in a box; the `alien-apps` table and the
`X-KDOS-Alien` key are named after it. All five are in the
[glossary](../06-reference/glossary.md).

## Quick start

```sh
kdos app install app.hugin            # the everyday command; runs kdos-appbox install
kdos-appbox catalogue                 # everything the catalogue offers, and what is installed
kdos-appbox install app.hugin         # build and install one application
hugin shots.pto                       # every installed application is also a command
kdos-appbox open report.pdf           # open a file in whatever handles its type
kdos-box clone app.hugin work         # a second box on Hugin's base; a pack box's work is copied too
kdos-box enter work                   # a shell inside it
kdos-box profile work                 # what the box is allowed, and how that is enforced
```

## Synopsis

```text
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

### Global options for kdos-appbox

These come before the command word, and apply only under the name `kdos-appbox`.

| Option | Effect |
|---|---|
| `-b NAME`, `--box NAME` | Run in the box `NAME` instead of the application's own box |
| `-v`, `--verbose` | Let the container engine print to standard error. By default a child's standard error is discarded, so a click never sprays container messages at the desktop |
| `-h`, `--help` | Print the usage summary to standard error and exit 0 |

An unknown option exits 1 with a message. With no command, or with a command it does not know,
`kdos-appbox` prints its usage and exits 1. `kdos-box` with no command, an unknown one or one missing
its arguments prints its own usage and exits 2.

## Description

### Two ways an application reaches a box

There are two lanes, and they differ only in how a box's root filesystem is made.

| Lane | How the application arrives | What the box is | Profile `base` |
|---|---|---|---|
| Store | `kdos-appbox install` builds it on this machine from the catalogue, as a stack of container images `kdos/base`, `kdos/rt-gtk`, `kdos/app.hugin` … | A container that `distrobox create` makes over the top image | `image:kdos/<id>` |
| Pack | A signed pack is installed through the pack daemon, `kdos-packd`: by `kdos-appbox import`, by `kdos-box import`, or from a medium that carries packs | A container made by `podman create --rootfs` over an overlay that `kdos-packd` composes from the pack and every pack it requires | `pack:<id>` |

Both lanes share one launch path, one environment, one set of launchers and one profile format.
Once a box exists, every launch goes through the same sequence of `podman start` and `podman exec`
whichever lane created it.

The box for an application is named after it, `app.hugin`, which is also the catalogue id and the
pack id. A box that a launch creates without a profile records its base in a new profile the first
time it is composed, so every later reader knows what it is made of.

### No shell anywhere

Application names, package names and file names all arrive from desktop entries and from command
lines, and a shell between them and the program would turn any of them into a way to run arbitrary
commands. Everything is executed through an argument-vector builder instead. The one shell command
this program runs itself runs *inside* a box, over a fixed string: `kdos-box apps` listing a
directory. The `RUN` line of a generated Containerfile is also shell, interpreted by the image build
over text taken from the shipped catalogue: its package lists, `deb` URLs and asset patterns.

The program is compiled together with the sources of four KDOS libraries, `libkbase`, `libktui`,
`libkcolor` and `libkxdg`, and links no library of anybody else's. Desktop notifications go over the
session bus through `gdbus`, with a two-second call timeout.

### Where it is installed

The port is `src/system/kdos-appbox`.

| Path | What |
|---|---|
| `/usr/local/bin/kdos-appbox` | The binary |
| `/usr/local/bin/kdos-box` | A symbolic link to it |
| `/usr/local/bin/xdg-open` | A symbolic link to it |
| `/usr/share/kdos/appstore/catalogue` | The catalogue it reads |

`/etc/profile` puts `/usr/local/bin` before `/usr/bin` on the shipped `PATH`, which is why this
`xdg-open` answers ahead of the xdg-utils script. The recipe depends on `podman`, `distrobox`,
`shared-mime-info` and `xdg-utils`; the container ports are on the
[`containers` shelf](../06-reference/ports-catalogue.md#containers) in the ports catalogue.

## Commands

### run

```sh
kdos-appbox run hugin shots.pto
kdos-appbox -b scratch run bash
```

Starts a program in its box. See [The launch path](#the-launch-path) for everything that happens
between the command and the window.

Without `-b`, the box is found from the command. The program's key is taken from the argument list:
an `env` prefix and its assignments are skipped, an `sh -c` or `bash -c` wrapper is looked inside,
and the result is reduced to a basename. That key is compared with the same key of each row's
command in the name-to-command table (your table first, then the system one), and the matching row's
third field names the box. A two-field row names no box and never matches. A command that no
installed application carries is refused with a sentence naming it and suggesting
`kdos app list`.

### open

```sh
kdos-appbox open report.pdf
kdos-appbox open --print report.pdf     # resolve and print; do not run anything
kdos-appbox open --choose report.pdf    # always ask which application
```

Resolves a file or a URI to the application that handles it and runs that application. See
[The open path](#the-open-path).

`--print` writes the resolution as tab-separated lines instead of running it:

```text
mime        application/pdf
candidates  okularApplication_pdf.desktop   kdos-peek.desktop
default     yes
entry       /usr/share/applications/okularApplication_pdf.desktop
exec        okular   report.pdf
```

Candidates are desktop ids exactly as the tables write them, `.desktop` suffix included, and a path
argument is passed on as typed, not made absolute. The sample is the shipped machine, where
`/etc/xdg/kdos-mimeapps.list` names Okular's PDF entry for PDF and the default comes first; every
other installed entry that claims the type follows it, and an installed boxed viewer adds its own
id to the candidates.

A `choose` line appears when the chooser would be asked, followed by what a tree without the chooser
would run. With no handler at all there are no `candidates` or `default` lines, `entry` is `-`, and
`exec` names `/usr/bin/xdg-open`.

### catalogue

```sh
kdos-appbox catalogue
kdos-appbox catalogue --groups
kdos-appbox catalogue --selftest
```

With no option it prints one tab-separated line for each `app` and `data` row:

```text
<id>  <name>  <category>  <bytes>  <parent>  <installed|available>  <tagline>
```

The separator is a tab because a tagline contains spaces and every other table in this tree splits
on tab. `<parent>` is `-` for a row with none. `--groups` prints one line per named group instead:
`<id>`, tab, `<description>`, tab, then the member ids separated by spaces.

This command is the only place that decides what is installed. The store surface in `kdos-shell` and
`kdos app` both ask it, so they cannot disagree. `kinstall` compiles the same catalogue reader into
itself to list the groups, and does not ask what is installed. A container that the engine lists
under a catalogue id *is* that application, because nothing else could be launched, and a machine
with no container engine reports everything as `available`.

`--selftest` runs the catalogue parser's own assertions against the file named by
`$KDOS_CATALOGUE`. It needs no daemon and no display, and it is checked before anything that does,
so it runs in a build container. `testing/selftest.sh` runs it against
`testing/fixtures/catalogue/catalogue` and then reads the shipped catalogue with and without
`--groups`. See [Testing](../05-developer/testing.md).

The shipped catalogue carries 73 `app` rows and 2 `data` rows, built on 2 base rows (`alpine` and
`base`) and 7 runtimes (`rt-gtk`, `rt-qt`, `rt-kde`, `rt-media`, `rt-sci`, `rt-electron`,
`rt-wine`), and groups them into 7 named groups: `essential`, `office`, `creative`, `dev`,
`science`, `make` and `games`. Every runtime sits on `base` except `rt-kde`, which sits on `rt-qt`.

### install and uninstall

```sh
kdos-appbox install <id|group>... [--dry-run]
kdos-appbox uninstall <id|group>...
```

An argument is a catalogue id (`app.hugin`) or a group name (`creative`); a group expands to its
members and duplicates are dropped. An argument that is neither, or that names a base or runtime
row, stops the command before anything is built.

Install builds each application's chain bottom-up as a stack of images, one per catalogue row, each
`FROM` the one below: `kdos/base`, then `kdos/rt-gtk`, then `kdos/app.hugin`. A second GTK
application is therefore one apt pass rather than three, because the base and the runtime images
already exist and are skipped. The image build context is `/var/empty`, since nothing is copied in.

An image that exists is never rebuilt. `podman image exists` is the whole check: the catalogue
carries no version per row, so an image is current until it is removed, and an existing image prints
`kdos/<id> is already here`. The generated Containerfile has an `ENV DEBIAN_FRONTEND=noninteractive`
and a single `RUN`, so each image adds exactly one content layer over its parent. A row with no
packages gets no `RUN` at all, which is what lets a non-Debian base such as `alpine` exist, and a
package list containing `:i386` enables that architecture first.

A data row an application needs comes with it: a `needs <app> <data>` row makes installing the
application build the data set too, as a box of its own, unless its image already exists. The
shipped catalogue has no `needs` row. `--dry-run` prints the generated Containerfiles instead of
building. It tracks what the same run has already covered, so a preview of two GTK applications
shows the runtime once, as a real install would build it.

Nothing rolls back. Six applications where the fourth fails to build leaves the other five's images
built and names the fourth: what one application built is not harmed by a later one failing, and
undoing it would throw away a long apt run. Progress is one flushed line per step (`==> building kdos/app.hugin`,
`==> app.hugin installed`), so a program showing install progress can read standard output without
parsing the container engine. Such a program should start `kdos-appbox` itself and read its output
directly. Reading it through the output of a service that `ksvc`, the service supervisor (see
[The daemons](daemons.md)), runs never ends, because the supervisor keeps the pipe open.

When the images are built, install creates the box with `kdos-box create <id>
base=image:kdos/<id>`. Like every `image:` create, that first runs `podman pull` on the reference
(see [kdos-box](#kdos-box)), and that pull fails. `podman pull` always contacts a registry: it
resolves the short name `kdos/<id>` to the image just built, `localhost/kdos/<id>`, and asks a
registry at `localhost` for it, where none answers. `create` therefore stops with `could not fetch
kdos/<id>`, and install prints `<id>: the image built and the box did not`, counts the application
as failed and does not regenerate the launchers for it. The images stay built and no box or launcher
is made, so a store install does not reach the Start menu as shipped. Where a box is created, install
regenerates your launchers before the command returns. The regeneration extracts `usr/share/applications` from the
image of every container whose name is a catalogue `app` row, and runs the
`genlaunchers --packs-dir` form over them into your tree. If no such container yields entries, it
falls back to `genlaunchers --packs --user`, which reads the installed packs instead; the two sources
are not merged. The extraction reads the image `kdos/<id>`, not the container, so a pack box whose
application was never built here yields nothing from it. On a machine holding both kinds, where at
least one store box yields entries, the regeneration therefore writes launchers for the store boxes
only, and the pack applications' launchers, table rows and shims are swept. Running
`kdos-appbox genlaunchers --packs --user` by hand restores them and sweeps the store boxes'
instead, because every run writes the whole set. The exit status is the number of applications
that failed.

`uninstall` removes the box with `kdos-box remove`, then the application's own image, then each
runtime above the base, top down, stopping at the first one that a remaining container's chain still
names. It then regenerates the launchers from what remains. The base image is never removed: it is
every chain's floor. Which runtimes are still wanted is asked of the catalogue rather than of the
engine, because a dangling-image sweep cannot tell a runtime nothing uses from one whose only
application is mid-install.

#### The Debian snapshot

The catalogue's `snapshot` line pins the Debian archive the packages come
from. `snapshot = auto`, which is what ships, runs `debian:trixie-slim` once per process and reads
the date out of its `/etc/apt/sources.list.d/debian.sources`, so the packages installed on top cannot
disagree with the root filesystem under them. The date is taken from a comment line of this form in
that file:

```text
# https://snapshot.debian.org/archive/debian/20260824T000000Z
```

A literal date such as `20260824T000000Z` pins explicitly, and `off` reads the live archive. With a
pin, each `RUN` writes its own `debian.sources` naming `http://snapshot.debian.org/archive/debian/`
and `…/debian-security/` at that date, and turns off `Acquire::Check-Valid-Until`, because a
snapshot's release file is stale by definition. The URL is plain HTTP because the base row is the
one that installs `ca-certificates`; the archive's integrity comes from its signature, not the
transport.

If the comment line is missing or has a different form, `auto` cannot be resolved: the install warns
that it is building against the live archive and carries on unpinned rather than refusing to
install. That warning is the only way to tell an unpinned build from a pinned one afterwards.

### export and import

```sh
kdos-appbox export <file.ktar> <id|group>...
kdos-appbox import <file.ktar> [<id>...]
```

An export is a set of installed applications as packs in one file, for a machine with no network
or for keeping a known-good set. `export` needs a selection and skips, with a message, any id whose
image is not built here. `import` does not need one, because the archive carries its own; naming ids
narrows it.

The file is a plain tar archive:

```text
apps.ktar
  SELECTION        what was picked, as text
  PACKAGES         the pack index: id, version, kind, size, sha256 and file name per pack
  PACKAGES.sig     present when a signing key was readable
  app.hugin.kpack
  app.scribus.kpack
```

Each pack is made by creating a throwaway container from `kdos/<id>`, flattening it with
`podman export`, and packing the tree with `kdos-pack build`. Its metadata carries the row's id and
kind, the catalogue name and tagline, and the export time as its version. The index is written by
`kdos-pack index`, with `--sign $KDOS_PACK_KEY` when that file exists.

Four choices shape the format:

- **Packs, not a container-engine save.** A store install is unchecked content from a package
  archive; a pack is checked by `kdos-packd` when it is installed into the pack store, against the
  payload hash in its own footer. An imported application therefore arrives intact or not at all,
  where a store-built one is taken as the image build left it.
- **`podman export`, not the overlay store.** An image's content is spread across its layers, and
  only an export flattens them. It also keeps overlay whiteouts (the files overlayfs uses to mark a
  deletion) and `trusted.overlay.*` extended attributes out of play, which is what lets this run
  without root.
- **`kdos-pack build`, not `mkfs.erofs` directly.** The build command already carries the
  reproducible flag set, so there is one answer to how a pack is made. Its `--force-uid=1000` is
  what lets an export run unprivileged: a box runs with `--userns keep-id`, so the process inside is
  uid 1000, and a tree owned by real root is one it could create nothing in.
- **An unsigned index says so.** With no readable `KDOS_PACK_KEY` the set still indexes and imports,
  but the export prints that the index is unsigned.

What the signature covers is narrower than the index suggests. Import stages each `.kpack` alone
and never the `PACKAGES` index beside it, so the export's index, signed or not, takes no part in
the daemon's check: it checks each pack's payload against the hash in that pack's own footer, at
install, and then its signature block. The packs an export builds carry no signature block of their own, so the daemon
accepts them as unsigned, and a daemon started with `KDOS_REQUIRE_SIG` refuses every one of them.
A pack in the store is not hashed again when it is mounted.

Import unpacks the archive, then stages each selected pack into the pack daemon's staging directory
(`$KDOS_PACK_STORE/staging`, by default `/var/lib/kdos/packs/staging`) and asks the daemon to install
it by file name, never by path. That rule is what stops a request reachable from the `wheel` group
from turning into a mount of an arbitrary device. For each pack the daemon accepts, import creates
the box with `kdos-box create <id> base=pack:<id>`. A pack the daemon refuses is named and skipped;
the rest of the archive still imports, the last line reports how many imported and how many failed,
and the exit status is 1 if any failed. When at least one imported, import regenerates your
launchers the way install does, with the same limit on a machine that also holds store boxes (see
[install and uninstall](#install-and-uninstall)).

`SELECTION` is plain text and can be edited or compared. It opens with `#` comment lines giving the
export date and the catalogue's identity (its row count and byte length). A `group <name>` line
records a group that was picked, and a bare line is an id. Import reads only the bare id lines. The
export writes an id line only for an id named on its command line, not for the members of a picked
group, even though the member packs are in the archive. An archive exported by group name alone
therefore imports nothing unless the ids are named on the `import` command line: a bare `import`
of it reports `0 imported, 0 failed` and exits 0.

### list, apps, warmup, status

| Command | Prints or does |
|---|---|
| `list` | Every container, its image, its state, and `custom` or `default` depending on whether a profile file exists |
| `apps` | Every row of both name-to-command tables: the name, `baked` (the system table) or `user`, then the command and, when the row has one, the pack it runs in |
| `warmup` | Composes and starts the boxes behind your pinned favourites. See [Warmup and collection](#warmup-and-collection) |
| `status` | What the pack daemon reports (or `packd : not answering`), then the state of the box named with `-b`, which defaults to the placeholder name `kdos-apps` |

### store --selftest

`kdos-appbox store --selftest` checks the Containerfile generator and the `SELECTION` writer and
reader offline, against the catalogue named by `$KDOS_CATALOGUE`. Like the catalogue self-test, it
needs no container engine. `testing/selftest.sh` also runs `install essential --dry-run` over the
shipped catalogue with the snapshot pinned.

## The catalogue file

The catalogue is `src/system/kdos-appbox/catalogue` in the tree and ships unchanged. It is read in
one pass, line by line; a `#` starts a comment only as the first non-blank character of a line, so a
tagline may contain one.

| Row | Form | Meaning |
|---|---|---|
| Pack row | `<kind> <id> <parent> <apt packages …>` | `kind` is `base`, `runtime`, `app` or `data`. `parent` is the row this one is built on, `-` for a base. A package list of `-` means none |
| `meta` | `meta <id> <name>\|<category>\|<bytes>\|<tagline>` | What the surfaces show. Optional: a row with none presents as its own id, category `Other`, size 0 and no tagline. A category written as `A;B;` shows as `A` |
| `group` | `group <id> <description>`, then `group <id> <member ids …>` | The first line for a group is its description; later lines list members, recognised by the `app.` or `data.` prefix |
| `snapshot` | `snapshot = auto` \| `<date>` \| `off` | Which Debian archive the packages come from. See [install and uninstall](#install-and-uninstall) |
| `image` | `image <base-id> <ref>` | A base built `FROM` that image instead of `debian:trixie-slim`. `alpine` is `alpine:3.24.1` |
| `env` | `env <id> NAME=VALUE` | A variable meant for every launch of a box whose chain contains this row. The launch path does not export it; see [The environment a box gets](#the-environment-a-box-gets) |
| `needs` | `needs <app-id> <data-id> …` | Data rows installed together with the application |
| `deb` | `deb <id> <releases-api-url> <asset-pattern>` | A package Debian does not carry: the newest release asset matching the pattern is fetched and handed to apt |
| `cmd`, `graft`, `boxgraft` | `cmd <id> <name>`, `graft <id> <from> <to>`, `boxgraft <id> <from> <to>` | Parsed, but no command of this program acts on them |

Pack rows, `group`, `image`, `env`, `cmd`, `needs`, `deb`, `graft` and `boxgraft` rows may appear
in any order: the whole file is read before any chain is resolved, and a parent or a base is looked
up by id wherever it stands. A `meta` row is the exception. It is applied as it is read, so it must
come after the pack row it describes; one placed above that row is dropped without a message, and
the application presents as its own id, category `Other`, size 0 and no tagline.

A chain cannot be closed when a row names a parent that is not in the file at all, or when it runs
past 16 rows, which is also where a cycle ends. The install then stops for that application with
`<id>: no chain — a parent is missing from the catalogue`, where `<id>` is the application's, not
the missing parent's. The file holds at most 512 pack rows and 32 groups.

## The launch path

In order, because every launcher on the system and the login warmup depend on the order:

1. **Choose the storage driver, once.** Every invocation, under any of the three names, checks this
   first; it settles only once per user. See [Storage drivers](#storage-drivers).
2. **Resolve the box.** A generated launcher for a pack application writes
   `Exec=kdos-appbox -b <pack> run <exec>`, so the box is named outright. A shim looks its own name
   up in the name-to-command table and takes the box from the row. Without either (a prompt, an
   entry naming no box) the box is resolved from the command as described under [run](#run).
3. **Say that a box is starting**, if it is not already running: a desktop notification, "Starting
   app", sent in the background so it can never hold up the launch.
4. **Recover a stuck box.** A stopped box is often still *stopping*: the container's init stays
   alive reaping after a stop, and asking the engine to start it then fails with "container state
   improper", which names nothing a person can act on. An application hung in uninterruptible I/O
   can leave a box there for good. The recovery waits up to fifteen seconds, then kills the
   container and waits two more, then removes it (with a "Resetting app container" notification).
   Nothing is lost: a pack box's packs are read-only and its writable layer is on disk, so the
   container is recreated over the same stack. `kdos-box start` shares this recovery.
5. **Create the box, if it does not exist.** This asks `kdos-packd` to compose the stack and runs
   `podman create --rootfs` over it (see [What a pack box is created with](#what-a-pack-box-is-created-with)),
   under a lock (`$XDG_RUNTIME_DIR/kdos-appbox.create.lock`) so a launch and the login warmup cannot
   race to create the same container. A first launch prints `==> First launch: composing '<box>'
   from packs...`. If the daemon is not running, or refuses, the launch stops with a message naming
   which.
6. **Start it, composing first.** A pack box's merged root lives under `$XDG_RUNTIME_DIR`, which is
   a temporary filesystem, so after a reboot the container still exists but the root it was created
   over is gone. Composing is idempotent, because the daemon counts references, so a box that is
   already composed costs one round trip. A store box has no pack base and is started as it is.
7. **Wait for readiness.** The box's init prints `container_setup_done` when the user account inside
   exists: `kdos-boxinit` in a pack box, distrobox's init in a store box. If that line is not yet in
   the container's log, the launch waits for it, for at most 30 seconds. Running a program in a box
   before that point runs it with no user.
8. **Build the environment.** See [The environment a box gets](#the-environment-a-box-gets). This
   includes starting `kdos-boxsock` for the box and waiting up to one second for its socket.
9. **Execute.** `podman exec --interactive --user=<uid>:<gid> --workdir=$HOME <box> env …
   <program>`, with `--tty` added when this process's own input is a terminal. The same command
   therefore gives an interactive prompt at a shell and a plain execution from a launcher. The
   process replaces itself with `podman`.

Stage timings are appended to `$XDG_RUNTIME_DIR/kdos-appbox.trace`, one line per stage with a
seconds-and-microseconds timestamp; `open` writes its decisions there too, so the trace is where to
read how long a launch takes on a given machine. A cold launch, with no container at all, pays for
composing, creating, the box's init and the program's own start, which the program's source records
as about 18 seconds; a launch into a running box skips all of that.

None of those stages is visible to the compositor. What a person sees during a cold start is the
"Starting app" notification and then nothing until the application connects, because there is no
window to map before that.

### What a pack box is created with

`podman create --rootfs <merged-root>` receives this flag set. Each part is there because something
breaks without it.

| Flags | Why |
|---|---|
| `--name <box>`, `--hostname=<box>.<host>`, `--label manager=kdos-box` | Identity |
| `--user root:root --userns keep-id`, `--annotation run.oci.keep_original_groups=1` | Container uid 1000 is host uid 1000, so the files in `$HOME` belong to the box's user and the box runs as a real non-root account; applications that refuse to run as root start |
| `--security-opt label=disable`, `--security-opt apparmor=unconfined` | The host runs neither, and a profile that is not loaded is a denial rather than a policy |
| `--ulimit host`, `--pids-limit` | Host limits; the profile's `pids`, or no limit |
| `--network`, `--ipc`, `--pid` | From the profile. See [Box profiles](#box-profiles) |
| `--memory`, `--cpus` | From the profile, when set |
| `tmpfs` on `/run` and `/run/lock` | A pack is read-only and `/run` must be writable |
| `/tmp`, `$HOME`, `/run/user/<uid>`, `/dev/shm` bound in | The session bus, the Wayland socket and PipeWire live in `/run/user/<uid>`; `/tmp` carries the X11 sockets |
| `/dev` and `/sys` bound in, or `/dev/dri` alone | Shared devices give the box the host's `/dev`; with private devices, `gpu = yes` binds the render nodes back |
| `/run/cups` bound in | When the CUPS socket exists at create time, so a boxed application's own print dialog works |
| `/var/lib/kdos/packs/mnt` bound in read-only | When it exists, so a data pack's grafted links resolve inside the box |
| `/usr/libexec/kdos/kdos-boxinit` bound to `/usr/libexec/kdos-boxinit`, as the entrypoint | The box's `/usr` is Debian's; a static init from the host runs there |
| `KDOS_BOX`, `KDOS_BOX_USER`, `KDOS_BOX_UID`, `KDOS_BOX_GID`, `KDOS_BOX_HOME`, `container=podman` | What `kdos-boxinit` needs to create the user |

The root filesystem path comes last, because `create` treats everything after its first positional
argument as the container's command.

A store box is created by `distrobox create --name <box> --image <ref> --yes`, with
`--unshare-netns`, `--unshare-ipc`, `--unshare-devsys`, `--unshare-process`, `--init` and
`--home <dir>` added as the profile asks, `/run/cups` bound in when its socket exists, and
`/dev/dri` bound in for private devices with `gpu = yes`.

## The environment a box gets

A program run with `podman exec` inherits nothing, neither the container init's environment nor the
caller's, so every variable is set explicitly. The reasons behind each one are in
[The session](../03-architecture/session.md#the-environment-a-box-receives).

| Variable | Value |
|---|---|
| `WAYLAND_DISPLAY` | The box's own tagged socket, `$XDG_RUNTIME_DIR/kdos-box-<box>@<tag>.sock`, when `/usr/bin/kdos-boxsock` exists and provides one within a second; otherwise unset, and the client finds the session's socket |
| `PATH` | `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games`; the games directories are where Debian puts its games |
| `XDG_RUNTIME_DIR` | `/run/user/<uid>` |
| `DBUS_SESSION_BUS_ADDRESS` | `unix:path=/run/user/<uid>/bus` |
| `XDG_SESSION_TYPE` | `wayland` |
| `HOME`, `USER`, `LOGNAME` | Yours |
| `LANG` | Yours if it contains `UTF-8`, otherwise `C.UTF-8` |
| `GSETTINGS_BACKEND` | `keyfile`, because no dconf service is reachable |
| `NO_AT_BRIDGE`, `GTK_A11Y` | `1` and `none`, unless you opt in to accessibility (below) |
| `GTK_USE_PORTAL`, `XDG_CURRENT_DESKTOP` | `1` and `KDOS`, so toolkits use the KDOS file chooser and settings portals |
| `CUPS_SERVER` | `/run/cups/cups.sock`, when that socket exists |
| `QT_IM_MODULE` | `wayland`: input methods reach a box through the compositor. `GTK_IM_MODULE` is left unset, because GTK on Wayland chooses correctly without it |
| Qt theming and anything else a runtime needs | From the pack's metadata (below) |
| `LIBGL_ALWAYS_SOFTWARE` | `1` only when the profile's `render` key resolves to software. See [render](#render) |
| `DISPLAY` | Yours, or `:<n>` from the first `/tmp/.X11-unix/X<n>` socket, so an X11-only application under Xwayland finds the server |

The `<tag>` in the socket name identifies the compositor: the last path component of your
`WAYLAND_DISPLAY`, keeping only letters, digits, `.`, `_` and `-`, at most twelve characters, or
`session` when that leaves nothing. Keyed on the box alone, a second compositor's launch would
connect to a socket served by the first. `kdos-boxsock` derives the same tag from the same variable.

Accessibility is a default, not a policy. The host runs no accessibility bus, so by default
every boxed GTK application is told not to look for one, which is a probe that could only time out.
Create `~/.config/kdos/a11y` (an empty file is enough) to opt in for good, or set `KDOS_A11Y=1` for
one launch; `KDOS_A11Y=0` opts out for one launch. The path resolves from `$HOME/.config`, the same
way the box profiles do.

Qt theming and other per-runtime variables come from the software, not from a list in this
program, so the variable and the package that makes it work cannot drift apart. Every launch exports
the `env =` lines in the metadata of one pack and of every pack it requires, up to eight packs in all.
The nearest pack wins, so an application can override its runtime. The pack is the one named by the
box's `pack:` base, or, for any other box, the installed pack whose id is the box's name. A store
box is normally named after an application no installed pack carries, so it gets none of these
variables.

The launch path therefore applies neither of the other two sources the program holds. The
catalogue's `env <row> NAME=VALUE` rows (`rt-qt` sets `QT_QPA_PLATFORMTHEME=gtk3` and
`QT_STYLE_OVERRIDE=Fusion`, `rt-kde` sets `QT_QPA_PLATFORMTHEME=kde`, `app.heaptrack` sets the
`rt-qt` pair again, `rt-media` points `LD_LIBRARY_PATH` at PipeWire's JACK library, `app.surf`
sets `GDK_BACKEND=x11`) are not exported into a store box, and the `kdos.qt-kde-theme` and
`kdos.qt-gtk-theme` image labels are not read for an image box. Both lookups sit behind the pack lookup in `box_env()` in `main.c`, and every launch
passes that lookup a non-empty id.

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
| `--packs` | Every pack of kind `app` that `kdos-packd` lists as `installed` or `mounted`, mounted through the daemon. Available packs are skipped: mounting one to read it is what installs it |
| `--packs-dir <dir>` | `<dir>/<pack-id>/usr/share/applications` for each subdirectory. The directory name is the box the launchers dispatch to |
| `<desktop-dir>` | One applications directory. Its entries name no box |

A pack or directory with no `usr/share/applications` contributes nothing; such an application is
reached with `kdos-appbox -b <pack> run <command>`.

An extra argument to `--packs` is refused rather than ignored. The `--packs` and two-argument forms
differ by one argument, so passing both a directory and a root would read the directory as the root
and write the whole set underneath it, exit 0, and leave the real table untouched.

A table that cannot be written stops the run with the path that could not be written, so a run
without permission for the tree it targets never reports a launcher set it did not write. The
summary on standard error reads `<n> launchers, <n> command-only, <n> mime types, <n> shims`,
preceded by how many packs were read.

### Four outputs

| Output | Without it |
|---|---|
| A desktop entry per application, marked `X-KDOS-Alien=true` | No launcher |
| `mimeinfo.cache` beside them | No type association is ever consulted, so no boxed application appears in Open With |
| The name-to-command table, `alien-apps` | A shim cannot find what to run |
| A shim per application | The application is not a command |

The MIME cache is written here because this directory is not `/usr/share/applications`, whose cache
`kpkg`'s shared-index trigger writes, and no package install touches it.

The table is tab-separated, `name`, `command` and an optional third field naming the pack, under a
`#` header line. Readers split at the second tab, so a two-field row still parses.

### Two trees

| Tree | Entries | Table | Shims | Written by |
|---|---|---|---|---|
| System | `<fs-root>/etc/skel/.local/share/applications` | `<fs-root>/usr/share/kdos/alien-apps` | `<fs-root>/usr/local/bin`, relative links | The image build, and `--packs <fs-root>` as root |
| User | `~/.local/share/applications` | `~/.local/share/kdos/alien-apps` | `~/.local/bin`, absolute links to `/usr/local/bin/kdos-appbox` | `install`, `uninstall`, `import`, and `--packs --user`, as you |

Every reader looks in your tree first: the Start menu reads your applications directory, the
dispatcher reads your table before the system one, and `/etc/profile.d/10-wayland.sh` puts
`~/.local/bin` on the `PATH`. That is why an install needs no root.

The system tree is reconciled at image-build time, in the
[image phase](../05-developer/how-kdos-is-built.md#packaging-70_image), by
`script/phases/70_image/030_launchers.sh`, which
runs `kdos-appbox genlaunchers --packs-dir "$KDOS_PACK_EXTRACT" /` (default
`/var/tmp/kdos-pack-extract`, created empty when absent) and then fails the build if alien desktop
entries survive a run whose table has no rows. The alien desktop entries, the table and the
`/usr/local/bin` shims are not under `fs/`, so nothing else would remove them, and the image
carries none unless a pack is baked. The step runs before `070_user.sh`, which copies
`/etc/skel` into every home; a skeleton cleaned after that step would leave stale launchers in
`/home/kdos`, which the Start menu reads first.

`genlaunchers` reconciles rather than appends. It removes every `.desktop` carrying
`X-KDOS-Alien=true` and every shim it recognises as its own, then rewrites the table and the MIME
cache whole. One call is therefore the whole clean-up.

A launcher can still outlive its application when the application is removed by a path that does
not run `genlaunchers`, such as a pack removed through `kdos-packd` directly. The Start menu and the
Open With chooser drop such a row at runtime (`sh_box_missing()` in `kdos-shell`) when its box is
neither an installed `.kpack` in the pack store nor a box profile. Absence has to be proven: an
unnamed box, a name containing a slash and a machine with no pack store all count as present,
because hiding an installed application is worse than showing one whose pack has gone.

### What is read

An entry is skipped when it is `NoDisplay=true`, is not `Type=Application`, has no `Name` or no
`Exec`, or carries the `Settings` category without `System`. The `SKIP_*` tables below remove more by
name. A generated entry carries upstream's `Name`, `Icon`, `Categories`, `GenericName`, `MimeType`,
`Keywords` and `Terminal`, a `Comment` naming it an alien application, the rewritten `Exec`, and
`StartupWMClass` (upstream's, or the desktop id when upstream has none).

### Naming rules

- The launcher carries its box. An application belonging to a pack gets
  `Exec=kdos-appbox -b <pack> run <exec>`; one with no pack gets `Exec=kdos-appbox run <exec>`. The
  pack id is also the box name and the stem of the box profile, so anything reading the command line
  learns the box's policy key directly, and `run` skips the table lookup.
- The launcher's file name is upstream's own desktop id, not a KDOS-prefixed name and not the
  window-class field. The panel matches a running window to an entry by the entry's file id, so a
  mismatch shows a second, generic icon beside the pinned one.
- A Wayland app id is not the X11 window class. GIMP's entry declares
  `StartupWMClass=gimp-3.0` while its window announces the app id `gimp`, which is its upstream file
  name. Pinned favourites therefore name upstream ids. `StartupWMClass` is still written, since an
  X11 application under Xwayland matches by it.
- A shim is named after the program its entry runs, through the one function that decides which
  program a command line runs (it skips an `env` prefix and looks inside an `sh -c` wrapper). A
  reverse-DNS entry therefore gets a shim named after the real program. The `RENAME` table wins
  where upstream's name is not the one people use, and a reserved name, or one with characters
  outside `A-Z a-z 0-9 . _ + -`, falls back to the lower-cased desktop id.
- A terminal entry stays one. The generated launcher keeps upstream's `Terminal=` flag, and the
  shell wraps such an entry in a terminal. Written as false, such a program would start with a pipe
  for input and exit on a usage error, with no window and no message.

### The tables

These live in `launchers.c`.

| Table | Holds |
|---|---|
| `COMMANDS` | `wine`, `winecfg`, `winetricks`: programs used as commands, which get a table row and a shim but deliberately no desktop entry, because a launcher for `wine` with no arguments opens nothing. Written only where `usr/bin/<name>` exists three directories above the applications directory being read |
| `ENTRIES` | Applications that ship no desktop entry at all, with the entry written here: `surf`, which then claims `text/html`, `application/xhtml+xml`, `http` and `https`. A browser with no launcher claims no URL scheme, so installing it would change nothing about what opens a link. Written only where the binary is present, by the same rule |
| `RENAME` | 60 upstream ids people do not use, mapped to the names they do: `firefox-esr` → `firefox`, `org.inkscape.Inkscape` → `inkscape`, `codium` → `vscodium`, and 57 more |
| `RESERVED` | Names no shim may take and the sweep must not delete: shell and system tools, `kdos`, `foot`, the native `git`, `gnuplot` and `mpv`, the package tools, several KDOS commands, and this program's own three names |
| `EXEC_EXTRA` | Arguments needed only because the application is containerised, inserted before any field code: VSCodium gets `--no-sandbox --ozone-platform-hint=auto --disable-gpu-compositing` |
| `SKIP_NEEDS_KWIN` | `org.kde.spectacle`, which opens an error dialog on any compositor but KWin. Screenshots on KDOS are `kdos-shot` |
| `SKIP_ROOTLESS_INERT` | GParted, GSmartControl, GNOME Disks, TestDisk, Baobab-as-root and Timeshift: they need raw block devices, which a rootless container cannot give. Use the native recovery tools instead |
| `SKIP_PREFIXES`, `SKIP_BASENAMES` | Entries that are not applications: settings panels, helpers, URL handlers, secondary tools of a suite |
| `X11_FORCING` | `GDK_BACKEND=x11`, `CLUTTER_BACKEND=x11`, `QT_QPA_PLATFORM=xcb`, `SDL_VIDEODRIVER=x11`, `MOZ_ENABLE_WAYLAND=0`, `ELECTRON_OZONE_PLATFORM_HINT=x11`, stripped from an `env` prefix on an `Exec` line. The catalogue's `env` rows, which this table cannot strip, are where an application states that it needs X11 |

Both `COMMANDS` and `ENTRIES` need the binary in the tree being read, as an installed pack has it.
A store install copies only `usr/share/applications` out of the image, so a store
box gets neither the `wine` shims nor the `surf` entry.

`RESERVED` is consulted when shims are swept as well as when they are written. The sweep removes
every symbolic link in the shim directory whose target is relative (the system tree) or is
`/usr/local/bin/kdos-appbox` (your tree), unless its name is reserved. `kdos-box` and `xdg-open` are
such links, and without the reserved check a regeneration would delete `kdos-box` and `xdg-open`,
two of this program's own three names.

### Exec lines

An `Exec=` line is not a whitespace-separated list. It carries the desktop-entry format's quoting,
`Exec="/usr/bin/gsmartcontrol-root"` or `sh -c "wesnoth-1.18 >/dev/null 2>&1"` whose shell argument
must reach the shell in one piece, and it carries field codes such as `%f`, which must disappear
when no file was selected or a media player tries to open a file literally named `%f`.

`kxdg_exec_split()` in `libkxdg` is the single implementation. It removes the quoting, substitutes
the single-file and multiple-file codes, drops the codes that carry no argument, and, when asked to,
keeps every code verbatim for a tool that rewrites a line instead of running it.
`kxdg_exec_quote()` is its inverse, so what the generator writes reads back as the same arguments.
Every launch path goes through it, and the test suite checks both directions against real lines from
the catalogue.

When `genlaunchers` rewrites a line it keeps `%f`, `%F`, `%u` and `%U` and drops every other field
code. A shim run from a terminal expands the field codes to nothing and appends your own arguments
after the command.

## The open path

`xdg-open` on KDOS is this binary. Everything that opens a link says that word (a mail client's
open-link command, a portal, anything reading `$BROWSER`) and means "whatever this machine opens it
with". The xdg-utils script stays installed at `/usr/bin/xdg-open` and is the last resort, reached by
absolute path, because calling it by name would find this binary again and loop.

Resolution runs in four steps:

1. **What the argument is.** `kxdg_mime_for_arg()` types an argument with a scheme as
   `x-scheme-handler/<scheme>` and unwraps `file:` to the path it names; anything else is a path,
   typed from the shared MIME database's glob table (longest matching suffix wins). Deciding by the
   basename alone would type `mailto:a@b.c` by its `*.c` glob and offer a mail address to a C
   editor. The chooser asks the same function, so both sides agree. The first argument's type is
   used for all of them.
2. **Who handles that type.** The search, in order:

   | Order | File | Section |
   |---|---|---|
   | 1 | `~/.config/kdos-mimeapps.list` | Default Applications |
   | 2 | `~/.config/mimeapps.list` | Default Applications |
   | 3 | `~/.config/kdos-mimeapps.list`, then `~/.config/mimeapps.list` | Added Associations |
   | 4 | `/etc/xdg/kdos-mimeapps.list`, then `/etc/xdg/mimeapps.list` | Default Applications |
   | 5 | `applications/mimeinfo.cache` in `$XDG_DATA_HOME`, then each of `$XDG_DATA_DIRS` | MIME Cache |

   `~/.config` is `$XDG_CONFIG_HOME` where that is set, and `$XDG_DATA_DIRS` defaults to
   `/usr/local/share:/usr/share`. The `kdos-` prefix is the first name in `XDG_CURRENT_DESKTOP`,
   lower-cased, as the specification asks. Only entries that are installed count, and each is
   counted once. A match in row 1 or 2 ends the search; the rows after it are consulted only
   without one. A match in rows 1, 2 or 4 is a *default*. The MIME cache in row 5 is the file
   `genlaunchers` writes, so a boxed application is found by exactly the same lookup as a native
   one.
3. **Ask, or open.** With `--choose`, or with more than one candidate and no default, the Open With
   chooser (`kdos-openwith`, a `kdos-shell` surface) replaces this process, given the original
   arguments. Otherwise the first candidate opens. Where the chooser is not installed, the first
   candidate opens.
4. **Run it.** The entry's `Exec` line is split by `kxdg_exec_split()` with field codes
   *substituted*, since the code is the document and dropping it opens the application with
   nothing. A line with no `%f`, `%F`, `%u` or `%U` gets the documents appended, the same decision
   the panel's launcher makes. At most four documents are passed. The process then replaces itself
   with the application.

With no handler at all, the arguments are passed unchanged to `/usr/bin/xdg-open`, which decodes a
`file://` URL itself; if that is missing too, the command fails with "nothing on this machine opens
<type>".

Every file opened by absolute path is added to the recent-files store under the entry's name
(`kxdg_recent_add()`), since every open on the desktop passes through here. A failed write is
ignored: a convenience list is no reason to refuse to open a file.

The shipped defaults are in `/etc/xdg`: `kdos-mimeapps.list` for types that answer differently on
this desktop, and plain `mimeapps.list` for types that answer the same way on a bare virtual
terminal. A type belongs in exactly one of them. A new home's `~/.config/mimeapps.list` is copied
from the skeleton with an empty `[Default Applications]` section, so every default comes from
`/etc/xdg` until you choose one. Your own choice in `~/.config` is searched before both, so Open
With can always change what is in force; it writes to `~/.config/mimeapps.list`, and it reads the
files in this same order, so what it shows as current is what the opener would run.

The MIME database is compiled on the target. The `shared-mime-info` port is built with
`update-mimedb=false`, and the install trigger of `kpkg`, the package manager (see
[Packaging](../03-architecture/packaging.md)), runs `update-mime-database`, which only works on
the target because the compiler is a target binary.

### Terminal entries

A `Terminal=true` entry is wrapped in the desktop's terminal, run as `<terminal> -e <command>`. The
terminal is what `kb_terminal()` names: `foot` whenever `WAYLAND_DISPLAY` is set.

On a bare virtual terminal it is not wrapped at all. `Ctrl+Alt+F2`, a serial console and an ssh
login have no session for an emulator to open in, and you are already at a terminal, so
`kb_terminal()` answers nothing and the program runs in place.

A `Terminal=true` entry started through GLib's GIO, rather than through this program, is wrapped by
GLib itself, with the first of `xdg-terminal-exec`, `gnome-terminal`, `mate-terminal`,
`xfce4-terminal` and further names that it finds on the `PATH`. That is the path
`xdg-desktop-portal` takes when a boxed application opens a link. None of the other names exists on
KDOS, so `/usr/local/bin/xdg-terminal-exec`, a shell script shipped under `fs/`, answers it: it
drops a leading `-e` or `--`, because some callers pass one and a terminal given `-e` twice tries to
run it, and runs `foot -e` with the rest. It must name the same terminal as `kb_terminal()`, or a
handler resolved correctly is wrapped in a terminal that cannot open and appears to be the wrong
handler. Unlike `kb_terminal()` it does not look at `WAYLAND_DISPLAY`, so it runs `foot` on a bare
virtual terminal too, where `foot` has no session to open in.

An entry can ask for a particular emulator with `X-KDOS-Term`, which is honoured whatever the
session uses, including on a bare virtual terminal. Use it for a program that needs pictures drawn
in the terminal's cell grid (`X-KDOS-Term=kdos-term`); see [kdos-term](kdos-term.md). It is a name,
not a program: only `kdos-term` and `foot` are accepted, and anything else falls back to the
session's terminal. A desktop entry is a file anything can write, and a key that named any program
would be a second `Exec` line without the field-code rules.

## Box profiles

A box's profile is `~/.config/kdos/boxes/<name>.conf`: flat `key = value` lines, with `#` comments.
It is read from `$HOME/.config` and not from `$XDG_CONFIG_HOME`, because the compositor reads the
same file for the same box and the two must never resolve it differently. `kdos-box create` and
`kdos-box profile <name> key=value` write it, atomically, with every known key spelled out; a box
with no file behaves exactly like one with the defaults.

An application box and a development box differ in `base`, which chooses the lane, and in two keys
that only describe the box, `persistence` and `export`. They do not differ in kind, which is what
lets one manager serve both.

| Key | Values | Default | What enforces it |
|---|---|---|---|
| `base` | `pack:<id>`, `image:<ref>` | none (the first compose records the pack) | Which lane creates the box. `box:<name>` is also accepted by the parser, but `kdos-box create` does not complete with it; see [kdos-box](#kdos-box) |
| `image` | an image reference | none | Shown by `kdos-box list` for a box with no `base`. No launch consults it. `create` with an `image:` base takes the reference from `base`, not from this key |
| `persistence` | `persistent`, `ephemeral`, `frozen` | `persistent` | Nothing: recorded only. See below |
| `network` | `host`, `private`, `none` | `host` | Pack lane: `--network host`; no flag, which is the rootless private namespace; `--network none`. Store lane: `private` and `none` both give `--unshare-netns` |
| `ipc` | `shared`, `private` | `shared` | `--ipc host` when shared; store lane `--unshare-ipc` when private |
| `devices` | `shared`, `private` | `shared` | `/dev` and `/sys` bound in when shared; store lane `--unshare-devsys` when private |
| `processes` | `shared`, `private` | `shared` | `--pid host` when shared; store lane `--unshare-process` when private |
| `home` | `shared`, `private` | `shared` | Private gives the box its own directory, `~/.local/share/kdos/boxes/<name>` (or `$XDG_DATA_HOME/kdos/boxes/<name>`). The store lane passes it to `distrobox --home`; the pack lane binds that directory at its own path and does not bind your home |
| `init` | `yes`, `no` | `no` | `--init` on the store lane. A pack box always runs `kdos-boxinit` as its init |
| `wayland` | `yes`, `no` | `yes` | Nothing can take the display away. See below |
| `audio` | `yes`, `no` | `yes` | Nothing: sound reaches every box through the PipeWire socket in `/run/user/<uid>`, which every box shares. See below |
| `gpu` | `yes`, `no` | `yes` | With private devices, binds `/dev/dri` back into the box |
| `render` | `auto`, `gpu`, `software` | `auto` | Which renderer the box's applications use. See [render](#render) |
| `memory` | a size, such as `4G` | unlimited | `--memory` on the pack lane; a cgroup limit only in an autologin session, and a victim preference for `kdos-oomd` everywhere. See [memory and grants](#memory-and-grants) |
| `cpus` | a number | every core | `--cpus` on the pack lane |
| `pids` | a number | unlimited | `--pids-limit` on the pack lane |
| `accent` | an accent name | the session's | The box's colour: its title-bar chip and the palette of the terminal `kdos-box enter` opens |
| `autostop` | `90s`, `30m`, `2h`, or seconds | `0` (never) | `kdos-box gc` stops the box once it has been running this long, counted from when it started, and has no window open |
| `grant` | comma- or space-separated protocol names | none | The compositor lets this box's clients bind those protocols. See [memory and grants](#memory-and-grants) |
| `export` | `auto`, `manual` | `manual` | Nothing: recorded only. See below |
| `display` | free text | `window` | Nothing: carried through untouched |

The store lane's `distrobox create` receives none of `memory`, `cpus` and `pids`, and gives
`network = none` the same private namespace, with an interface, as `private`. `kdos-box profile`
prints the pack-lane flag for these keys (`--memory`, `--network none`) whichever lane the box is
on.

For the sharing keys, `private`, `yes`, `on`, `1` and `true` all mean private (or yes); anything else
means shared (or no). For `wayland`, `audio` and `gpu`, `shared` also means yes. An unknown key is
reported by name on a `!` line under the profile; it is not written back when the profile is
rewritten.

### How the profile reports itself

`kdos-box profile <name>` prints each key beside the mechanism that enforces it, and a `!` line
under any setting it cannot deliver:

- `wayland = no` cannot take a display away. The box shares `$XDG_RUNTIME_DIR`, so a client that
  opens the default `wayland-0` reaches the session's socket anyway. What a launch adds is the
  per-box `kdos-boxsock` socket, whose tag is what the compositor's sandbox allowlist filters on,
  and that is handed over whatever the key says.
- `export = auto` triggers nothing. `genlaunchers` writes launchers for every installed pack and
  every store box at once, and `kdos-box export <box> <app>` is the per-application route.
- `persistence = ephemeral` or `frozen` is remembered, not imposed. Every launch keeps the
  writable layer where its runtime puts it, and on the pack lane `kdos-packd` decides: on disk when
  your home can hold an overlay's writable layer, on a temporary filesystem when it cannot.
- `audio = no` or `gpu = no` with `devices = shared` cannot be enforced separately. A shared
  `/dev` cannot have a hole cut in it, and no container flag grants a speaker while denying a camera.
  `gpu` is enforceable in one direction only: with `devices = private` the box has no `/dev`, and
  `gpu = yes` binds `/dev/dri` back.
- `audio = no` is never enforced, whatever `devices` says, because PipeWire is reached through its
  socket in the shared runtime directory rather than through a device node. The `!` line appears
  only while `devices` is shared; with `devices = private`, `audio = no` is accepted without a
  warning and changes nothing.

`display` means nothing to a container flag and nothing on this desktop reads it, but the profile
writer carries it through a rewrite unchanged. A profile writer that kept only the keys it acted on
would delete everybody else's, and a setting that disappears when an unrelated one changes is worse
than no setting.

Namespaces and volumes apply at create time. A namespace or a volume cannot be changed on a running
container, so `kdos-box profile <name> key=value` writes the file and then tells you to
`kdos-box remove <name>` and create it again for those keys to take effect.

### render

`render` decides whether a box's applications draw with the graphics card or on the processor. The
card is the default: the render nodes are in every box that shares devices, and the drivers and
`libva` are in the base pack, so a box drawing with llvmpipe, Mesa's software renderer, on a machine
with a working card is paying for nothing.

| Value | Resolves to |
|---|---|
| `auto` (and an absent key) | The card if a `/dev/dri/renderD*` node opens for you, otherwise software |
| `gpu` | The same as `auto`: a request, not a guarantee |
| `software` (also `pixman`, `no`, `off`, `0`, `false`) | Software, whatever is plugged in |

The node is *opened*, not only looked for: it is owned by the `render` group, and a node you cannot
open is the same dead end as no card. Under QEMU, virtio-gpu publishes a render node only with
virgl, QEMU's GL passthrough, so `make run` resolves to software.

The software answer becomes `LIBGL_ALWAYS_SOFTWARE=1` in the launch environment. Nothing is set for
the hardware answer, because Mesa already loads the right driver and falls back by itself, and a
variable pinning hardware would take that fallback away. The variable governs GL and EGL only;
VA-API and Vulkan find the render node on their own, and denying those is the `gpu` key's job. It is
advisory, since an application may unset it, and `kdos-box profile` says so.

A box with `devices = private` and `gpu = no` has no `/dev/dri` inside it, so the profile answers
software without probing: the host's node is a path that does not exist in the box.

```text
render      = auto        /dev/dri/renderD128
render      = software    LIBGL_ALWAYS_SOFTWARE=1 (advisory) — the profile refuses the card
render      = auto        LIBGL_ALWAYS_SOFTWARE=1 (advisory) — no render node on this machine
render      = auto        LIBGL_ALWAYS_SOFTWARE=1 (advisory) — no /dev/dri inside this box
```

### memory and grants

`memory` and `cpus` are passed to the container engine, and whether the kernel enforces them
depends on how the session started. Rootless podman creates a container's cgroup as a sibling of
the cgroup it is called from. `kdos-getty` moves an autologin session into the account's delegated
cgroup, `/sys/fs/cgroup/user.slice/user-<uid>/session`, so a pack box created from that session gets
its own cgroup beside it with `memory.max` set. A session started any other way (a password login
on a virtual terminal, an ssh login) stays in the root cgroup, where the engine accepts the limit
and ignores it. In every case `kdos-oomd` reads the profiles and, under memory pressure, picks a
box that is over its own declared budget first, ahead of its general preference for boxed
processes.

`grant` opens protocols that the compositor's sandbox allowlist otherwise refuses to a box's
clients. A client tagged through `kdos-boxsock` gets surfaces, the seat, dmabuf, text input and the
primary selection, and nothing else unless its box's profile grants it. The names are short, and
some cover both generations of a protocol:

| Name | Unlocks |
|---|---|
| `screencopy` | `zwlr_screencopy_manager_v1`, `ext_image_copy_capture_manager_v1`, `ext_output_image_capture_source_manager_v1` |
| `toplevel-capture` | `ext_foreign_toplevel_image_capture_source_manager_v1` |
| `export-dmabuf` | `zwlr_export_dmabuf_manager_v1` |
| `data-control` | `zwlr_data_control_manager_v1`, `ext_data_control_manager_v1` |
| `foreign-toplevel` | `zwlr_foreign_toplevel_manager_v1`, `ext_foreign_toplevel_list_v1` |
| `layer-shell` | `zwlr_layer_shell_v1` |
| `input-method` | `zwp_input_method_manager_v2`, `zwp_virtual_keyboard_manager_v1` |
| `output-power` | `zwlr_output_power_manager_v1` |

A name not in this table grants nothing. The compositor reads a box's `grant` line the first time one
of its clients reaches a filtered global and caches the answer per box; reloading the compositor
drops the cache, so an edit takes effect for the next client while a running one keeps what it has
bound. `input-method` is never implied by another name, because an input method sees every key
typed. The map is in `src/desktop/kdos-comp/src/kdos-grant.c`; see [kdos-comp](kdos-comp.md).

## kdos-box

`kdos-box` is the same binary under a second name, managing boxes directly. A box name, and a
snapshot tag, is 1 to 63 characters from `A-Z a-z 0-9 . _ -` and does not start with `.` or `-`.

| Command | Does |
|---|---|
| `list`, `ls` | Every box with its base, state, persistence, disk use of `~/.local/share/kdos/boxes/<name>` and accent, including boxes that have a profile but no container (state `not created`) |
| `create <name> [key=value ...]` | Writes the profile and creates the box, then prints `<name> created` and the profile. A base is required: `base=pack:<id>` or `base=image:<ref>`. Fails if a container of that name exists |
| `enter <name> [command ...]` | Starts the box and runs a login shell (`/bin/bash -l`) or the command inside it. See below for when it opens a window |
| `run <name> <app> [args ...]` | The launch path, in this box |
| `apps <name>` | Starts the box and lists the desktop ids in its `/usr/share/applications` |
| `export <name> <app>` | A launcher `~/.local/share/applications/<app>.<name>.desktop` named `<app> (<name>)` whose command is `kdos-box run <name> <app> %U`, and a shim `~/.local/bin/<app>@<name>` |
| `unexport <name> <app>` | Removes both |
| `freeze <name> [out.kpack]` | The writable layer as one pack. See below |
| `import <file.kpack> [as <name>]` | Installs a pack through `kdos-packd`, and with `as` creates a box on it |
| `clone <src> <dst>` | A new box on the same base, with a copy of the source's writable layer |
| `snapshot <name> [tag]` | Copies the writable layer to `snapshots/<tag>`. The default tag is the UTC time, `YYYYMMDD-HHMMSS`; an existing tag is refused |
| `snapshots <name>` | Lists them with their sizes |
| `rollback <name> <tag>` | Replaces the writable layer with a snapshot. Refused while the box is running |
| `start`, `stop`, `restart <name>` | Start recovers a stuck box and composes a pack box first, as the launch path does. Stop waits up to 10 seconds before killing |
| `remove <name> [--force]` | Removes the container and releases its composed stack. The profile (`~/.config/kdos/boxes/<name>.conf`) is kept, and so is `~/.local/share/kdos/boxes/<name>`: a pack box's writable layer, its snapshots and a private home. A store box's writes are inside its container and go with it. The confirmation line names only the second directory, for both |
| `profile <name> [key=value ...]` | Prints the profile, or sets keys and then prints it |
| `gc [--dry-run]` | Stops idle boxes. See [Warmup and collection](#warmup-and-collection) |

`create` lets the container engine print its errors, since its message is the diagnosis. `enter`
does not: a box that will not start reports only `could not start <name>`, and `kdos-box start
<name>` shows no more. A pack-lane create that fails reports the engine's exit status, or, when your
home is on overlayfs, says that a box's writable layer has nowhere to go and that a persistent box
needs an installed system.

A `create` with an `image:` base prints that it reaches the network and fetches unsigned content,
runs `podman pull <ref>`, and stops if the pull fails. It then creates the box with
`distrobox create`. The strict signature setting does not cover an `image:` base; `pack:` is the
offline, verified base.

### enter and the terminal window

With no command, run from a terminal, and without `KDOS_BOX_NOTERM` in the environment, `enter`
opens a new `foot` window titled `<name> — KDOS box`, using the accent's palette from
`~/.config/foot/themes/<accent>` when that file exists, and the window runs `kdos-box enter <name>`
again inside it. In every other case it runs in place: when its input is not a terminal, when a
command is given, or when `KDOS_BOX_NOTERM` is set to any value (only whether the variable exists is
checked). The in-place form is `podman exec --interactive [--tty] --user=<uid>:<gid> --workdir=$HOME
--env=KDOS_BOX=<name>`; it sets no other variable. `KDOS_BOX` is what a prompt such as starship
shows.

### create with a box base

`create` with `base=box:<name>` does not work. The command is meant to make a box on another box's
software with an empty writable layer, but it never completes: it repeats its own base lookup until
the program crashes. Use `kdos-box clone` instead, which copies the software and the work, or read
the other box's base with `kdos-box profile <name>` and pass that base to `create`.

### freeze, import and clone

These commands, like snapshots, work on the writable layer that `kdos-packd` keeps for a pack box at
`~/.local/share/kdos/boxes/<name>/upper`. A store box keeps its writes inside the container engine's
own store and has no such directory, so on a store box `snapshot` stops with `<name> has nothing
written to snapshot`, `freeze` stops with `<name> has no writable layer to freeze`, and `clone`
creates the new box on the same image but copies none of the source's work, while still printing
`<src> cloned to <dst>`.

`freeze` packs the box's writable layer, which is only what you changed, into one pack with
`kdos-pack build`, with the box's `pack:` base recorded in `requires` so an import knows what it sits
on. The pack id is `box.<name>`, its kind is `app` and its version is the time of the freeze in
seconds; the default output is `<name>.kpack` in the current directory. When the box runs on a
temporary writable layer, which is the case on a live session, `freeze` warns and packs
`/run/user/<uid>/kdos/boxes/<name>/upper` instead. The result is a difference, and it can be
compared against a previous freeze like any other pack.

A pack box is created over an exploded root, an unpacked directory tree rather than an image, so the
container engine has no image to commit it to. Freezing is the way to capture a pack box's state.
Sign the result with `kdos-pack sign <file> <key>` and bring it back with `kdos-box import`.

`import` copies the pack into the daemon's staging directory and asks the daemon to install it,
because a check made by the client would prove nothing to the daemon that mounts the pack; the
daemon checks the pack's hash and any signature as it installs it. With `as <name>` it then runs
`create` with `base=pack:<id>`, taking the id from the daemon's reply.

`clone` creates the new box with the source's `base` and then copies the source's writable layer
into it with `cp -a`, so it copies the software *and* the work. To start from the same software
with an empty workspace, read the source box's `base` with `kdos-box profile <src>` and create the
new box on it.

### Snapshots and rollback

Snapshots exist for pack boxes only, for the reason given under
[freeze, import and clone](#freeze-import-and-clone). A snapshot is a full copy of the writable
layer, made with `cp -a`, so on an ordinary filesystem it
costs as much space as the box has written. It is not a pack: writing a pack back into a writable
layer needs it mounted, and a rollback that needed the daemon would fail exactly when a box is
broken. Rollback deletes the writable layer and copies the snapshot in its place.

### kdos-box export

This is `kdos-box export`, a launcher for one application in one box; it is unrelated to
`kdos-appbox export`, which writes packs to a `.ktar` file. A secondary box's application gets a
box-qualified desktop id, `<app>.<box>.desktop`, and the category `X-KDOS-Box`, while the default
box keeps upstream's own id. That refines the launcher-naming rule rather than breaking it: the rule
exists because the panel matches a window to an entry, and for a box's windows the panel has a
better key than the file name, the box named in the window's security context.

The `<app>@<box>` shim is an absolute link to `/usr/local/bin/kdos-appbox`, which dispatches on its
own name. That name is looked up in the name-to-command tables like any shim, and `export` writes no
row for it, so the shim answers "unknown alien app" until a row of that name exists. The desktop
entry does not depend on the table and works. The shim is also a link the launcher sweep recognises
as its own, so the next `kdos-appbox install`, `uninstall` or `import` deletes it; the desktop
entry, which carries no `X-KDOS-Alien` key, is kept.

## Warmup and collection

At login the session (`kdos_session_boxes` in `/usr/local/lib/kdos/session-common.sh`) runs
`nice -n 10 kdos-appbox warmup` in the background. With one box per application, starting a single
shared box would help nothing, so the warmup reads your favourites, `~/.config/kdos/favorites` (one
desktop id per line, `#` for comments), and composes and starts the box behind each, up to eight.

For each favourite it reads the desktop entry, from `~/.local/share/applications`, then
`/usr/share/applications`, and takes the pack from the `Exec` line: `-b <pack>` where it is named
before `run`, otherwise the command after `run`, resolved as `run` resolves it. A hand-written entry
naming a shim is looked up in the table by that name. A favourite that resolves to no pack is
skipped and does not count towards the eight. A lock (`$XDG_RUNTIME_DIR/kdos-appbox.warmup.lock`)
makes a second warmup a no-op rather than a queue.

The session also runs `kdos-box gc` every ten minutes at `nice -n 10`. It looks only at running
boxes, and stops only those whose profile sets `autostop`; the default is never. The time is counted
from when the box started, not from when its last window closed. A box past its time is stopped at
the next run, but only after asking the compositor (`kdos hey boxes`) whether the box has a window
open. A box with a window is left alone, and so is every box when there is no session to ask,
because "cannot tell" is not "no window". `--dry-run` prints what would be stopped.

Boxes that a launch or the warmup creates get a default profile with no `autostop`, so with the
defaults a warmed box stays running for the rest of the session. To have warmed boxes given back
when idle, set `autostop` on your favourites' boxes, for example
`kdos-box profile app.hugin autostop=30m`.

## Storage drivers

This governs the container engine's own store: the store lane's images and containers, and
development boxes with an `image:` base. Pack boxes are composed by `kdos-packd` and are not in it.

| Situation | Driver |
|---|---|
| A live session | `overlay` through `fuse-overlayfs`, pinned in `/etc/containers/storage.conf` by the `containers-common` port |
| An installed system whose home is on ext2, ext3, ext4, btrfs, xfs or f2fs | The kernel's native overlay |

A live session needs the userspace driver because the home directory sits on the boot overlay, and
the kernel refuses to stack an overlay's writable layer on an overlay. The engine does not fall back:
the container fails to mount.

On an installed system the native driver is much faster, so the first run writes
`~/.config/containers/storage.conf` with `driver = "overlay"` and no mount program. It does so only
when that file does not exist, the store's `containers.json` lists no container, and the filesystem
holding your home is one of those above. The two drivers write incompatible deletion markers into
container layers, so the choice must never change once a container exists. To change it, remove every
container first.

## Files and variables

| Path | What |
|---|---|
| `/usr/share/kdos/appstore/catalogue` | The catalogue |
| `/usr/share/kdos/alien-apps`, `~/.local/share/kdos/alien-apps` | The name-to-command tables, system and yours. Lookups read `$XDG_DATA_HOME/kdos/alien-apps` when that variable is set |
| `~/.config/kdos/boxes/<name>.conf` | Box profiles |
| `~/.local/share/kdos/boxes/<name>/` | A pack box's writable layer (`upper`), its snapshots, and a private home |
| `/run/user/<uid>/kdos/boxes/<name>/` | A pack box's merged root (`root`), and its writable layer when the home cannot hold one |
| `~/.config/kdos/favorites` | What the warmup starts |
| `~/.config/kdos/a11y` | Opts boxes in to accessibility |
| `~/.config/containers/storage.conf` | The storage driver choice |
| `$XDG_RUNTIME_DIR/kdos-appbox.trace` | Launch stage timings |
| `$XDG_RUNTIME_DIR/kdos-appbox.create.lock`, `kdos-appbox.warmup.lock` | Serialise box creation and the warmup |
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
| `KDOS_PACK_EXTRACT` | Build time only: where `030_launchers.sh` looks for extracted packs |

## See also

- [Applications](../02-user-guide/applications.md): using all of this day to day
- [Packs and boxes](../03-architecture/packs-and-boxes.md): the pack format, the daemon and composition
- [The daemons](daemons.md): the pack daemon, the memory daemon and `kdos-boxsock`
- [The session](../03-architecture/session.md): the environment and what a box shares
- [The security model](../03-architecture/security-model.md): what a box is and is not
- [kdos-comp](kdos-comp.md): the sandbox allowlist that `grant` extends
- [The kdos command](kdos-command.md): `kdos app`, the everyday front end
- [How KDOS differs](../01-philosophy/how-kdos-differs.md): boxed applications against Flatpak,
  Snap and distribution packages

<!-- book-nav -->
---

*Part IV — Programs, chapter 25.* Previous: [24. kdos-term](kdos-term.md) · [Contents](../README.md) · Next: [26. The daemons](daemons.md)
