# kdos-appbox

`kdos-appbox` is the program that runs containerised applications on KDOS: it builds them from the
catalogue, creates and starts the containers they run in, writes the launchers that make them look
native, confines each box under KWin, and opens files and links in whatever handles them. This
chapter is its reference. It is
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
# the everyday command; it runs kdos-appbox install
kdos app install app.hugin
# everything the catalogue offers, and what is installed
kdos-appbox catalogue
# build and install one application
kdos-appbox install app.hugin
# every installed application is also a command
hugin shots.pto
# open a file in whatever handles its type
kdos-appbox open report.pdf
# a second box on Hugin's base; a pack box's work is copied too
kdos-box clone app.hugin work
# a shell inside it
kdos-box enter work
# what the box is allowed, and how that is enforced
kdos-box profile work
```

## Synopsis

```text
kdos-appbox [-b BOX] [-l LAUNCHER] [-v] run <app> [args...]
kdos-appbox open [--print] [--choose] <path|uri>...
kdos-appbox throwaway [--pack <id>] <file>
kdos-appbox session save | restore
kdos-appbox catalogue [--groups | --chain | --selftest]
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

kdos-box list | ls [--json]
kdos-box create <name> [base=pack:<id>|image:<ref>|box:<name>]
                [key=value ...]
kdos-box enter <name> [command ...] | --window
kdos-box run [-l <launcher-id>] <name> <app> [args ...]
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
kdos-box gc [--dry-run] [--session-end]

xdg-open <path|uri>...
<app-name> [args...]
```

### Global options for kdos-appbox

These come before the command word, and apply only under the name `kdos-appbox`.

| Option | Effect |
|---|---|
| `-b NAME`, `--box NAME` | Run in the box `NAME` instead of the application's own box |
| `-l ID`, `--launcher ID` | The launcher id the launch's windows take: the box, or the box and one more component. Every generated launcher passes one; see [The launch path](#the-launch-path) |
| `-v`, `--verbose` | Let the container engine print to standard error, and a store build print its steps and `RUN` output to the terminal. By default a child's standard error is discarded, so a click never sprays container messages at the desktop, and a store build's output goes to its build log |
| `-h`, `--help` | Print the usage summary to standard error and exit 0 |

An unknown option exits 1 with a message. With no command, or with a command it does not know,
`kdos-appbox` prints its usage and exits 1. `kdos-box` with no command, an unknown one or one missing
its arguments prints its own usage and exits 2. `kdos-appbox keeper <box>` is started by a launch
and is not a command a person types; see [The keeper](#the-keeper).

## Description

### Two ways an application reaches a box

There are two lanes, and they differ only in how a box's root filesystem is made.

| Lane | How the application arrives | What the box is | Profile `base` |
|---|---|---|---|
| Store | `kdos-appbox install` builds it on this machine from the catalogue, as a stack of container images `kdos/base`, `kdos/rt-gtk`, `kdos/app.hugin` … | A container made by `podman create` over the top image, with the flag set a pack box gets | `image:kdos/<id>` |
| Pack | A signed pack is installed through the pack daemon, `kdos-packd`: by `kdos-appbox import`, by `kdos-box import`, or from a medium that carries packs | A container made by `podman create --rootfs` over an overlay that `kdos-packd` composes from the pack and every pack it requires | `pack:<id>` |

Both lanes share one launch path, one environment, one set of launchers, one profile format and one
boundary: each sees a private runtime directory of its own as `/run/user/<uid>`. Once a box exists,
every launch goes through the same sequence of `podman start` and `podman exec` whichever lane
created it. A third kind of box, a development box that `kdos-box create` makes over somebody
else's image (an `image:` base naming anything but `kdos/<id>`), is made by `distrobox create` and
shares the session's runtime directory; it is never more than [tagged](#confinement).

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
session bus through `gdbus`, with a two-second call timeout. Three programs it runs are
dependencies of the port: `kdos-boxsock` for every launch's tagged socket, `kdos-boxinit`, which
every pack box and store box runs as its init, and `xdg-dbus-proxy`, which the box's keeper runs as
its bus. Without any one of them a box does not start, and the launch says which is missing.
`kbuildsycoca6` is run after launchers are written when it is installed, so the Start menu,
KRunner and the Task Manager see them at once.

### Where it is installed

The port is `src/system/kdos-appbox`.

| Path | What |
|---|---|
| `/usr/local/bin/kdos-appbox` | The binary |
| `/usr/local/bin/kdos-box` | A symbolic link to it |
| `/usr/local/bin/xdg-open` | A symbolic link to it |
| `/usr/share/kdos/appstore/catalogue` | The catalogue it reads |

`/etc/profile` puts `/usr/local/bin` before `/usr/bin` on the shipped `PATH`, which is why this
`xdg-open` answers ahead of the xdg-utils script. The recipe also depends on `podman`, `distrobox`,
`shared-mime-info` and `xdg-utils`; the container ports are on the
[`containers` shelf](../06-reference/ports-catalogue.md#containers) in the ports catalogue.

Tab completion for both names comes from the base system's files
`/usr/share/bash-completion/completions/kdos-appbox` and `kdos-box`, shipped from `fs/`: the
commands and options, the commands `kdos-appbox apps` lists after `run`, store ids and groups after
`install`, and box names wherever a box is named
([what each completes](kdos-command.md#finding-a-subcommand)).

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
kdos-appbox open --print report.pdf   # resolve and print, run nothing
kdos-appbox open --choose report.pdf  # always ask which application
```

Resolves a file or a URI to the application that handles it and runs that application. See
[The open path](#the-open-path).

`--print` writes the resolution as tab-separated lines instead of running it:

```text
mime        application/pdf
candidates  okularApplication_pdf.desktop   …
default     yes
entry       /usr/share/applications/okularApplication_pdf.desktop
exec        okular   report.pdf
```

Candidates are desktop ids exactly as the tables write them, `.desktop` suffix included, and a path
argument is passed on as typed, not made absolute. The sample is the shipped machine, where
`/usr/share/kdos/xdg/kde-mimeapps.list` names Okular's PDF entry for PDF and the default comes
first; every other installed entry that claims the type follows it, and an installed boxed viewer
adds its own launcher id to the candidates.

A `choose` line, `choose` and `portal`, appears when the chooser would be asked, followed by what
a tree without the chooser would run. With no handler at all there are no `candidates` or `default` lines, `entry` is `-`, and
`exec` names `/usr/bin/xdg-open`.

### catalogue

```sh
kdos-appbox catalogue
kdos-appbox catalogue --groups
kdos-appbox catalogue --chain
kdos-appbox catalogue --selftest
```

With no option it prints one tab-separated line for each `app` and `data` row:

```text
<id> <name> <category> <bytes> <parent> <installed|available> <tagline>
```

The separator is a tab because a tagline contains spaces and every other table in this tree splits
on tab. `<parent>` is `-` for a row with none. `--groups` prints one line per named group instead:
`<id>`, tab, `<description>`, tab, then the member ids separated by spaces. `--chain` prints one
line per `base` and `runtime` row, `<id>  <bytes>  <parent>  <built|absent>`, where `built` means
the engine lists the image `kdos/<id>` and a machine with no container engine reports every row
`absent`; Discover adds the bytes of each `absent` row up an application's chain to its estimate.

This command is the only place that decides what is installed. Discover's KDOS backend and
`kdos app` both ask it, so they cannot disagree. `kinstall` compiles the same catalogue reader into
itself to list the groups, and does not ask what is installed. A container that the engine lists
under a catalogue id *is* that application, because nothing else could be launched, and a machine
with no container engine reports everything as `available`.

`--selftest` runs the catalogue parser's own assertions against the file named by
`$KDOS_CATALOGUE`. It needs no daemon and no display, and it is checked before anything that does,
so it runs in a build container. `testing/selftest.sh` runs it against
`testing/fixtures/catalogue/catalogue` and then reads the shipped catalogue with and without
`--groups`. See [Testing](../05-developer/testing.md).

The shipped catalogue carries 69 `app` rows and 2 `data` rows, built on 2 base rows (`alpine` and
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

Before it builds anything, install asks whether the build fits. Every row of the chain still to
build counts: twice its `meta` size (the downloaded packages sit beside the unpacked ones, and the
commit writes a copy of the layer) plus 128 MiB for what the packages' maintainer scripts write
and the caches the layer copies up from its parent, or 384 MiB for a row that carries no size. A row's size is what its image adds on disk over its
parent's, and for a base its whole image. The base and runtime sizes were measured with `du` of each
built image on trixie; the application and data sizes are the summed `Installed-Size` of every
package the row's chain installs and its parent's chain does not, as `apt-get install -s
--no-install-recommends` resolves them at the catalogue's snapshot, and for the `deb` row the
package's own `Installed-Size` as well. `testing/catsizes.py` computes them and, with `--write`,
rewrites the application and data rows' `meta` lines. When that is more than the free space
where podman keeps its images (`$XDG_DATA_HOME/containers/storage`, or the nearest directory of it
that exists), install builds nothing for that application and prints `<id>: not enough room — the
build needs about <N> and <M> is free where podman keeps its images`; on a live session, where that
is the memory overlay, the sentence adds that everything the session writes shares it, and to
install it on an installed system, on a live session with a persistent store (`kdos persist`), or
on a machine with more memory. A
`deb:<package>` install asks the same of its base row.

The check is an estimate from sizes measured on one day's snapshot, so a build can still run out of
room, most often on a live session. Each row's build writes both of the container engine's output
streams to `~/.cache/kdos-appbox/build-<id>.log` (under `$XDG_CACHE_HOME` when that is set),
replaced by the next build of that row: the steps and every `RUN`'s output from standard output, and
podman's final `Error:` line, which names the reason, from standard error. When a build fails,
install prints `the build's last lines (all of it: <log>)` and the last eight lines of the log on
standard error, which hold apt's or podman's own reason, such as a package with no installation
candidate or `no space left on device`, before `<id>: <row> did not build`. A line longer than 300
characters is printed as its first 100 and its last 180, joined by `…`: podman's `Error:` line
repeats the whole `RUN` and puts its reason at the end. Under `-v` the build draws on the terminal
as it runs instead, and no log is written. When the log cannot be created, install says so, with
the reason, and runs the build on the terminal the same way.

Nothing rolls back. Six applications where the fourth fails to build leaves the other five's images
built and names the fourth: what one application built is not harmed by a later one failing, and
undoing it would throw away a long apt run. Progress is one flushed line per step
(`==> building kdos/app.hugin`, `==> app.hugin installed`), so a program showing install progress
can read standard output without parsing the container engine. Such a program should start
`kdos-appbox` itself and read its output directly. A service that `ksvc`, the service supervisor,
runs cannot carry it: the supervisor sends a supervised program's output to syslog and
`/run/kdos-svc.<name>.log`, never to whoever started the service (see
[Supervision and logging](daemons.md#supervision-and-logging)).

When the images are built, install creates the box with `kdos-box create <id>
base=image:kdos/<id>`. The image is the one just built, `localhost/kdos/<id>` in your own image
store, and an `image:` create uses a stored image as it is, without asking a registry (see
[kdos-box](#kdos-box)). The box is created by `podman create` over that image with the flag set
described under [What a box is created with](#what-a-pack-box-is-created-with). When the create fails, install prints `<id>: the image built and the box
did not: <reason>`, the last line `kdos-box create` printed, counts the application as failed and does not regenerate the launchers for it; the images
stay built, and `kdos-box remove <id>` deletes the profile the failed create wrote. Where a box is
created, install regenerates your launchers before the command returns. Every regeneration
(after `install`, `uninstall` and `import`, and `genlaunchers --packs --user`) writes your tree from
two sources together, because each run writes the whole set and either source alone would delete
the other's launchers. The first is the store boxes: for every container whose name is a catalogue
`app` row and whose profile names no base other than `image:kdos/<id>`, it extracts `usr/share/applications`, `usr/share/icons/hicolor` and `usr/share/pixmaps`
from the image `kdos/<id>`, so a store box's icons are exported under its launcher id as a pack's
are, and takes the row's
catalogue `cmd` rows, which become shims and table rows like any command; a store box with either
counts. The second is every installed app pack, read as `--packs` reads them. The extraction reads
the image, not the container, so a pack box whose application was never built here yields nothing
from it and is covered by the pack walk instead. With no catalogue, or no answer from podman, the
regeneration is the pack walk alone. The exit status is the number of applications that failed.

`uninstall` of an id with no container and no `kdos/<id>` image prints `==> <id> is not
installed` and counts no failure, so it exits 0, and a group's uninstalled members are skipped the
same way. A profile does not make an id installed: the first uninstall keeps it, so a second
uninstall of the same id says `not installed` and leaves that profile where it is. A
`deb:<package>` uninstall follows the same rule over its box `deb.<package>` and its image
`kdos/deb.<package>`, printing `==> deb:<package> is not installed`. Otherwise `uninstall` removes the box first. A box with a container is stopped with the
[`kdos-box stop`](#kdos-box) sequence, since a box outlives its application's last window, and then
removed with `kdos-box remove`; as after any `kdos-box remove`, its profile and
`~/.local/share/kdos/boxes/<id>` stay. A box installed and never started has a profile and no
container, and `kdos-box remove` deletes that profile, which takes the box out of `kdos-box list`.
When the stop leaves the box up or the remove refuses, the uninstall stops there, printing
`<id> was not uninstalled` and counting a failure. A podman that cannot say whether the box has a container
(`podman container exists` exits neither 0 nor 1) stops the uninstall the same way, before any
image is touched. Then it removes the application's own image, then each
runtime above the base, top down, stopping at the first one that a remaining container's chain still
names. It then regenerates the launchers from what remains. The base image is never removed: it is
every chain's floor, and the next install would build it again. When no remaining box stands on it
(a `deb.<package>` box counts, being built on the `base` row), the uninstall, a `deb:<package>`
one too, which removes the box and `kdos/deb.<package>`, says so in one line,
``kept kdos/base and the debian:trixie-slim it is built on (about <size> together), the floor of
every store application; `podman rmi kdos/base debian:trixie-slim` gives the space back``,
naming the image the catalogue's `base` row is built `FROM` and the size podman gives the base
image, which counts the layers below it; the size is left out when podman cannot say. Which
runtimes are still wanted is asked of the catalogue rather than of the engine, because a
dangling-image sweep cannot tell a runtime nothing uses from one whose only application is
mid-install.

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
  PACKAGES         the pack index: id, version, kind, size, sha256
                   and file name per pack
  PACKAGES.sig     present when a signing key was readable
  app.hugin.kpack
  app.scribus.kpack
```

Each pack is made by creating a throwaway container from `kdos/<id>`, flattening it with
`podman export`, and packing the tree with `kdos-pack build`. Its metadata carries the row's id and
kind, the catalogue name and tagline, and the export time as its version. It also carries the
rows the pack lane cannot ask the catalogue for: an application or runtime row's `env` chain as
`env =` lines, its `cmd` rows as `command =` lines and its `needs` rows as `needs =` lines, and a
data row's `graft` and `boxgraft` rows as `graft =` and `boxgraft =` lines. The index is written by
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
  but the export prints that the index is UNSIGNED and that an import treats these packs as
  unsigned.

The packs an export builds carry no signature block of their own; a signed set is vouched for by
its index. When the archive holds both `PACKAGES` and `PACKAGES.sig`, import copies them into the
daemon's staging directory as `PACKAGES.<pid>` and `PACKAGES.<pid>.sig`, names that index in each
`install` it sends, and removes both when the import ends. The daemon reads only an index a request
names, so another install running at the same time is judged on its own index or none. The daemon
checks each pack's payload against the hash in its own footer, then accepts a pack whose hash a
signed index it trusts lists, so a set signed with a key in `/etc/kdos/keys/packs` imports as
signed. An unsigned set's packs have only the footer hash, which does not catch a pack whose payload
and footer were rewritten together; the daemon accepts them as unsigned, and a daemon started with
`KDOS_REQUIRE_SIG` refuses every one of them. A pack in the store is not hashed again when it is
mounted.

Import unpacks the archive, then stages each selected pack into the pack daemon's staging directory
(`$KDOS_PACK_STORE/staging`, by default `/var/lib/kdos/packs/staging`) and asks the daemon to install
it by file name, never by path. That rule is what stops a request reachable from the `wheel` group
from turning into a mount of an arbitrary device. For each pack the daemon accepts, import asks the
daemon for its kind. A data pack is grafted (`graft <id>`, which prints `<id>: N graft(s)`) and gets
no box, since the daemon never composes one into a root filesystem; any other pack gets its box
with `kdos-box create <id> base=pack:<id>`. A pack the daemon refuses is named and skipped;
the rest of the archive still imports. An id whose box already exists — a store box of that name, or the
box an earlier import made — is not staged: import prints `<id>: already installed (box <id>
exists — …)` and counts it apart from both the imported and the failed; `kdos-box remove <id>` and
the import again moves the box onto the set's pack, and its launchers then come from the pack
alone, though the store image `kdos/<id>` stays built until `podman rmi` removes it. A box
create that fails prints `<id>: the pack installed and the box did not: <reason>`, asks the daemon
to remove the pack it has just installed and says whether it did, and counts a failure. The last
line reports how many imported, how many were already installed (when any were) and how many
failed, and the exit status is 1 if any failed. When at least one imported, import regenerates your
launchers the way install does, with the same limit on a machine that also holds store boxes (see
[install and uninstall](#install-and-uninstall)).

`SELECTION` is plain text and can be edited or compared. It opens with `#` comment lines giving the
export date and the catalogue's identity (its row count and byte length). A `group <name>` line
records a group that was picked, and a bare line is an id. Under each `group` line the export
writes the ids that group held in the catalogue at export; after the groups come the ids named
directly on the command line. Each id is written once, under the first group that holds it when a
group does. Import reads only the bare id lines, so a bare `import` installs every member of every
picked group and every named id, whatever the catalogue on the importing machine says the group
holds. An id whose application was not built on
the exporting machine is still listed but has no pack; import prints `<id>: not in this archive`
and counts it as a failure.

### list, apps, warmup, status

| Command | Prints or does |
|---|---|
| `list` | Every container, its image, its state, and `custom` or `default` depending on whether a profile file exists |
| `apps` | Every row of both name-to-command tables: the name, `baked` (the system table) or `user`, then the command and, when the row has one, the pack it runs in |
| `warmup` | Composes and starts the boxes your favourites file names, each with its keeper. See [Warmup and collection](#warmup-and-collection) |
| `status` | What the pack daemon reports, then the state of the box named with `-b` as `box   : created (<state>)` or `box   : not created (first launch will) (absent)`, and the `state :` lines of the boundary its last launch recorded (see [Confinement](#confinement)). With no `-b` it lists every box made from a store image instead, one `store : <box> (<state>)` line each, or `store : none`, followed by the box line only when the placeholder box `kdos-apps` exists; when podman cannot be asked it prints the placeholder's line as with `-b`. When the daemon refuses, the first line is its reason, `packd : <reason>`: a user who is neither root nor in `wheel` sees `packd : not permitted`, and a refusal with no reason prints `packd : refused`. `packd : not answering` means only that no daemon answered on the socket, or its reply could not be read |

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
| `meta` | `meta <id> <name>\|<category>\|<bytes>\|<tagline>` | What Discover and `kdos app` show. Optional: a row with none presents as its own id, category `Other`, size 0 and no tagline. A category written as `A;B;` shows as `A`. A category the catalogue writes is one main desktop-entry category, `AudioVideo`, `Development`, `Education`, `Game`, `Graphics`, `Network`, `Office`, `Science`, `Settings`, `System` or `Utility`, so Discover can show its icon for an application with none of its own; `testing/selftest.sh` refuses any other. The tagline is the card's second line and the page's heading, under the name, so an application's or data row's tagline says what it does without repeating its name: `testing/selftest.sh` refuses one that holds the name anywhere or opens with the name's first word, each as a whole word, and one whose parenthesised aside spells the one to three words before it again (`VS Code (vscode)`) |
| `group` | `group <id> <description>`, then `group <id> <member ids …>` | The first line for a group is its description; later lines list members, recognised by the `app.` or `data.` prefix |
| `snapshot` | `snapshot = auto` \| `<date>` \| `off` | Which Debian archive the packages come from. See [The Debian snapshot](#the-debian-snapshot) |
| `image` | `image <base-id> <ref>` | A base built `FROM` that image instead of `debian:trixie-slim`. `alpine` is `alpine:3.24.1` |
| `env` | `env <id> NAME=VALUE` | A variable for every launch of a store box whose chain contains this row, and an `env =` line in an exported pack's metadata. See [The environment a box gets](#the-environment-a-box-gets) |
| `needs` | `needs <app-id> <data-id> …` | Data rows installed together with the application |
| `deb` | `deb <id> <releases-api-url> <asset-pattern>` | A package Debian does not carry: the first download URL in the API's answer that ends in the pattern (so `amd64.deb` never takes the `amd64.deb.sha256` beside it) is fetched, checked with `dpkg-deb --info` and handed to apt. An answer with no such URL — the query failed, GitHub refused it (it answers 60 unauthenticated queries an hour from one address), or the release has none — stops the build with `<id>: no release asset ends in <pattern>. The query failed, was refused (GitHub answers 60 unauthenticated queries an hour) or lists none: <url>`, which the build's last lines show. The pattern may hold only letters, digits and `. _ + -`; another character, or a `'` in the URL, makes no Containerfile. The shipped row, `app.vscodium`, asks the API for the latest release |
| `about` | `about <id> <licence>\|<author>\|<homepage>\|<description>` | What Discover shows on the page of an application no Debian component describes, such as a `deb` row's; a component fills only the fields it leaves empty. The description is the rest of the line, as plain text. Read by Discover alone, in any position; kdos-appbox, `kinstall` and `kdos app` skip it |
| `cmd` | `cmd <id> <name>` | A command the application provides. Each becomes a shim and a table row when your launchers are regenerated from the store boxes, and a `command =` line in an exported pack |
| `graft`, `boxgraft` | `graft <id> <from> <to>`, `boxgraft <id> <from> <to>` | Where a data row's files land, on the host and inside a box. They travel as `graft =` and `boxgraft =` lines in the data row's exported pack, and `kdos-packd` lands them when `import` installs that pack; a data row the store builds is an image and is grafted nowhere |

Pack rows, `group`, `image`, `env`, `cmd`, `needs`, `deb`, `about`, `graft` and `boxgraft` rows may appear
in any order: the whole file is read before any chain is resolved, and a parent or a base is looked
up by id wherever it stands. A `meta` row is the exception. It is applied as it is read, so it must
come after the pack row it describes; one placed above that row is dropped without a message, and
the application presents as its own id, category `Other`, size 0 and no tagline.

A chain cannot be closed when a row names a parent that is not in the file at all, or when it runs
past 16 rows, which is also where a cycle ends. The install then stops for that application with
`<id>: no chain — a parent is missing from the catalogue`, where `<id>` is the application's, not
the missing parent's. The file holds at most 512 pack rows and 32 groups: a 513th pack row stops
the catalogue loading at all, with `more than 512 rows`, and a 33rd group is ignored.

## The launch path

In order, because every launcher on the system and the login warmup depend on the order:

1. **Choose the storage driver, once.** Every invocation, under any name including a shim's, checks
   this first; it settles only once per user. See [Storage drivers](#storage-drivers).
2. **Resolve the box.** A generated launcher for a pack application writes
   `Exec=kdos-appbox -b <pack> -l <launcher-id> run <exec>`, so the box and the launcher are named
   outright. A shim looks its own name up in the name-to-command table and takes the box from the
   row. Without either (a prompt, an entry naming no box) the box is resolved from the command as
   described under [run](#run).
3. **Resolve the launcher id.** The launcher id is the application id KWin gives the launch's
   windows, so it decides the icon a window shows, the pinned task it lights and the window rule
   that colours it; it must be the file name of a launcher `genlaunchers` wrote. `-l` names it. A
   launch without one (a shim, a prompt, `kdos-box run`) asks the box's own launchers which of them
   runs this program, by the same key the name-to-command table is matched by, and a program none
   of them runs takes the box's own name. A throwaway box takes the launcher id of the box it was
   made from.
4. **Say that a box is starting**, if it is not already running: a desktop notification, "Starting
   app", sent in the background so it can never hold up the launch.
5. **Recover a stuck box.** A box left *stopping* is not startable: a `podman stop` from outside
   KDOS that times out on a box sharing the host's PID namespace (see
   [kdos-box stop](#kdos-box)) leaves it there, and asking the engine to start it then fails with
   "container state improper", which names nothing a person can act on. An application hung in
   uninterruptible I/O can leave a box there for good. The recovery runs the stop sequence with a
   three-second grace, and reaches the box's applications and its init but not the other launches
   waiting for the box, so two launches into a stuck box do not end each other. A pack box still `stopping` after that is removed (with a
   "Resetting app container" notification), and nothing is lost: its packs are read-only and its
   writable layer is on disk outside the container, so the next step creates the container again
   over the same stack. Any other box, a store box or one whose profile records no base, keeps its
   writable layer inside the container, so it is never removed here. It is left `stopping`, the
   launch stops with an "App container stuck" notification and exits 1, and the warning names
   `kdos-box remove <name> --force`, which discards everything the box wrote; the profile is kept,
   so the launch after that creates the box again over its image.
   `kdos-box start` shares this recovery: it creates a removed pack box or store box again before
   starting it, and exits 1 for a box it left stuck.
6. **Create the box, if it does not exist.** For a pack box this asks `kdos-packd` to compose the
   stack and runs `podman create --rootfs` over it; a store box (`base=image:kdos/<id>`) whose
   container is gone is created again over its image, printing `==> Recreating '<box>' from
   <image>...`. Both take the flag set under
   [What a box is created with](#what-a-pack-box-is-created-with), under a lock
   (`$XDG_RUNTIME_DIR/kdos-appbox.create.lock`) so a launch and the login warmup cannot race to
   create the same container. A pack box's first launch prints
   `==> First launch: composing '<box>' from packs...`; if the daemon is not running, or refuses,
   the launch says which, and an image missing from your store is named as not present. Either
   failure stops the launch with `could not create box '<box>': <reason>` and an "App not started"
   notification carrying the same reason, podman's last line when podman refused. A stopped pack
   box, one whose profile's base is `pack:<id>`, is removed and created again over the same packs
   when its container shares the session's runtime directory, or when its cgroup scope belonged to
   a login session that has ended, because its recorded cgroup parent can never be made again. A
   box with no profile, such as a distrobox made with `distrobox create` (`kdos-fetch-app`'s
   among them), shares the session's runtime directory by design and keeps everything installed in
   it inside its container: it is never removed or composed, only started with `podman start`.
7. **Start it, composing first.** A pack box's merged root lives under `$XDG_RUNTIME_DIR`, which is
   a temporary filesystem, so after a reboot the container still exists but the root it was created
   over is gone. Composing is idempotent, because the daemon counts references, so a box that is
   already composed costs one round trip. A store box has no pack base and is started as it is.
   A start that fails stops the launch the same way, with `could not start box '<box>': <reason>`
   and an "App not started" notification, so the "Starting app" notification is never the last
   word.
8. **Wait for readiness.** The box's init prints `container_setup_done` when the user account inside
   exists: `kdos-boxinit` in a pack box or a store box, distrobox's init in a development box. If
   that line is not yet in the container's log, the launch waits for it, for at most 30 seconds.
   Running a program in a box before that point runs it with no user. A box that is not running is
   not ready. The check reads the log of this start only: `podman logs --since` the later of the
   container's `FinishedAt`, the end of its previous run, and two seconds before its `StartedAt`,
   both from `podman inspect`. `StartedAt` alone would be too late: podman takes it after the
   runtime's start returns, when the init is already running, and conmon stamps each line as it
   reads it, so `kdos-boxinit`'s marker, printed within milliseconds, can carry an earlier stamp
   and be dropped. `FinishedAt` keeps a marker an earlier run wrote from answering for a restart
   whose init is still running; podman sets it only when a run exits, so after a reboot, whose
   last run never reached one, the two seconds below `StartedAt` do that. The log is read whole,
   since a box stopped during its setup and started again holds two inits' output and passes
   64 KiB before its marker, and searched as bytes, so a NUL in a line hides nothing after it.
   `--tail` is not used: podman's `k8s-file` tail reader gives up at the first line it cannot
   parse, such as the NULs an unclean shutdown leaves in the log file, where the forward reader
   skips the line and goes on. A marker in the output counts whatever podman's exit status. When
   `podman logs` fails with no marker in its output, the check asks whether `/.containersetupdone`
   exists in the box. Only distrobox's init creates that file, once its first package install is
   done, and it never removes it, so for a development box it proves an earlier init got that far,
   not that this start's init has finished; `kdos-boxinit` creates none, so a pack box or a store
   box whose log podman cannot read is never reported ready. While it waits the launch
   holds a mark, `$XDG_RUNTIME_DIR/kdos/boxes/<box>.wait.<pid>` holding its start time, by which
   [`kdos-box stop`](#kdos-box) finds and ends it; `kdos-box enter` waits the same way.
9. **Prepare the boundary.** See [Confinement](#confinement). The box's private runtime directory
   is refreshed, the [keeper](#the-keeper) is started if none runs and the launch waits for its
   filtered bus, and then `kdos-boxsock` is asked for the launcher's tagged socket and given five
   seconds to answer. A box whose bus or socket cannot be had does not start: the launch records the
   reason in the box's state file, shows an "App not started" notification naming it, and exits.
   There is no fallback to the session's own socket.
10. **Build the environment.** See [The environment a box gets](#the-environment-a-box-gets).
11. **Execute.** `podman exec --interactive --user=<uid>:<gid> --workdir=$HOME <box> env …
    <program>`, with `--tty` added when this process's input is a terminal and the process is in
    that terminal's foreground process group (`tcgetpgrp(0) == getpgrp()`). The same command
    therefore gives an interactive prompt at a shell and a plain execution from a launcher. A
    background job, such as `zathura report.pdf &`, takes neither `--interactive` nor `--tty`, and
    its input is `/dev/null`: podman sets a terminal it is handed to raw mode, and a process outside
    the foreground group that does so is stopped by `SIGTTOU` before its window opens. A foreground
    launch whose output is a terminal too titles it `<program> — KDOS box` with Konsole's OSC 30
    and with OSC 0, as `kdos-box enter` does (see [enter and the terminal
    window](#enter-and-the-terminal-window)), runs `podman` as its child with `SIGINT` and `SIGQUIT`
    ignored while it waits, since they are the application's, and `SIGTERM` and `SIGHUP` passed on
    to `podman`, and writes Konsole's default tab format `%d : %n` back when it ends; OSC 30
    replaces the tab's title format, which would otherwise outlive the program. A terminal cannot
    be asked for its format, so a Konsole profile with a tab title format of its own shows
    Konsole's default in that tab after a boxed terminal program has run there. The launch writes its pid and launcher into the box's launch
    ledger, `$XDG_RUNTIME_DIR/kdos/boxes/<box>.apps`, and an untitled launch replaces itself with
    `podman`.

Stage timings are appended to `$XDG_RUNTIME_DIR/kdos-appbox.trace`, one line per stage with a
seconds-and-microseconds timestamp; `open` writes its decisions there too, so the trace is where to
read how long a launch takes on a given machine. A cold launch, with no container at all, pays for
composing, creating, the box's init and the program's own start, which the program's source records
as about 18 seconds; a launch into a running box skips all of that.

None of those stages is visible to KWin. What a person sees during a cold start is the "Starting
app" notification and then nothing until the application connects, because there is no window to
map before that.

### What a pack box is created with

`podman create` receives this flag set for a pack box (`--rootfs <merged-root>`) and a store box
(the image). Each part is there because something breaks without it.

| Flags | Why |
|---|---|
| `--name <box>`, `--hostname=<box>.<host>`, `--label manager=kdos-box` | Identity |
| `--cgroup-parent <scope>` | A pack box only: a scope named `app-kdos-<box>-<n>.scope` in the current login's delegated cgroup subtree, so `memory`, `cpus` and `pids` are enforced. Omitted when the login has no subtree the box can be moved into. See [Memory and grants](#memory-and-grants) |
| `--user root:root --userns keep-id`, `--annotation run.oci.keep_original_groups=1` | Container uid 1000 is host uid 1000, so the files in `$HOME` belong to the box's user and the box runs as a real non-root account; applications that refuse to run as root start |
| `--security-opt label=disable`, `--security-opt apparmor=unconfined` | The host runs neither, and a profile that is not loaded is a denial rather than a policy |
| `--ulimit host`, `--pids-limit` | Host limits; the profile's `pids`, or no limit |
| `--network`, `--ipc`, `--pid` | From the profile. See [Box profiles](#box-profiles) |
| `--memory`, `--cpus` | From the profile, when set |
| `--rm`, and `:O` on the root | From `persistence`: `ephemeral` gives both, `frozen` the `:O` overlay alone. A store box takes `--rm` only |
| `tmpfs` on `/run` and `/run/lock` | A pack is read-only and `/run` must be writable |
| `/tmp` bound in | Shared with the host, except for a throwaway box, which gets a `/tmp` of its own and the host's `/tmp/.X11-unix` read-only only when it is handed the X11 display |
| `$HOME` bound in | Your home at its own path; with `home = private`, the box's own directory bound at that path instead |
| The box's private runtime directory at `/run/user/<uid>` | `$XDG_RUNTIME_DIR/kdos/box/<box>`. Nothing else of the session's runtime directory is bound, the document portal's mount included: the portal mounts it without `allow_other`, so the box's root, a subordinate uid, cannot stat it and a box with that bind never starts, and the portal hands a box real paths, not `/run/user/<uid>/doc` ones |
| `~/.config/kdos/boxes` read-only | With a shared home, so a box cannot grant itself what its profile withholds |
| `/dev/shm` bound in | Shared memory for toolkits |
| `/dev` and `/sys` bound in, or `/dev/dri` alone | Shared devices give the box the host's `/dev`; with private devices, `gpu = yes` binds the render nodes back |
| `devpts` on `/dev/pts` | With shared devices: a pty instance of the box's own over the shared `/dev`. The host's devpts belongs to the host's user namespace, so the engine cannot hand a pty there to the exec's user, and every `--tty` exec (a shim typed at a prompt, `kdos-box enter`) would fail with `chown /dev/pts/N: Operation not permitted`. A box with private devices gets the engine's own |
| `/run/cups` bound in | When the CUPS socket exists at create time, so a boxed application's own print dialog works |
| `/var/lib/kdos/packs/mnt` bound in read-only | When it exists, so a data pack's grafted links resolve inside the box |
| `/usr/share/kdos/xdg` bound in read-only | KDOS's configuration defaults, which every launch puts on `XDG_CONFIG_DIRS` after the box's own `/etc/xdg` |
| `/usr/libexec/kdos/kdos-boxinit` bound to `/usr/libexec/kdos-boxinit`, as the entrypoint | The box's `/usr` is Debian's; a static init from the host runs there |
| `KDOS_BOX`, `KDOS_BOX_USER`, `KDOS_BOX_UID`, `KDOS_BOX_GID`, `KDOS_BOX_HOME`, `container=podman` | What `kdos-boxinit` needs to create the user |

The root filesystem path, or the image, comes last, because `create` treats everything after its
first positional argument as the container's command.

A development box over somebody else's image is created by `distrobox create --name <box> --image
<ref> --yes --no-entry`. `--no-entry` stops distrobox writing a menu entry of its own for the box,
which `kdos-box remove`, a `podman rm`, would leave behind as a dead launcher; a box's launchers
come only from [`kdos-box export`](#kdos-box-export), `kdos-box remove` deletes them with the box,
and `kdos-fetch-app` creates its box the same way. distrobox's standard output, which carries its
`distrobox enter <box>` and a pull's image id, goes to `/dev/null`. Its standard error is read line
by line with the colour sequences taken out: its closing `Distrobox '<box>' successfully created.`,
`To enter, run:` and the blank line after it are dropped, and every other line, its errors
included, reaches the terminal. `Creating '<box>' using image <ref>` is shown while the container
is being made, and its `[ OK ]` follows when it is. The only way in that the create prints is
`kdos-box enter <box>`. The create adds `--unshare-netns`, `--unshare-ipc`, `--unshare-devsys`, `--unshare-process`,
`--init` and `--home <dir>` as the profile asks, `/run/cups` bound in when its socket exists,
and `/dev/dri` bound in for private devices with `gpu = yes`.

## Confinement

A boxed window is drawn by KWin like any other, and what keeps it inside its box is the box's
private runtime directory, its filtered bus and the tag KWin puts on its Wayland socket. The
security argument is in [The security model](../03-architecture/security-model.md#sandboxed-clients);
this section is what the program does.

**The private runtime directory.** A pack box and a store box see
`$XDG_RUNTIME_DIR/kdos/box/<box>` as their whole `/run/user/<uid>`, and what is in it is what the
box can reach:

| Entry | What |
|---|---|
| `wl[.<stem>]@<display>` | The tagged Wayland sockets, one per launcher, served by [`kdos-boxsock`](daemons.md#kdos-boxsock) |
| `dbus-proxy` | The box's filtered session bus, served by its [keeper](#the-keeper) |
| `pipewire-0`, `pulse/native` | Hard links to the session's audio sockets. A socket's inode is what a `connect()` reaches, so a link is the same socket, and the session's directory stays out of the box. Absent with `audio = no` |
| `at-spi/` | The accessibility bus, linked the same way, only when you have opted in (below) |
| `Xauthority` | The session's X11 cookie, only for a box whose `x11` resolves to yes |

The session's own Wayland socket (`wayland-1`) and `bus` are never in it. KWin's tag filters only
what reaches the tagged socket, so a box that could open `wayland-1` would be one `connect()` away
from every global the tag denies; that is why no launch falls back to the session's socket. A restarted
PipeWire binds a new socket and a link goes on naming the old one, so the keeper watches the
session's directory and links again, and every launch links again too.

**Which boundary held** is recorded for every launch in `$XDG_RUNTIME_DIR/kdos/boxes/<box>.state`,
which the Boxes settings page and `kdos-appbox status` read:

| State | Means | KWin engine |
|---|---|---|
| `confined` | Tagged, a private runtime directory, the filtered bus, and no X11 display in reach | `io.kdos.appbox+private` |
| `tagged` | Tagged, and the box has a way past the tag: it shares the session's runtime directory (a development box, or a box created before it had one), or it can reach the X11 display, whose windows reach KWin untagged | `io.kdos.appbox+tagged` |
| `unconfined` | No tag: the launch was refused, and the box did not start | — |

The file holds `confinement`, `launcher`, `socket` (`tagged` or `refused`), `runtime` (`private`
or `session`), `bus` (`filtered` or `none`), `x11` (`reachable` or `no`), `engine`, `reason` (why
it is not confined, in words for a person) and `time`. The engine string travels to the window
decoration through the security context, and KWin's badge on the title bar shows the state.

**The filtered bus.** The keeper runs `xdg-dbus-proxy` with `--filter`. A box may talk to the
portals (`org.freedesktop.portal.*`), call and hear `org.freedesktop.Notifications`, register a tray
icon with `org.kde.StatusNotifierWatcher`, inhibit the screen saver through
`org.freedesktop.ScreenSaver`, own `org.mpris.MediaPlayer2.*` names so media keys and the media
applet reach it, and own its application's own reverse-DNS name and the names under it, which
GApplication and KDBusService need to stay single-instance. The accessibility bus is added when you
have opted in, and `org.freedesktop.secrets` when the profile grants `secrets`; no grant turns the
filter off. Everything else on
the bus is hidden, among it `org.kde.KWin` (whose screenshot interface answers any caller with the
whole screen), `org.kde.kglobalaccel`, `org.kde.plasmashell`, `org.kde.klipper`, `org.kde.krunner`
and `org.kde.ksmserver`. Interfaces that a desktop process owns beside everything else it serves
are reached by scoped call rules, never by talk rights, because the proxy would otherwise hand the
box every name that process owns.

### The keeper

`kdos-appbox run` becomes `podman exec`, so nothing in it can hold a socket open for the box. The
keeper, `kdos-appbox keeper <box>`, is one process per running box, started detached by the first
launch or the warmup and gone with the box's container:

- **The bus proxy.** It runs `xdg-dbus-proxy` at `<private>/dbus-proxy` with the policy above, and
  the proxy exits when the keeper does, so no proxy outlives the box it served.
- **The socket links.** It watches the session's runtime directory and links the audio and
  accessibility sockets again when the daemon behind one restarts.
- **The launch ledger.** Every launch writes its pid into `<box>.apps`. The keeper watches each one
  exit: closed while the session is logging out, the line is kept and marked, because KWin closes
  every window before the session's save runs; closed any other time, the line goes.

It ends when the container's `conmon` does, which is the one process whose life is exactly the
box's: `kdos-box stop`, `kdos-box gc` and the user services' stop at the last logout all end there.
A second keeper for the same box exits at once.

### Throwaway boxes

```sh
kdos-appbox throwaway [--pack <id>] <file>
```

Dolphin's **Open in a Throwaway Box**, a service menu `genlaunchers` writes on the types a boxed
application opens, runs this. The file is opened by a boxed application, the boxed handler of its
type or the one in `--pack`, in a fresh box made only for it:

| Profile | Effect |
|---|---|
| `network = none` | No interface but loopback |
| `home = private` | An empty home inside the box, not yours |
| `persistence = ephemeral` | Whatever the application writes is destroyed with the container: `--rootfs …:O --rm` over the stack of a pack box, `--rm` over the image of a store box, which `kdos-packd` composes nothing for |
| `devices`, `processes`, `ipc = private` and a `/tmp` of its own | No camera, no view of the session's processes or of what the host keeps in `/tmp` |
| The file, read-only | Bound into the box at its own path and nothing beside it, so a document cannot reach its folder |

The box is named `<launcher-id>.tw<pid>`, so its windows take the application's own launcher id,
its icon, its pin and its box's title-bar colour, while KWin's grant check finds the throwaway's own
profile, which grants nothing. When the application exits, the box is stopped and removed, and its
profile, its private runtime directory, its socket holder and its layer directory go with it.

A path containing `:` or `,` cannot be bound, because the container engine's `--volume` splits on
the one and `--mount` on the other, so such a file is copied into the throwaway's runtime directory
under a name without them and the copy is what the application opens. A throwaway is refused, with
a notification, while the same single-instance application (one whose desktop id is reverse-DNS)
runs in its own box: a second launch would hand the file over the bus to the running copy, with the
network and the home the throwaway exists to withhold. With no boxed application for the type the
notification says so and nothing opens. Any other failure — no pack to compose, a create podman
refuses — posts a "Not opened in a throwaway box" notification with the reason and removes
whatever of the throwaway was made.

### Session save and restore

```sh
kdos-appbox session save
kdos-appbox session restore
```

KDE's own session restore brings back every window whose desktop entry it recorded, and a boxed
window's entry is its launcher, whose `Exec` starts the box, so a boxed application with a launcher
comes back through KDE. What KDE cannot see comes back from here: a box that was running with no
window open, and an application that runs in a box under no launcher of its own (`kdos-box run`,
an exported entry).

`save` is run at Log Out, Restart and Shut Down by plasma-shutdown, from
`/usr/share/kdos/xdg/plasma-workspace/shutdown/kdos-boxes.sh`. By then KWin has closed every
window, so the record is the launch ledger, not the windows: `save` takes the launches the keeper
marked as closed during the logout and those still running, and writes
`$XDG_STATE_HOME/kdos/session-boxes`, one line each, `box<TAB><name>` or
`app<TAB><box><TAB><launcher-id><TAB><argument>…`. `restore` is run at login by the autostart entry
`/etc/xdg/autostart/kdos-boxes-restore.desktop`. It follows the login mode KDE's own restore
follows: only `loginMode=restorePreviousLogout` in `ksmserverrc` restores, an application in its
`excludeApps` is never started, and every launcher KDE's own record names is skipped. A recorded
box is started the way `kdos-box start` starts it, keeper and all; a box with no profile, such as
`kdos-fetch-app`'s `kdos-debian`, is given a plain `podman start` and is never removed or
recomposed.

### Window rules

A box's colour is its security domain, so `genlaunchers` writes one KWin window rule per box into
`kwinrulesrc`, in a group named `kdos-box-<box>`, and asks KWin to reconfigure. The rule matches
the window class `<box>` or `<box>.<stem>`, which is what KWin gives every window of the box's
launchers, and forces the title bar's colour scheme to `KDOS<Accent>` of the box's `accent`, or
`KDOSBox` for a box with none, a grey title bar lighter than any accent's (see
[The box colour](kwin.md#the-box-colour)). A window that draws its own title bar takes no
decoration colour.

Only groups named `kdos-box-<box>` are this program's: they are dropped and written again whole,
every other group and key is kept as it was, and the `[General]` order lists keep your own rules
where they were with the boxes' after them. The rules are written again by `kdos-box create`
(and by `clone` and `import … as <name>`, which create), by `kdos-box remove`, and by
`kdos-box profile <name> accent=<scheme>`, and by a launch that makes a kept box again, so a new
box's title bar takes its accent at once and a removed box's group leaves `kwinrulesrc`. A rule is written for every box an exported or generated
launcher names and every box with a profile, except a profile `kdos-box remove` kept (its first
line is the kept mark) whose box has no container.

## The environment a box gets

A program run with `podman exec` inherits neither the caller's environment nor what the box's init
sets for the programs it starts itself, so every variable below is set explicitly. The reasons behind each one are in
[The session](../03-architecture/session.md#the-environment-a-box-receives).

| Variable | Value |
|---|---|
| `WAYLAND_DISPLAY` | The launcher's tagged socket, by its path inside the box; empty with `wayland = no`, so a toolkit that would guess `wayland-0` finds no display to guess at |
| `PATH` | `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games`; the games directories are where Debian puts its games |
| `XDG_RUNTIME_DIR` | `/run/user/<uid>`, which in a pack box or a store box is its private directory |
| `DBUS_SESSION_BUS_ADDRESS` | `unix:path=` the keeper's proxy |
| `XDG_SESSION_TYPE` | `wayland` |
| `HOME`, `USER`, `LOGNAME` | Yours |
| `LANG` | `C.UTF-8`, whatever yours is: the one UTF-8 locale a Debian userland has without generating one |
| `TZ` | Your time zone, the name `/etc/localtime` points to under `zoneinfo/` (`Europe/Berlin`), read at every launch; unset when the link is missing or does not name a zone path, and the box then keeps its image's UTC |
| `XDG_CONFIG_DIRS` | `/etc/xdg:/usr/share/kdos/xdg`: the box's own defaults first, then KDOS's, the order the host session has |
| `GSETTINGS_BACKEND` | `keyfile`, because no dconf service is reachable over the filtered bus |
| `NO_AT_BRIDGE`, `GTK_A11Y` | `1` and `none`, unless you opt in to accessibility (below) |
| `GTK_USE_PORTAL`, `XDG_CURRENT_DESKTOP` | `1` and `KDE`, so toolkits use the KDE file dialog and the colour preference through the portals |
| `GTK_THEME` | The theme `gtk-theme-name` names in your `~/.config/gtk-3.0/settings.ini` (`$XDG_CONFIG_HOME` when set), with `:dark` when `gtk-application-prefer-dark-theme` is true; unset when the file names none, or a name that is not one word of letters, digits, `.`, `_` and `-`. The settings portal answers no theme name and the box's GSettings holds none, so without it GTK draws Adwaita. A running program keeps the theme it started with |
| `CUPS_SERVER` | `/run/cups/cups.sock`, when that socket exists |
| `QT_IM_MODULE` | `wayland`: input methods reach a box through KWin, which relays to the input method it started. `GTK_IM_MODULE` is left unset, because GTK on Wayland chooses correctly without it |
| Qt theming and anything else a runtime needs | From the pack's metadata (below) |
| `LIBGL_ALWAYS_SOFTWARE` | `1` only when the profile's `render` key resolves to software. See [render](#render) |
| `QT_QPA_PLATFORM`, `SDL_VIDEODRIVER` | `wayland;xcb` and `wayland,x11`: Wayland first and X11 as the fallback, since Qt 5 and SDL 2 would otherwise take `DISPLAY` over `WAYLAND_DISPLAY`. Set before the catalogue's and the pack's `env` rows, so a row that forces X11 wins |
| `DISPLAY`, `XAUTHORITY` | Unless the profile says `x11 = no`: yours, or `:<n>` from the first `/tmp/.X11-unix/X<n>` socket, and the cookie in the box's own runtime directory. A box with `x11 = no` gets an empty `DISPLAY`, so a toolkit with an X11 fallback does not reach for a display it would be refused |

Accessibility is a default, not a policy. The accessibility bus is not linked into a box unless you
ask, so by default every boxed GTK application is told not to look for one, which is a probe that
could only time out. Create `~/.config/kdos/a11y` (an empty file is enough) to opt in for good, or
set `KDOS_A11Y=1` for one launch; `KDOS_A11Y=0` opts out for one launch. The path resolves from
`$HOME/.config`, the same way the box profiles do.

Qt theming and other per-runtime variables come from the software, not from a list in this
program, so the variable and the package that makes it work cannot drift apart. Every launch exports
variables from one of three sources, chosen in `box_env()` in `main.c` by the box's base.

A pack box gets the `env =` lines in the metadata of one pack and of every pack it requires, up to
eight packs in all. The nearest pack wins, so an application can override its runtime. The pack is
the one named by the box's `pack:` base, or, for a box with no `image:` base, the installed pack
whose id is the box's name.

A store box (`base=image:kdos/<id>`, named after its catalogue row) has no vendor label to carry
the fact, so it gets the catalogue's `env <row> NAME=VALUE` rows for its whole chain, base first:
`rt-qt` sets `QT_QPA_PLATFORMTHEME=gtk3` and `QT_STYLE_OVERRIDE=Fusion`, `rt-kde` sets
`QT_QPA_PLATFORMTHEME=kde`, `app.heaptrack` sets the `rt-qt` pair again (`rt-qt` carries the
Breeze GTK theme that `gtk3` platform theme reads; without it the theme falls back to a light
palette), `rt-media` points
`LD_LIBRARY_PATH` at PipeWire's JACK library, and `app.surf` sets `GDK_BACKEND=x11`.

Any other image box is themed from its image's labels: `QT_QPA_PLATFORMTHEME=kde` when it carries
`kdos.qt-kde-theme`, otherwise `QT_QPA_PLATFORMTHEME=gtk3`, with `QT_STYLE_OVERRIDE=Fusion` added
when it carries `kdos.qt-gtk-theme`. The `kde` platform theme reads the `kdeglobals` Plasma writes
into the home the box shares, so a KDE application in a box paints in the session's colours.

A value may begin with `$HOME`, which is replaced with your home directory; the catalogue cannot
know your user name.

## Launcher generation

```sh
# every installed pack, into the system tree
kdos-appbox genlaunchers --packs <fs-root>
# every installed pack and store box, into your tree
kdos-appbox genlaunchers --packs --user
# extracted packs, one per subdirectory
kdos-appbox genlaunchers --packs-dir <dir> <fs-root>
# one applications directory
kdos-appbox genlaunchers <desktop-dir> <fs-root>
```

`genlaunchers` reads applications' own desktop entries and writes everything the host needs to
present them as native. You rarely run it by hand: installing and uninstalling run it for you.

| Source form | Reads |
|---|---|
| `--packs` | Every pack of kind `app` that `kdos-packd` lists as `installed` or `mounted`, mounted through the daemon. Available packs are skipped: mounting one to read it is what installs it. With `--user` it also reads every store box, as the [install's regeneration](#install-and-uninstall) does |
| `--packs-dir <dir>` | `<dir>/<pack-id>/usr/share/applications` for each subdirectory. The directory name is the box the launchers dispatch to |
| `<desktop-dir>` | One applications directory. Its entries name no box |

A pack or directory with no `usr/share/applications` contributes nothing; such an application is
reached with `kdos-appbox -b <pack> run <command>`.

An extra argument to `--packs` is refused rather than ignored. The `--packs` and two-argument forms
differ by one argument, so passing both a directory and a root would read the directory as the root
and write the whole set underneath it, exit 0, and leave the real table untouched.

A table that cannot be written stops the run with the path that could not be written, so a run
without permission for the tree it targets never reports a launcher set it did not write. The
summary on standard error reads `<n> launchers, <n> command-only, <n> mime types, <n> shims`; the
two pack forms print how many packs they read before it. Only the `genlaunchers` command prints
the summary and the pack counts: the regeneration an `install`, `uninstall` or `import` runs is
quiet, and prints only its warnings.

### What it writes

| Output | Without it |
|---|---|
| A desktop entry per launcher, `<launcher-id>.desktop`, marked `X-KDOS-Alien=true` | No launcher, and no entry for KWin to match a boxed window to |
| The application's icon, copied out of its pack, or out of a store box's image, to `icons/hicolor/<size>/apps/<launcher-id>.<ext>` | A generic icon: the pack's `/usr/share/icons` is inside the box, and nothing on the host reads it |
| `mimeinfo.cache` beside the entries | No type association is ever consulted, so no boxed application appears in Open With |
| The name-to-command table, `alien-apps` | A shim cannot find what to run |
| A shim per application | The application is not a command |
| One window rule per box in `kwinrulesrc` | Boxed title bars in the session's colours rather than the box's. See [Window rules](#window-rules) |
| Dolphin's throwaway-box service menu, `kio/servicemenus/kdos-disposable.desktop` | No **Open in a Throwaway Box**. See [Throwaway boxes](#throwaway-boxes) |

The MIME cache is written here because this directory is not `/usr/share/applications`, whose cache
`kpkg`'s shared-index trigger writes, and no package install touches it. The service menu's
`MimeType` is the union of every boxed launcher's, it is removed when there are none, and it is
written mode 0755, because KIO runs a service menu from a user's own data directory only with its
execute bit set.

The table is tab-separated, `name`, `command`, an optional third field naming the pack and an
optional fourth naming the launcher id, under a `#` header line. Readers split at the tabs, so a
two-field row still parses.

The user's tree is followed by `kbuildsycoca6`, when it is installed, so the Start menu, KRunner
and the Task Manager see the new launchers before the command returns; without it they see them
when kded next rebuilds its cache.

### Two trees

| Tree | Entries and icons | Table | Shims | Written by |
|---|---|---|---|---|
| System | `<fs-root>/etc/skel/.local/share/applications`, `…/icons` | `<fs-root>/usr/share/kdos/alien-apps` | `<fs-root>/usr/local/bin`, relative links | The image build, and `--packs <fs-root>` as root |
| User | `$XDG_DATA_HOME/applications`, `$XDG_DATA_HOME/icons` | `$XDG_DATA_HOME/kdos/alien-apps` | `~/.local/bin`, absolute links to `/usr/local/bin/kdos-appbox` | `install`, `uninstall`, `import`, and `--packs --user`, as you |

The window rules go to `<fs-root>/etc/skel/.config/kwinrulesrc` in the system tree and
`~/.config/kwinrulesrc` in yours.

`$XDG_DATA_HOME` is `~/.local/share` when the variable is unset; the writer and every lookup
resolve it the same way. Every reader looks in your tree first: the Start menu reads your
applications directory, the dispatcher reads your table before the system one, and
`/etc/profile.d/10-wayland.sh` puts `~/.local/bin` on the `PATH`. That is why an install needs no
root.

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
`X-KDOS-Alien=true`, every icon it exported and every shim it recognises as its own, then rewrites
the table, the MIME cache, the service menu and its window rules whole. One call is therefore the
whole clean-up. A launcher can still outlive its application when the application is removed by a
path that does not run `genlaunchers`, such as a pack removed through `kdos-packd` directly; the
next regeneration removes it.

### What is read

An entry is skipped when it is `NoDisplay=true`, is not `Type=Application`, has no `Name` or no
`Exec`, or carries the `Settings` category without `System`. A skipped `NoDisplay=true` application
entry still lends its `MimeType` to the visible launcher of the same directory whose `Exec` runs the
same program: Debian ships zathura's launcher with no type and each of its plugins as a hidden
entry with `Exec=zathura %U` and the type it opens, so without the loan boxed Zathura claims
nothing. A hidden entry that matches no visible launcher lends nothing, and none gets a launcher or
a shim of its own. The `SKIP_*` tables below remove more by
name. A generated entry carries upstream's `Name`, `Categories`, `GenericName`, `MimeType` and
`Terminal`, upstream's `Keywords` with `box` added (so the boxed copy of a program the host also
carries can be told apart in KRunner), an `Icon` naming the exported icon (upstream's own name when
no icon file could be copied), a `Comment` of `<generic name> — in the box <pack>`, the rewritten
`Exec`, a `StartupWMClass` (see the naming rules), and for a pack application `X-KDOS-Box=<pack>`
and `X-KDOS-Upstream=<upstream desktop id>`.

### Naming rules

- The launcher carries its box and its id. An application belonging to a pack gets
  `Exec=kdos-appbox -b <pack> -l <launcher-id> run <exec>`; one with no pack gets
  `Exec=kdos-appbox run <exec>`. The pack id is also the box name and the stem of the box profile,
  so anything reading the command line learns the box's policy key directly, and `run` skips the
  table lookup.
- The launcher's file name is its **launcher id**: the box itself for a pack with one launcher,
  `<box>.<stem>` for each launcher of a pack with several, where the stem is the shim name with
  every character an application id cannot carry turned into `_`. It is never upstream's own
  desktop id. KWin replaces every tagged window's application id with the launcher id
  `kdos-boxsock` tagged its socket with, and sets the window's class and desktop file name from
  it, so the Task Manager, KWin's rules and the window's icon all look for a desktop entry of
  exactly that name; under upstream's id every boxed window would show a generic icon beside a pin
  that never lights. It also keeps a boxed copy and a host copy of one program apart, which the
  Start menu and Open With need.
- `StartupWMClass` names the class an X11 window of the program has, since an X11 application under
  Xwayland carries no socket tag and the Task Manager matches its window to an entry by its
  `WM_CLASS`: the `WMCLASS` table's class for the program, otherwise upstream's own
  `StartupWMClass`, otherwise the launcher id. A tagged Wayland window is matched by its launcher
  id whatever this says. KWin adds the box's host name to an X11 window's title, as
  `ImageJ <@app.imagej.<host>>`: a box's host name is `<box>.<host>`, which differs from the
  machine's, and KWin shows a window's client machine whenever it differs. A Wayland window's
  title carries no suffix.
- A shim is named after the program its entry runs, through the one function that decides which
  program a command line runs (it skips an `env` prefix and looks inside an `sh -c` wrapper). A
  reverse-DNS entry therefore gets a shim named after the real program. The `RENAME` table wins
  where upstream's name is not the one people use, matched against the desktop id and then against
  the program, and a reserved name, a name a host program in
  `/usr/bin` already answers to, or one with characters outside `A-Z a-z 0-9 . _ + -`, falls back to
  the lower-cased desktop id.
- A terminal entry stays one. The generated launcher keeps upstream's `Terminal=` flag, and the
  opener wraps such an entry in a terminal. Written as false, such a program would start with a
  pipe for input and exit on a usage error, with no window and no message.

### The tables

These live in `launchers.c`.

| Table | Holds |
|---|---|
| `COMMANDS` | `wine`, `winecfg`, `winetricks`: programs used as commands, which get a table row and a shim but deliberately no desktop entry, because a launcher for `wine` with no arguments opens nothing. Written only where `usr/bin/<name>` exists three directories above the applications directory being read |
| `ENTRIES` | Applications that ship no desktop entry at all, with the entry written here: `surf`, which then claims `text/html`, `application/xhtml+xml`, `http` and `https`. A browser with no launcher claims no URL scheme, so installing it would change nothing about what opens a link. Written only where the binary is present, by the same rule, or, for a store box, where the catalogue row's own package list names the package that installs it (`surf`) |
| `RENAME` | 60 upstream ids or programs people do not use, mapped to the names they do: `firefox-esr` → `firefox`, `org.inkscape.Inkscape` → `inkscape`, the program `codium` → `vscodium`, and 57 more |
| `WMCLASS` | The X11 class of a boxed program whose entry does not name it, by shim name: `imagej` → `ij-ImageJ`, `jmol` → `org-openscience-jmol-app-jmolpanel-JmolPanel`, `pymol` → `PyMOL`, `surf` → `Surf` |
| `RESERVED` | Names no shim may take and the sweep must not delete: shell and system tools, `kdos`, `konsole`, the native `git`, `gnuplot` and `mpv`, SoX's `sox`, `soxi`, `play` and `rec`, the package tools, several KDOS commands, and this program's own three names |
| `EXEC_EXTRA` | Arguments needed only because the application is containerised, inserted before any field code and keyed on the program the `Exec` runs, not on the desktop id, which upstream renames: the program `codium` (VSCodium, whose entry is `com.visualstudio.CodeOSS.desktop`) gets `--no-sandbox --ozone-platform-hint=auto --disable-gpu-compositing` |
| `SKIP_HOST_ONLY` | `org.kde.spectacle`. Spectacle captures through KWin's screenshot interface on the session bus, which a box's bus filter hides, so a boxed copy opens on an error; the host's Spectacle is the screenshot tool |
| `SKIP_ROOTLESS_INERT` | GParted, GSmartControl, GNOME Disks, TestDisk, Baobab-as-root and Timeshift: they need raw block devices, which a rootless container cannot give. Use the native recovery tools instead |
| `SKIP_PREFIXES`, `SKIP_BASENAMES` | Entries that are not applications: settings panels, helpers, URL handlers (VSCodium's `com.visualstudio.CodeOSS.UrlHandler`), secondary tools of a suite, and KDE's own daemons' entries; the entries a dependency drops into the image (`debian-xterm` and `debian-uxterm` from xterm under Grace and GRASS, `tkcon-console` under DS9); a second entry for one program (ParaView's plain `paraview`, ZynAddSubFX's ALSA, OSS and multi-channel variants); and the 185 `in.lsp_plug.*` entries, one JACK host per LSP plugin |
| `X11_FORCING` | `GDK_BACKEND=x11`, `CLUTTER_BACKEND=x11`, `QT_QPA_PLATFORM=xcb`, `SDL_VIDEODRIVER=x11`, `MOZ_ENABLE_WAYLAND=0`, `ELECTRON_OZONE_PLATFORM_HINT=x11`, stripped from an `env` prefix on an `Exec` line. The catalogue's `env` rows are where an application that has a Wayland backend but must not use it records so (`app.surf` sets `GDK_BACKEND=x11`); see [Box profiles](#box-profiles) |

Both `COMMANDS` and `ENTRIES` need the binary in the tree being read, as an installed pack has it.
A store install copies only `usr/share/applications` and the icons out of the image, so a store box
has no `usr/bin` to probe: it gets an `ENTRIES` entry when its catalogue row's own package list names
the entry's package, which is how `app.surf` gets its launcher, shim and URL handler, and it never
gets the `wine` shims, which would shadow the host's own `wine` on the path.

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
Every launch path goes through it, and `testing/selftest.sh` checks both directions against the
real Debian lines quoted above.

When `genlaunchers` rewrites a line it keeps `%f`, `%F`, `%u` and `%U` and drops every other field
code. A shim run from a terminal expands the field codes to nothing and appends your own arguments
after the command.

## The open path

`xdg-open` on KDOS is this binary. Everything that opens a link says that word (a mail client's
open-link command, a portal, anything reading `$BROWSER`) and means "whatever this machine opens it
with". The xdg-utils script stays installed at `/usr/bin/xdg-open` and is the last resort, reached by
absolute path, because calling it by name would find this binary again and loop.

Resolution runs in four steps:

1. **What the argument is.** `kxdg_mime_for_arg()` treats an argument that names an existing file
   as a path, whatever characters it contains. Otherwise it types an argument with a scheme as
   `x-scheme-handler/<scheme>` and unwraps `file:` to the path it names. Anything else is a path: a
   directory is `inode/directory`, and a file is typed from the shared MIME database's glob table
   (longest matching suffix wins, then an exact file name, then `application/octet-stream`). Deciding by the
   basename alone would type `mailto:a@b.c` by its `*.c` glob and offer a mail address to a C
   editor. The chooser asks the same function, so both sides agree. The first argument's type is
   used for all of them.
2. **Who handles that type.** The search, in order:

   | Order | File | Section |
   |---|---|---|
   | 1 | `~/.config/kde-mimeapps.list` | Default Applications |
   | 2 | `~/.config/mimeapps.list` | Default Applications |
   | 3 | `~/.config/kde-mimeapps.list`, then `~/.config/mimeapps.list` | Added Associations |
   | 4 | `kde-mimeapps.list`, then `mimeapps.list`, in each `$XDG_CONFIG_DIRS` directory in order | Default Applications |
   | 5 | `applications/mimeinfo.cache` in `$XDG_DATA_HOME`, then each of `$XDG_DATA_DIRS` | MIME Cache |

   `~/.config` is `$XDG_CONFIG_HOME` where that is set, and `$XDG_DATA_DIRS` defaults to
   `/usr/local/share:/usr/share`. The `kde-` prefix is the first name in `XDG_CURRENT_DESKTOP`,
   lower-cased, as the specification asks; it is `KDE` in and out of a Plasma session. Only entries
   that are installed count, and each is counted once. A match in row 1 or 2 ends the search; the
   rows after it are consulted only without one. A match in rows 1, 2 or 4 is a *default*. The MIME
   cache in row 5 is the file `genlaunchers` writes, so a boxed application is found by exactly the
   same lookup as a native one. The search is libkxdg's `kxdg_handlers()`, which reads the same
   `mimeapps.list` chain KDE's own resolution reads, so the handler System Settings' Default
   Applications page shows is the one that opens.
3. **Ask, or open.** With `--choose`, or with more than one candidate and no default, the desktop's
   own application chooser is asked: one `org.freedesktop.portal.OpenURI.OpenURI` call per file
   with `ask` set, through `gdbus`, which puts up `xdg-desktop-portal-kde`'s chooser and launches
   what is picked through its desktop entry, a boxed application's generated launcher included.
   The chooser's list of every application, and its search, leave out entries marked
   `NoDisplay=true` (helpers and settings pages) unless one handles the file's type or is among
   the request's choices (`xdg-desktop-portal-kde`'s `hidden-entries.patch`).
   Otherwise the first candidate opens. Where `gdbus` is missing or the portal refuses, the first
   candidate opens, because losing a dialog is a worse desktop and losing the click a broken one.
4. **Run it.** The entry's `Exec` line is split by `kxdg_exec_split()` with field codes
   *substituted*, since the code is the document and dropping it opens the application with
   nothing. A line with no `%f`, `%F`, `%u` or `%U` gets the documents appended, the same decision
   KIO's launcher makes. At most four documents are passed. The process then replaces itself
   with the application.

With no handler at all, the arguments are passed unchanged to `/usr/bin/xdg-open`, which decodes a
`file://` URL itself; if that is missing too, the command fails with "nothing on this machine opens
<type>".

Every file opened by absolute path is added to the recent-files store under the entry's name
(`kxdg_recent_add()`). A failed write is ignored: a convenience list is no reason to refuse to open
a file.

The shipped defaults are in two files. `/usr/share/kdos/xdg/kde-mimeapps.list` names the desktop
application for each role, and `/etc/xdg/mimeapps.list` holds the few handlers that are not a
desktop application's; a type's default is named in one of them and never both, because the first
directory read would win. `/etc/xdg/mimeapps.list` also carries the removed associations, which
KService and GLib read only from a list of that name: Ark off the OpenDocument and Office Open XML
types, and Kate and Okular's text viewer off `text/calendar`. A new home's `~/.config/mimeapps.list` is copied from the skeleton with an empty
`[Default Applications]` section, so every default comes from those two until you choose one. Your own choice in `~/.config`, which System Settings and Open
With write, is searched before it, so what they show as current is what the opener would run.

The MIME database is compiled on the target. The `shared-mime-info` port is built with
`update-mimedb=false`, and the install trigger of `kpkg`, the package manager (see
[Packaging](../03-architecture/packaging.md)), runs `update-mime-database`, which only works on
the target because the compiler is a target binary.

### Terminal entries

A `Terminal=true` entry is wrapped in the desktop's terminal, run as `<terminal> -e <command>`. The
terminal is what `kb_terminal()` names: `konsole` whenever `WAYLAND_DISPLAY` is set. Konsole draws
sixel, iTerm2 and kitty pictures, so a terminal program that shows pictures needs nothing of its
entry but `Terminal=true`.

On a bare virtual terminal it is not wrapped at all. `Ctrl+Alt+F2`, a serial console and an ssh
login have no session for an emulator to open in, and you are already at a terminal, so
`kb_terminal()` answers nothing and the program runs in place.

A `Terminal=true` entry started through GLib's GIO, rather than through this program, is wrapped by
GLib itself, with the first of `xdg-terminal-exec`, `gnome-terminal`, `mate-terminal`,
`xfce4-terminal` and further names that it finds on the `PATH`. That is the path
`xdg-desktop-portal` takes when a boxed GTK application opens a link. `/usr/local/bin/xdg-terminal-exec`,
a shell script shipped under `fs/`, is the first name GLib finds: it drops a leading `-e` or `--`,
because some callers pass one and a terminal given `-e` twice tries to run it, and runs
`konsole -e` with the rest, so it names the same terminal as `kb_terminal()`. Unlike `kb_terminal()`
it does not look at `WAYLAND_DISPLAY`, so it runs `konsole` on a bare virtual terminal too, where
Konsole has no session to open in.

## Box profiles

A box's profile is `~/.config/kdos/boxes/<name>.conf`: flat `key = value` lines, with `#` comments.
It is read from `$HOME/.config` and not from `$XDG_CONFIG_HOME`, because KWin's sandbox policy reads
the same file for the same box's `grant` and the two must never resolve it differently. The Boxes
page of System Settings edits the same file. `kdos-box create` and
`kdos-box profile <name> key=value` write it, atomically, with every known key spelled out; a box
with no file behaves exactly like one with the defaults.

An application box and a development box differ in `base`, which chooses the lane, and in two keys
that only describe the box, `persistence` and `export`. They do not differ in kind, which is what
lets one manager serve both.

| Key | Values | Default | What enforces it |
|---|---|---|---|
| `base` | `pack:<id>`, `image:<ref>` | none (the first compose records the pack) | Which lane creates the box. `kdos-box create` also takes `box:<name>`, which it resolves to that box's own `pack:` or `image:` base and saves in its place; see [create with a box base](#create-with-a-box-base) |
| `image` | an image reference | none | Shown by `kdos-box list` for a box with no `base`. No launch consults it. `create` with an `image:` base takes the reference from `base`, not from this key |
| `persistence` | `persistent`, `ephemeral`, `frozen` | `persistent` | Pack lane: `ephemeral` is `--rootfs …:O --rm`, a throwaway overlay over the composed root and the container removed when it stops; `frozen` is `:O` alone, the container kept and its writes not. Store lane: `ephemeral` is `--rm`, and `frozen` is not enforced |
| `network` | `host`, `private`, `none` | `host` | Pack and store lanes: `--network host`; no flag, which is podman's rootless private namespace; `--network none`. Development box: `--network host`, `--unshare-netns`, and `--unshare-netns` with `--network none`. `none` leaves the box no interface on any lane |
| `ipc` | `shared`, `private` | `shared` | Pack and store lanes: `--ipc host` when shared, podman's own IPC namespace when private. Development box: `--ipc host`, or `--unshare-ipc` |
| `devices` | `shared`, `private` | `shared` | Pack and store lanes: `/dev` and `/sys` bound in when shared, podman's own when private. Development box: the same binds, or `--unshare-devsys` |
| `processes` | `shared`, `private` | `shared` | Pack and store lanes: `--pid host` when shared, podman's own PID namespace when private. Development box: `--pid host`, or `--unshare-process` |
| `home` | `shared`, `private` | `shared` | Pack and store lanes: shared binds your own `$HOME` in; private gives the box its own home, the `home` directory inside `~/.local/share/kdos/boxes/<name>` (or `$XDG_DATA_HOME/kdos/boxes/<name>`), bound at your home's own path so every launch's working directory exists, and your home is not bound. Development box: your own `$HOME`, or `distrobox --home` |
| `init` | `yes`, `no` | `no` | `--init` on a development box. A pack box and a store box always run `kdos-boxinit` as their init |
| `wayland` | `yes`, `no` | `yes` | No tagged socket in the box's private runtime directory, and an empty `WAYLAND_DISPLAY`. Not enforced on a development box, which shares the session's runtime directory |
| `audio` | `yes`, `no` | `yes` | No PipeWire or PulseAudio socket linked into the private runtime directory. Not enforced on a development box |
| `x11` | `yes`, `no` | `yes` | Whether the box is handed the X11 display and the session's cookie. `yes` lets a program written for X11 alone (Tk, Java's AWT and Swing, Motif, FLTK, GTK 2, Xt) open its window; `no` withholds the display, and such a program finds none and exits while the box stays running. A box handed X11 is at most `tagged`. See below |
| `gpu` | `yes`, `no` | `yes` | With private devices, binds `/dev/dri` back into the box |
| `render` | `auto`, `gpu`, `software` | `auto` | Which renderer the box's applications use. See [render](#render) |
| `memory` | a size in podman's spelling of at least 6 MiB, podman's floor, such as `512m`, `1.5g` or `4G` (a number, then `b`, `k`, `m`, `g` or `t` with an optional `b`, in either case; a number with no unit is bytes), or `unlimited` in any case for none; any other value is refused | unlimited (stored empty) | `--memory` on both lanes; a cgroup limit for a pack box created in a login with a delegated cgroup subtree, and a victim preference for `kdos-oomd` everywhere. See [Memory and grants](#memory-and-grants) |
| `cpus` | a number of cores above zero, such as `2` or `1.5`, or `all` in any case for every core; any other value is refused | every core (stored empty) | `--cpus` on both lanes |
| `pids` | a number | unlimited | `--pids-limit` on both lanes |
| `accent` | an accent name, or `none` (also `session`) for none | none (stored as `session`, printed `none`) | The box's colour: its windows' title bars, through a KWin window rule (see [Window rules](#window-rules)), and the Konsole profile `kdos-box enter` opens |
| `autostop` | `90s`, `30m`, `2h`, or seconds | `0` (never) | `kdos-box gc` stops the box once it has been running this long, counted from when it started, and has no window open |
| `grant` | comma- or space-separated names | none | KWin lets this box's clients bind the protocols those names open, and `secrets` adds the password store to its bus. See [Memory and grants](#memory-and-grants) |
| `export` | `auto`, `manual` | `manual` | Nothing: recorded only. See below |
| `display` | free text | `window` | Nothing: carried through untouched |

distrobox has no flags of its own for `memory`, `cpus`, `pids` or `network = none`, so a
development box passes them to podman through `distrobox create --additional-flags`. distrobox
evaluates the command it builds through a shell, so a `memory` or `cpus` value made of anything but
letters, digits and dots is refused with a warning naming it and is not applied. `kdos-box profile`
prints the podman flag for each of these keys, which is the same on every lane.

For the sharing keys, `private`, `yes`, `on`, `1` and `true` all mean private (or yes), and
`shared`, `no`, `off`, `0` and `false` mean shared (or no); for `wayland`, `audio` and `gpu`,
`shared` means yes. `network` takes `host` or `shared` for the host's, `none`, and `private` or
`yes`, `on`, `1` and `true` for a namespace of its own; it refuses `no`, `off`, `0` and `false`,
which could mean the host's or none. `x11` takes the four yes and the four no spellings,
`persistence` its three words, `export` `auto` or `manual`, `pids` a whole number and `autostop`
`0` or a number with an optional `s`, `m` or `h`. A value outside these is never read as the key's
default: `kdos-box create` and `kdos-box profile <name> key=value` refuse it with one line such as
`'network=maybe': network takes host, private or none — nothing was written`, exit 1 and write
nothing. In a hand-edited file the line is ignored, so the key keeps the value an earlier line gave
it or its default, and the line is named on a `!` line under the profile, as is an unknown key. Neither is written back when the profile is rewritten.

### How the profile reports itself

`kdos-box profile <name>` prints each key beside the mechanism that enforces it on the box's own
lane, and a `!` line under any setting it cannot deliver. A pack or store box shows podman's
mechanisms (`--network host`, `a network namespace of its own (podman's rootless default)`,
`--ipc host`, `podman's own /dev and /sys`, `its layer directory's home, bound at $HOME` and so
on), never a distrobox flag; a development box shows distrobox's (`--unshare-netns`,
`--unshare-ipc`, `--unshare-devsys`, `--unshare-process`, `--home`). Which keys cannot be
delivered depends on the lane too: a pack box and a store box see their own runtime directory as
`/run/user/<uid>`, and a development box shares the session's.

- `wayland = no`, `audio = no` and `x11 = no` are enforced by what is left out of a private runtime
  directory, so on a development box each gets a `!` line: the session's socket, PipeWire's socket
  and the X11 cookie are in the directory it shares. `x11 = no` also gets one wherever the session's
  cookie sits somewhere the box can read without being handed it, under `/tmp` or a home it shares.
- `home = shared` always gets one: a program that can write `~/.config/autostart` or a shell's
  profile decides what the next login runs, outside every box, and only `home = private` keeps its
  writes inside the box.
- `persistence = ephemeral` or `frozen` on a development box, and `frozen` on a store box, gets
  one: those keep their writes inside their container, and nothing discards them short of
  `kdos-box remove`.
- `memory`, `cpus` and `pids` get one when the box has no cgroup of its own for them: a store box,
  a development box, or a pack box in a login whose cgroup cannot take it. The note names the
  limits the profile sets: `kdos-oomd`'s budget check is the only enforcement of `memory`, and
  nothing holds the box to `cpus` or `pids`.
- A `grant` word that is no name in the vocabulary gets one, so a typo is never read as a
  permission, and so does an `accent` that is no KDOS accent, whose title bars stay the neutral
  `KDOSBox` scheme. No accent prints as `none`.
- A box whose kept writes are in a live session's memory gets one, worded by lane. A persistent
  pack box whose layer directory is on an overlay or in memory gets `! the writes are in this live
  session's memory and end with it — a pack box keeps them only on an installed system`, with or
  without a [`kdos persist`](kdos-command.md#kdos-persist) store, because `kdos-packd` puts the
  box's upper in the runtime directory's memory and only an installed system keeps it. A store or
  development box whose podman storage is in memory gets ``! the writes are on this live session's
  memory overlay and end with it — a `kdos persist` store keeps them from its next boot``; a
  session whose persistence store is in use (mounted at `/mnt/persist`) keeps that storage, and
  gets no line.
- A box with a container gets a `created` line, right after `base` or `image`, saying what the
  container was made from: `created = matches` when its label holds the profile's create-time
  values, `created = differs` when it does not, and `created = unrecorded` for a container with
  no label. A box with no container gets no `created` line. Under `created = differs`,
  ``! the profile was written after this box was created: <keys> differ — they apply once
  `kdos-box remove <box>` has run and the box is made again`` (`differs — it applies` for one
  key) names the keys, so a namespace or `home` change is never shown as in force when the
  container predates it. Every create labels the container `io.kdos.profile=<digest>`: `v1.`
  and eight hexadecimal digits of a SHA-256 of `<key>=<value>` for each create-time key, in the
  order `base`, `image`, `persistence`, `network`, `ipc`, `devices`, `processes`, `home`, `init`,
  `gpu`, `memory`, `cpus`, `pids`, and the profile is compared with that label key by key. The
  keys a launch reads (`accent`, `grant`, `x11`, `autostop`, `wayland`, `audio`, `render`,
  `display`, `export`) are not in it, so changing one raises no line. A box with no container,
  and a container with no such label, such as one `distrobox create` made by itself, never gets
  the `!` line; the settings page reads `matches` and `differs` as the whole account of a
  labelled container, and keeps its own memory of a change only for an `unrecorded` one.
- `export = auto` triggers nothing. `genlaunchers` writes launchers for every installed pack and
  every store box at once, and `kdos-box export <box> <app>` is the per-application route.
- `audio = no` or `gpu = no` with `devices = shared` cannot be enforced through the device nodes. A
  shared `/dev` cannot have a hole cut in it, and no container flag grants a speaker while denying a
  camera. `gpu` is enforceable in one direction only: with `devices = private` the box has no
  `/dev`, and `gpu = yes` binds `/dev/dri` back.

`display` means nothing to a container flag and nothing on this desktop reads it, but the profile
writer carries it through a rewrite unchanged. A profile writer that kept only the keys it acted on
would delete everybody else's, and a setting that disappears when an unrelated one changes is worse
than no setting.

Namespaces and volumes apply at create time. A namespace or a volume cannot be changed on a running
container, so `kdos-box profile <name> key=value` writes the file and, for a box that has a
container, names the create-time keys that differ from what the container was created with (the
digest in its `io.kdos.profile` label, the comparison `kdos-box profile <name>` and the settings
page make) and tells you to `kdos-box remove <name>` and create it again for them to take effect.
A key an earlier write changed is still named, even by a write that touched only launch-time keys.
A key this write set back to its created value is in effect now:
` <keys> is what the box was created with, so it is in effect now.` (`are` and `they are` for
several) follows the CREATE-time sentence, or stands alone after `<name>: written —` when no
create-time key differs. Either way the line ends with ` The rest applies at the box's next launch.`
when the write also changed a key a launch reads, so one line accounts for every key written. A
container with no `v1.` label, one distrobox made by itself, records nothing to compare with, so the
keys named are those the write changed. For a box with no container yet it says the profile applies
when the box is created: `remove` would delete the profile just written. A change only to keys a
launch reads, on a box whose create-time keys all match its container, prints
`<name>: written — it applies at the box's next launch.` `accent` is a
KWin rule, written again at once, and a change of the accent alone prints
`<name>: written — the title-bar colour changes now.` A write whose every value is one the profile
already holds (`accent=none` over a stored `session` among them) writes nothing, prints
`<name>: unchanged — it already holds those values.` and then the profile. A profile file with a
value its load refused or a key it did not know is written all the same, since the rewrite drops
them, and so is a box with no profile file yet.

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
render      = software    LIBGL_ALWAYS_SOFTWARE=1 (advisory) — <why>
render      = auto        LIBGL_ALWAYS_SOFTWARE=1 (advisory) — <why>
```

`<why>` is `the profile refuses the card` after `software`, and after `auto` either `no render
node on this machine` or `no /dev/dri inside this box`.

### Memory and grants

`memory`, `cpus` and `pids` are passed to the container engine, and the kernel enforces them only
in a cgroup the engine may write. elogind, configured with `DelegateSessionCgroups=yes`, runs each
login in `/sys/fs/cgroup/<session>/session` and gives the user
`/sys/fs/cgroup/<session>/delegated`, the one subtree in which a rootless podman can create cgroups
and enable the memory, cpu and pids controllers. A pack box is created with a cgroup parent there,
a scope named `app-kdos-<box>-<n>.scope` (the box's dashes escaped as `\x2d`), which is the shape
System Monitor and the desktop portal read an application id out of. That needs the user to be
able to write `/sys/fs/cgroup/<session>/cgroup.procs`, the common ancestor of the login and its
subtree, and elogind leaves it root's, so the box is created under podman's default instead, where
the limits are accepted and enforced by nothing, and `kdos-box profile` says so. A
store box takes no session's cgroup, because its writes are in its container and a container whose
cgroup parent went with an old session could never start again. In every case `kdos-oomd` reads
the profiles and, under memory pressure, picks a box that is over its own declared budget first,
ahead of its general preference for boxed processes.

`grant` opens what KWin's sandbox policy otherwise refuses a box's clients. Every client of a
tagged socket gets the globals an ordinary application window needs, and none of those that reach
past its own windows; a box's profile names the sets it may bind beyond that. KWin reads the `grant`
key of `~/.config/kdos/boxes/<instance-id>.conf`, the instance id being the box, once per client,
for the engine `io.kdos.appbox` and no other:

| Name | Opens |
|---|---|
| `window-list` | Listing and acting on every window (`org_kde_plasma_window_management`) |
| `fake-input` | Typing and clicking into other windows (`org_kde_kwin_fake_input`) |
| `screencast` | Recording the screen without asking (`zkde_screencast_unstable_v1`) |
| `data-control` | Reading and setting the clipboard at any time (`ext_data_control_manager_v1`) |
| `layer-shell` | Drawing panels and overlays (`zwlr_layer_shell_v1`) |
| `output-management` | Changing the screens' modes and layout (`kde_output_management_v2`) |
| `dpms` | Switching the screens off and on (`org_kde_kwin_dpms_manager`) |
| `plasma-shell` | Placing its windows as the shell does (`org_kde_plasma_shell`) |
| `input-method` | Passes KWin's filter for the input-method globals, which KWin still offers only to the input method it started, so it opens nothing |
| `secrets` | The password store: `org.freedesktop.secrets` on the box's filtered bus, added by the keeper |

A name not in this table grants nothing, and `kdos-box profile` says so. The security-context
manager and the lock screen's overlay are never granted. A client that is already connected keeps
what it bound; a changed `grant` applies to the box's next client. The globals each name maps to are
in KWin's carried `sandbox-policy.patch`, described in
[The security model](../03-architecture/security-model.md#sandboxed-clients).

## kdos-box

`kdos-box` is the same binary under a second name, managing boxes directly. A box name, and a
snapshot tag, is 1 to 63 characters from `A-Z a-z 0-9 . _ -` and starts with a letter or a digit;
any other name is refused with that rule in one line. `kdos-box --help` (or `-h`, or `help`)
prints the usage on standard output and exits 0, and so does `kdos-box <verb> --help` (or `-h`)
whatever the first word, a verb or not, since no box name starts with `-`; no verb, or an unknown
one without `--help` or `-h` after it, prints it on standard error and exits 2. A verb of the launch path given a name that is no box (`enter`, `run`,
`export`, `clone`'s source, `start`, `stop`) is refused with `there is no box named <name>` before
anything is written, so a typo leaves no profile behind.

| Command | Does |
|---|---|
| `list`, `ls` | Every box with its base, state, persistence, disk use and accent. The disk use is what `~/.local/share/kdos/boxes/<name>` holds (a pack box's writable layer, snapshots and private home) plus, for a box whose base is not `pack:`, what the container itself has written, read from one `podman ps --all --size` per listing (`{{.ListContainer.Size.RwSize}}`), which is where a store box and a development box keep what they install. A pack box is a `--rootfs` container, whose figure podman takes by walking the whole composed stack, so its row counts only its directory, where its writable layer is; a podman that cannot say leaves the directory's figure. Any option but `--json` is a usage error (exit 2). The accent reads `none` for a box with none (`--json` carries the stored token, `session`), and the list includes boxes that have a profile but no container (state `not created`). `--json` prints the same as one JSON document, each box also carrying `confinement` (what its last launch recorded) and `manager` (the container's `manager` label, `distrobox` for any container distrobox made), and a store box (base `image:kdos/<id>`) also carrying `image`, `present` when its image is in the local store and `absent` when its application's uninstall released it, read from one `podman images --format {{.Repository}}` per listing and left out of every row when that call fails. The Boxes settings page and Discover's KDOS backend read these fields. The containers come from one `podman ps --all` whose format reads the label with `{{.Label "manager"}}`: `podman ps` formats a row through a reporter whose `Labels` is one comma-joined string, so `index .Labels` fails the template. When `podman ps` fails, the whole rows it printed before the failure are still listed and the row it stopped inside is not, podman's error goes to stderr, a last line says the list is incomplete (a box with no profile is missing and a state may be wrong), and the exit is 1. The Boxes settings page reads a non-zero exit as an error and does not use its rows: with no listing yet it shows that last line as the error, and after a good listing it keeps the last one on screen under an error banner that names that line |
| `create <name> [key=value ...]` | Writes the profile and creates the box, then prints `<name> created — enter it with: kdos-box enter <name>` and the profile; distrobox's own closing instruction is not printed. A base is required: `base=pack:<id>`, `base=image:<ref>` or `base=box:<name>` (see [create with a box base](#create-with-a-box-base)). Fails if a container of that name exists. Nothing is written until the command line is known good: a word with no `=`, an unknown key, a value its key does not take (see [Box profiles](#box-profiles)), no base, a base that is not one word, `base=image:` with no reference, `base=pack:` with no id and `base=box:` of a box with no pack or image base are each refused with one line ending `nothing was written`, before podman is asked anything. A store base, `base=image:kdos/<id>`, whose image is not in the local store is refused the same way after one `podman image exists`. When `<name>` is `<id>`, the application's own box, the line is `<name>: its application is not installed — \`kdos app install <id>\` builds its image and makes the box — nothing was written`; for a box of any other name it is `<name>: its image kdos/<id> is not installed — \`kdos app install <id>\` builds it, then this create can run — nothing was written`, since the install makes only `<id>` and the named box is created afterwards from the image it built (`deb:<package>` for a `deb.` box, each `_` of its name written back as the package's `+`); a `podman image exists` that fails rather than answering absent is refused with `podman could not look for kdos/<id> in the local store (exit <n>)` instead. A `kdos/` image is only ever built on the machine by that install, so no registry is asked and nothing reaches the network. A create that fails after the profile is written puts the profile back as it found it, deleted, or the text of a profile `remove` kept, so neither a refusal nor a failure leaves a `not created` box behind. A create that succeeds labels the container with its create-time keys (see [How the profile reports itself](#how-the-profile-reports-itself)) and writes the boxes' [window rules](#window-rules) again |
| `enter <name> [command ...]` | Starts the box and runs a login shell (`/bin/bash -l`) or the command inside it. `--window` opens the Konsole window even with no terminal, which is how KRunner opens a box's shell. When `enter` opens a window it does so at once, and the wait for a box not yet set up happens inside it; run in place, `enter` titles the terminal (as a terminal's own program) and waits up to 120 seconds for the box's init, silently for the first 3 seconds, well within which a restart of a box set up before is ready (about half a second); past them it prints, on standard error, `kdos-box: setting up <name> for its first use; this can take a minute` when no start of the box ever finished a setup and `kdos-box: waiting for <name> to start` when one did. A window `enter` waits for its window to close, and when the wait inside it was stopped or ran out it prints that line too and exits 1. See below for when it opens a window and how the wait ends |
| `run [-l <launcher-id>] <name> <app> [args ...]` | The launch path, in this box, with its windows under the launcher id `-l` names. A name that is neither a container, a profile nor an installed pack's id is refused with `there is no box named <name>` before the launch path composes, creates or records anything |
| `apps <name>` | Refuses a name that is no box, as `export` does (`there is no box named <name>`, exit 1, nothing started); otherwise starts the box and lists the desktop ids in its `/usr/share/applications` |
| `export <name> <app>` | Refuses a name that is neither a container, a profile nor an installed pack's id, the names `run` accepts (`there is no box named <name>`, exit 1, nothing written), and an `<app>` the box has no entry or command for. A launcher `$XDG_DATA_HOME/applications/<name>.<stem>.desktop` (`~/.local/share/applications` when the variable is unset) whose command is `kdos-box run -l <name>.<stem> <name> <command> %U`, and a shim `~/.local/bin/<command>@<name>`. `<app>` is a desktop id that `kdos-box apps` lists, or a command. See [kdos-box export](#kdos-box-export) |
| `unexport <name> <app>` | Removes the launcher, the icons `export` copied for it, and the shim, both `<app>@<name>` and `<command>@<name>` |
| `freeze <name> [out.kpack]` | The writable layer as one pack. See below |
| `import <file.kpack> [as <name>]` | Installs a pack through `kdos-packd`, and with `as` creates a box on it |
| `clone <src> <dst>` | A new box on the same base, with a copy of the source's writable layer |
| `snapshot <name> [tag]` | Copies the writable layer to `snapshots/<tag>`. The default tag is the UTC time, `YYYYMMDD-HHMMSS`; an existing tag is refused |
| `snapshots <name>` | Lists them with their sizes |
| `rollback <name> <tag>` | Replaces the writable layer with a snapshot. Refused while the box is running |
| `start`, `restart <name>` | Start recovers a stuck box, creates again a pack box or store box whose container that recovery removed, and composes a pack box first, as the launch path does; it exits 1 for a removed box whose profile records no base. A box with no profile at all, such as a distrobox made with `distrobox create`, is never removed, recreated or composed: it is given a plain `podman start`. A start that fails prints `<name>: <reason>` and exits 1, and the Boxes settings page shows that line after "did not start:". The reason is podman's last line, or for a box that has no container yet `it has no container yet: its application's first launch, or kdos-box create <name>, makes it`. Restart is stop, then start, and does not start a box that would not stop |
| `stop <name> ...` | Never `podman stop`: a box that shares the host's PID namespace and has no cgroup of its own (every store box, and a pack box in a login with no delegated subtree) sits in the login's cgroup, where podman's stop reaches the box's init and the init's descendants (crun's `kill-all-without-cgroup.patch`, which finds them through the parent links in `/proc`) and leaves running every application the box's launches started, each a `podman exec` that descends from its own `conmon` and outlives the init. A `conmon` is the container's when its comm is `conmon` and its command line carries the container's full ID after `-c` or `--cid`; with `-e` or `--exec` it serves an exec session, and without it is the container's main conmon. Stop sends SIGTERM to every application the box's launches started (each process in the init's mount namespace, or with the box's `KDOS_BOX` once the init is gone); to every process of any of the container's exec sessions, whoever started it (each process whose parent chain reaches an exec `conmon`: a `distrobox enter`, or a distrobox application's exported launcher, carries no `KDOS_BOX` and under `--pid host` is no descendant of the init), each sent SIGHUP first, which is what a closed terminal sends and what ends an interactive shell such as `kdos-box enter`'s, which ignores SIGTERM; to every process under the main conmon but the init, which is how the processes a dead init left behind are reached (the main conmon is the container's subreaper, so distrobox's `sleep 15` becomes its child), except the conmon's own exit command and OCI runtime (argv[0] its `--exit-command` or its first `-r`; a value given to an option, such as each `--exit-command-arg`, is never read as an option); to every launch of the user's still waiting for the box to be ready — a `kdos-box run <name>`, a launcher's `kdos-appbox -b <name> … run` or a `kdos-box enter <name>`, found by the wait mark it holds (`$XDG_RUNTIME_DIR/kdos/boxes/<name>.wait.<pid>`, whose start time must still name the process), so it does not go on to launch into the box after the next start; a waiting `kdos-box enter` catches the SIGTERM, prints `kdos-box: <name> was stopped before it was ready` and exits 1, so the calling shell reports an exit status rather than `Terminated`, and when that `enter` is a window's re-entry the `enter` that opened the window prints the line again in the shell that typed it and exits 1 as the window closes — and, with `podman kill`, to its init. It waits up to 10 seconds by the clock for the box to be down and its applications, waiting launches, exec `conmon`s and main conmon gone; then SIGKILL to what is left, the exec sessions' `conmon`s included, and 3 seconds more. The init leaves its orphans under the main conmon as it dies, after the pass that sent it SIGTERM: distrobox's `sleep 15`, or a setup command of an entrypoint stopped within seconds of its start. So every check of the wait, one each 200 ms or so, sends the wait's own signal again (SIGTERM through the 10 seconds, SIGKILL in the 3 after) to each process under the main conmon whose parent chain does not go through the living init, and such an orphan is ended at the next check rather than waited out. The stop asks podman for the container's ID and its init once, as it starts, and tells afterwards from `/proc` whether that init lives (its pid and start time); each check reads `/proc` first and runs `podman inspect` for the box's state only when nothing of the box is alive there, since every `podman inspect` and `podman kill` takes the container's lock, which the exit command holds while it cleans the box. No conmon is matched by its mount namespace or its environment, and the main conmon is never signalled. As soon as the init has exited it runs podman's exit command, `podman container cleanup`, then waits for that command and for every child it still has, the dead init's orphans among them, before it exits; the box's storage stays busy while any process in the container's mount namespace is alive, so the stop ends the orphans and returns only once the main conmon is gone, and a SIGKILLed main conmon runs no cleanup at all. The main conmon stops being a subreaper as it starts its exit command, so a process whose parent was one of the orphans and has died is reparented past it; while the init is gone, the stop therefore remembers the mount namespace of every process it found under the box's conmons (never its own, nor the one podman runs its conmons in) for its whole length, and ends every process in one of them in each later pass. A box left `stopped` with nothing of it alive is one whose conmon died without that cleanup, and the stop runs `podman container cleanup <name>` itself; a box whose cleanup ran is `exited` and is not given one. A box left with an exec session, or with its storage still mounted, makes the next `podman start` fail with "device or resource busy"; `podman stop` itself refuses a box with an exec session ("has active exec sessions"), which is podman's own behaviour. Several boxes are stopped together, in one grace period. Exit 0 when every box is down; otherwise each box still up is named with its state and the exit is 1. A box with a profile and no container is down already; a name with neither is refused with `there is no box named <name>` and makes the exit 1. The box's keeper ends with it |
| `remove <name> [--force] [--purge]` | Removes the container and releases its composed stack. The profile (`~/.config/kdos/boxes/<name>.conf`) is kept unless `--purge` is given, and so is `~/.local/share/kdos/boxes/<name>`: a pack box's writable layer, its snapshots and a private home. A store box's writes are inside its container and go with it. Every launcher, icon and shim [`export`](#kdos-box-export) wrote for the box goes with it: each `<name>.*.desktop` entry marked `X-KDOS-Export=true` whose `X-KDOS-Box` is the box with the icons named after its launcher id, and each `~/.local/bin/*@<name>` linked to `kdos-appbox`, after which `kbuildsycoca6` rebuilds the menu's cache; another box's exports stay. The confirmation names the profile's path, names the directory on a second line only when it exists, and ends with `<name>: N exported launchers and commands removed` (`1 exported launcher or command`) when there were any. Under `--force`, and for a box that is not running, it runs the stop sequence first, so no application a stopped box still runs holds an exec session that `podman rm` would refuse over; then `podman rm`, with `-f` under `--force`. Nothing podman prints reaches the terminal. When podman refuses, it prints one line, `<name> was not removed: <podman's last line>`, with every container or image id in podman's line replaced by the box's name and, without `--force`, `` — a running box needs `kdos-box stop` first, or --force`` after it, releases nothing and exits with podman's status: a running box is refused without `--force`. A remove that keeps the profile marks it with a first line, `# kept by kdos-box remove; remove --purge deletes it`, which changes no setting; a `kdos-box create` of the name drops the mark, as does a launch that makes the box again (`kdos-box run`, `start`, or a launcher), which writes the profile once its create has succeeded, and `kdos-box profile` keeps it on a box with no container. `--purge` deletes the profile as well and prints `<name> removed, and its profile <path> deleted`, and releases the box's image when its base is `image:<ref>` (a development box's distribution image, or a store box's `kdos/<id>`): when no other profile names the same base it runs `podman rmi <ref>` and prints `  and its image <ref>, which no other box uses, is released`; when another profile does it prints `  its image <ref> is kept: the box <other> is made of it too`; and when podman refuses, because a container made outside `kdos-box` or a running one still uses the image, it prints `  its image <ref> is kept: <podman's reason>`, ids replaced by `another container`. A store application's runtime images are released by its uninstall, `kdos app remove`. A remove that took anything writes the boxes' [window rules](#window-rules) again, so the removed box's rule leaves `kwinrulesrc`. A box with no container whose profile carries the mark keeps it, deletes its exports, releases any composed stack, prints ``<name> has no container; its profile is kept in <path>; `kdos-box remove <name> --purge` deletes it`` and exits 0, so repeating a remove never deletes what the first said it kept. A box with no container and an unmarked profile, such as the one a store install whose create failed leaves, or any box under `--purge`, has its profile and its exports deleted and any composed stack released, prints `<name> had no container — its profile is removed` and exits 0, releasing its image as well under `--purge`; a name with neither a container nor a profile releases any composed stack and deletes any exports the same way, and exits 0 when there were exports and otherwise prints `there is no box named <name>` and exits 1. When `podman container exists` cannot tell (status 125), it releases nothing and exits with that status. Stop the box before removing it |
| `profile <name> [key=value ...]` | Prints the profile, or sets keys and then prints it |
| `gc [--dry-run] [--session-end]` | Stops boxes past their `autostop` with no window open. See [Warmup and collection](#warmup-and-collection) |

`create` lets the container engine print its errors, since its message is the diagnosis. `enter`
and `apps` print the start's `<name>: <reason>` line and then `could not start <name>`. A pack-lane create that fails reports the engine's exit status, or, when your
home is on overlayfs, says that a box's writable layer has nowhere to go and that a persistent box
needs an installed system.

A `create` with an `image:` base first asks `podman image exists <ref>`. An image already in your
image store, such as one `kdos app install` built, is used as it is, with no network. Otherwise the
create prints that it reaches the network and fetches unsigned content, runs `podman pull <ref>`,
and stops if the pull fails. It then creates the box: with `podman create` for a store image
(`kdos/<id>`), and with `distrobox create` for any other. `KDOS_REQUIRE_SIG`,
the pack daemon's strict signature setting, does not cover an `image:` base; `pack:` is the
offline, verified base.

### enter and the terminal window

`enter` takes one of three courses, by what runs it:

- **Typed at a shell in a terminal**, with no command and without `KDOS_BOX_NOTERM` in the
  environment, or given `--window`, which needs no terminal at all and is how KRunner opens a
  box's shell: it opens a new Konsole window with `konsole --separate`, so the window is a process
  of its own started with this environment. The window uses the box accent's Konsole profile,
  `KDOS-<Accent>.profile` from `/usr/share/konsole` when it exists, its tab is titled
  `<name> — KDOS box`, and it runs `kdos-box enter <name> --shell` inside, which runs in place.
  Konsole is started with its standard output and standard error on `/dev/null`: the window's
  terminal output stays in that window, and nothing Konsole prints, such as Qt's warnings as the
  window closes, reaches the calling terminal. When Konsole cannot be run, `enter` prints
  `konsole: not found` on the caller's standard error.
- **As a terminal's own program**, such as `konsole -e kdos-box enter <name>`, with no command:
  it runs in place, in that terminal, and first titles it `<name> — KDOS box`. It writes two
  sequences: Konsole's OSC 30 (`ESC ] 30 ; <title> BEL`), which replaces the session's tab title
  format, so the tab and the window caption show the name where Konsole's default format
  (`%d : %n`) would keep showing the directory and `podman` whatever OSC 0 says; then OSC 0
  (`ESC ] 0 ; <title> BEL`), which titles other terminals, since they ignore OSC 30.
  A terminal's `-e` program is the leader of the session the terminal made for it (`getsid(0)`
  is its own pid), where a command typed at a shell is the shell's child and never its session's
  leader. A new window here would leave the calling
  terminal open and empty behind it. The shell inside the box may set its own OSC 0 title
  afterwards, which a terminal other than Konsole shows instead; Konsole's tab keeps the name.
- **In every other case** it runs in place and sets no title: when its input is not a terminal,
  when a command is given, or when `KDOS_BOX_NOTERM` is set to any value (only whether the variable
  exists is checked).

Only the in-place courses wait for a box that is not yet set up, one whose container log for its
current start does not yet hold `container_setup_done`, checked as in
[the launch path](#the-launch-path), step 8 (a new `image:` box's first start runs distrobox's init, which
creates the user and installs packages and can take a minute). The window course opens its Konsole
at once, and the window's `--shell` re-entry, an in-place course, waits there. Each in-place course
writes its title first, when it writes one, then waits up to 120 seconds, or the seconds
`KDOS_BOX_SETUP_WAIT` gives (1 to 3600; any other value is ignored), holding the wait mark
described in [the launch path](#the-launch-path), step 8, for the whole wait. The first 3 seconds,
by the monotonic clock, print nothing: a restart of a box set up before, including the first start
after a reboot, is ready in about half a second. A box still not ready then gets one line on
standard error, and its whole container log, read without the current-start bound, picks it: with
`container_setup_done` anywhere in it the line is `kdos-box: waiting for <name> to start`; without
it, `kdos-box: setting up <name> for its first use; this can take a minute`.
`/.containersetupdone` answers only when `podman logs` fails without the marker (a log driver it
cannot read back): distrobox's init creates the file after its first package install, well before
its marker, so a log that reads back without one is a first setup still running. A wait shorter
than 3 seconds prints its line as its time runs out. A pack or store box, which never creates
`/.containersetupdone`, reads as never set up when its log holds no marker. A person is watching
the wait and can stop it, and a first setup over a slow mirror runs past half a minute. A wait that runs out ends with ``kdos-box: <name> did not finish setting up in
<seconds> s — its init's output is in `podman logs <name>` `` and exit 1, and nothing runs in the
box; in the window's `--shell` re-entry, with a terminal as its input, it then prints
`press Enter to close` and holds the window until a line is read, since Konsole closes the
window as the command exits. A `kdos-box stop` of the box
while it waits ends it with `kdos-box: <name> was stopped before it was ready` and exit 1, so a
shell that ran it reports the status, not `Terminated`. The window course runs Konsole
(`--separate`, which returns when its window closes) and waits for it rather than replacing
itself with it, with Konsole's output sent to `/dev/null` and its own kept, so the shell that typed
the bare `enter` hears how the wait ended too: it sets `KDOS_BOX_ENTER_STATUS` to
`$XDG_RUNTIME_DIR/kdos/boxes/<name>.enter.<pid>`, the window's re-entry writes `stopped` or
`timeout <seconds>` there before it exits 1, and the calling `enter` prints the same line on its
own standard error, removes the file and exits 1. With no such file it exits with Konsole's
status. The wait catches the stop's SIGTERM, removes
its mark and puts back the handling it found: `enter` ignores SIGTERM around its wait and is told,
then puts back the SIGTERM handling it was started with before it runs anything in the box, while a waiting `kdos-box run` or launcher leaves SIGTERM at its default and dies of it, never
going on into the box.

The in-place form is `podman exec --interactive [--tty]
--user=<uid>:<gid> --workdir=$HOME --env=KDOS_BOX=<name>`; it sets no other variable. `--tty`
is added, with `--interactive`, only when the input is a terminal of which this process is in the
foreground group; a background job gets neither and `/dev/null` as its input, for the reason given
in [the launch path](#the-launch-path), step 11. `KDOS_BOX`
carries the box's name for a prompt to show; the shipped `starship.toml` does not read it, and its
`container` module names the container by its own means.

### create with a box base

`create` with `base=box:<name>` makes a box on another box's software with an empty writable layer.
It reads the other box's profile, replaces `box:<name>` with that box's own `pack:` or `image:`
base, saves the new profile with it, and creates the box from that base as if it had been named on
the command line. A box whose profile has neither base is refused with `<name> has no pack or image
base to clone`. `kdos-box clone` is the verb that copies the work as well as the software.

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
with an empty workspace, use `kdos-box create <dst> base=box:<src>`.

### Snapshots and rollback

Snapshots exist for pack boxes only, for the reason given under
[freeze, import and clone](#freeze-import-and-clone). A snapshot is a full copy of the writable
layer, made with `cp -a`, so on an ordinary filesystem it
costs as much space as the box has written. It is not a pack: writing a pack back into a writable
layer needs it mounted, and a rollback that needed the daemon would fail exactly when a box is
broken. Rollback deletes the writable layer and copies the snapshot in its place.

### kdos-box export

This is `kdos-box export`, a launcher for one application in one box; it is unrelated to
`kdos-appbox export`, which writes packs to a `.ktar` file. The launcher is named by a launcher id
of the box, `<box>.<stem>`, where the stem is the command it runs, so its windows match it the way
a generated launcher's do. It carries the category `X-KDOS-Box`, `X-KDOS-Box=<box>` and
`X-KDOS-Export=true`.

`<app>` is a desktop id that `kdos-box apps` lists, such as `org.gnome.TextEditor`, or a command.
A name that is neither a container, a profile nor an installed pack's id (the names `kdos-box
run` accepts) is refused first, with `there is no box named <box>`, exit 1, before anything is started or written. Export starts the box and reads
`/usr/share/applications/<app>.desktop` inside it. When that entry exists and has an `Exec`, its
`Exec`, `Name`, `Comment` and `Icon` are taken from it: the launcher runs
`kdos-box run -l <launcher-id> <box>` followed by the entry's own command words and `%U` (the
entry's field codes are dropped), is named `<Name> (<box>)` and carries the entry's `Comment`.

The icon is copied out of the box under the launcher id's name, since the host's icon themes do not
hold an icon that is installed only inside the box. The box is asked, with the icon name as the
shell's `$1` and never part of its script, which files of that name it has under
`/usr/share/icons/hicolor/<size>/apps/` and `/usr/share/pixmaps/`; each is read out with
`podman exec <box> cat`, which follows the symbolic links icon themes are made of, into a scratch
directory under `$XDG_RUNTIME_DIR`, and laid out as a generated launcher's icons are:
`$XDG_DATA_HOME/icons/hicolor/<size>/apps/<launcher-id>.<ext>` for every size, or
`$XDG_DATA_HOME/pixmaps/<launcher-id>.<ext>` when the box has no hicolor copy (`~/.local/share`
when the variable is unset). An absolute `Icon=` path is copied as it is. The entry then says
`Icon=<launcher-id>`; when nothing could be copied it keeps the box's icon name, which a theme on
the host may have. `unexport`, and `kdos-box remove` for every export of the box, delete those
icon files with the launcher. A word that names no entry is the
command itself, and export asks the box whether it has it, running `sh -c 'command -v -- "$1"'`
inside it with the word as `$1`; a box that answers no is refused with `the box <box> has no
command or application named <app>`, exit 1, and nothing is written. A box that cannot be started
or asked is exported unchecked, with a warning. The launcher of a bare command is named after it,
runs in a terminal (`Terminal=true`), takes the terminal's icon (`utilities-terminal`) and the
subtitle `<app> in the box <box>`, since a command has no window or icon of its own. An `<app>`
that starts with `-` is an option, not an application, and is a usage error for both `export` and
`unexport`. `StartupWMClass` is the launcher id.
The launcher is written to `$XDG_DATA_HOME/applications`, or `~/.local/share/applications` when the
variable is unset, which is the directory `genlaunchers --user` writes and the Start menu reads.

The shim is `~/.local/bin/<command>@<box>`, where `<command>` is the program the entry runs (for
`org.gnome.TextEditor`, `gnome-text-editor`), or `<app>` itself when it named no entry. It is an
absolute link to `/usr/local/bin/kdos-appbox`, which dispatches on its own name. That name is
looked up in the name-to-command tables like any shim; `export` writes no row for it, so the lookup
misses and the name is split at its last `@` into a command and a box, and the shim runs what
`kdos-box run <box> <command>` runs, with its own arguments appended. The box part must be a box
name (`[A-Za-z0-9._-]`, not starting with `.` or `-`) and the command part must be non-empty; any
other miss answers "unknown alien app". The shim is also a link the launcher sweep recognises as
its own, so the next `kdos-appbox install`, `uninstall` or `import` deletes it; the desktop entry,
which carries no `X-KDOS-Alien` key, is kept.

`unexport` removes the launcher, the `<app>@<box>` shim and the `<command>@<box>` shim. It takes the
command from the box's entry when the box is already running, and otherwise from the `Exec` line of
the launcher export wrote, so it does not start a stopped box to learn which shim to delete.
`kdos-box remove` deletes every export of the box it removes, so no launcher outlives its box.

## Warmup and collection

At login, before Plasma starts, `kdos-session-wrapper` (SDDM's session command) runs
`nice -n 10 kdos-appbox warmup` in the background. With one box per application, starting a single
shared box would help nothing, so the warmup reads your favourites, `~/.config/kdos/favorites`, and
composes and starts the box behind each line, up to eight, each with its keeper, so the first launch
finds the bus proxy up too.

A line's first word is a desktop-entry id (the file name under `~/.local/share/applications` or
`/usr/share/applications`, without `.desktop`) or a box's name; anything after it is a note, except
a `code=XX` word, which the KDOS runner reads. A generated launcher names its box with
`X-KDOS-Box`. An entry without that key, written by hand, names it through its `Exec`: `-b <pack>`
where it is named before `run`, otherwise the command after `run`, resolved as `run` resolves it,
or a shim the table knows. A word that is no entry but has a box profile is a box by name. A host
application's entry names no box and is skipped without counting towards the eight; the shipped
file lists the applications the desks pin, so a fresh account whose pinned application is boxed
starts that box warm. A lock (`$XDG_RUNTIME_DIR/kdos-appbox.warmup.lock`) makes a second warmup a
no-op rather than a queue. The panel's pinned launchers are Plasma's own and are not read from this
file.

`kdos-box gc` gives idle boxes back. It looks only at running boxes, and stops only those whose
profile sets `autostop`; the default is never. The time is counted from when the box started, not
from when its last window closed, because the engine keeps no record of when a box was last used. A
box past its time is stopped only after asking KWin, through `kdos hey boxes`, whether the box has a
window open, matching each box name the list gives whole. A box with a window is left alone, and so
is every box when there is no session to ask, because "cannot tell" is not "no window". Each box it
acts on is printed as `<box> up past <N>s, no window — stopping`, or `— would stop` under
`--dry-run`; a run that stops nothing prints `nothing to stop`.

It runs at two moments:

- **Every ten minutes**, from the `box-gc` row of `/etc/kdos/user-services/timers.d/40-box-gc.timer`,
  at `nice -n 10`, under your own user services rather than a root service, because the boxes are
  yours and so is the question asked about their windows.
- **At the end of a session**, from `/usr/share/kdos/xdg/plasma-workspace/shutdown/kdos.sh`, which
  plasma-shutdown runs at Log Out, Restart and Shut Down after KWin has closed every window. It runs
  `kdos-box gc --session-end`, which does not ask about windows: every window is closed by then,
  and a desktop that is quitting may not answer.

Boxes that a launch or the warmup creates get a default profile with no `autostop`, so with the
defaults a warmed box keeps running past a Log Out. Every container of yours stops at your **last**
logout, when turnstile stops your user services (`/etc/kdos/user-services/40_boxes.sh`), unless you
linger. To have warmed boxes given back sooner, set `autostop` on your favourites' boxes, for example
`kdos-box profile app.hugin autostop=30m`: the box is then stopped once it has been up longer than
that since it started and has no window, however recently it was last used.

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
| `/usr/share/kdos/alien-apps`, `$XDG_DATA_HOME/kdos/alien-apps` | The name-to-command tables, system and yours. Yours is under `~/.local/share` when `XDG_DATA_HOME` is unset; `genlaunchers` writes it and every lookup reads it at the same path |
| `~/.config/kdos/boxes/<name>.conf` | Box profiles |
| `~/.local/share/kdos/boxes/<name>/` | A pack box's writable layer (`upper`), its snapshots, and a private home (`home`) |
| `/run/user/<uid>/kdos/boxes/<name>/` | A pack box's merged root (`root`), and its writable layer when the home cannot hold one |
| `$XDG_RUNTIME_DIR/kdos/box/<name>/` | The box's private runtime directory, seen inside it as `/run/user/<uid>` |
| `$XDG_RUNTIME_DIR/kdos/boxes/<name>.state` | Which boundary held for the box's last launch |
| `$XDG_RUNTIME_DIR/kdos/boxes/<name>.apps` | The box's launch ledger, kept by the keeper |
| `$XDG_STATE_HOME/kdos/session-boxes` | What `session save` recorded for the next login |
| `~/.config/kwinrulesrc`, groups `kdos-box-<name>` | The boxes' window rules |
| `$XDG_DATA_HOME/kio/servicemenus/kdos-disposable.desktop` | Dolphin's throwaway-box menu |
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
| `KDOS_BOXSOCK` | Run this program in place of `kdos-boxsock`, which is how the test suite drives the launch path without a compositor |
| `KDOS_LOCALTIME` | Read the time zone from this link instead of `/etc/localtime` |
| `KDOS_PACK_EXTRACT` | Build time only: where `030_launchers.sh` looks for extracted packs |

## See also

- [Applications](../02-user-guide/applications.md): using all of this day to day
- [Packs and boxes](../03-architecture/packs-and-boxes.md): the pack format, the daemon and composition
- [The daemons](daemons.md): the pack daemon, the memory daemon and `kdos-boxsock`
- [The session](../03-architecture/session.md): the environment and what a box shares
- [The security model](../03-architecture/security-model.md#sandboxed-clients): what a box is and
  is not, and KWin's sandbox policy that `grant` extends
- [The kdos command](kdos-command.md): `kdos app`, the everyday front end
- [How KDOS differs](../01-philosophy/how-kdos-differs.md): boxed applications against Flatpak,
  Snap and distribution packages

<!-- book-nav -->
---

*Part IV — Programs, chapter 25.* Previous: [24. Discover](discover.md) · [Contents](../README.md) · Next: [26. The daemons](daemons.md)
