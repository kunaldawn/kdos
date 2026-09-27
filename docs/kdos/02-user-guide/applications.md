# Applications

This chapter is for anyone who wants graphical software on a KDOS machine: a browser, an office
suite, an image editor, a CAD package. It explains where such software comes from, how to find,
install, launch, update and remove it, how to carry a set of it to another machine without a
network, how files and links reach it, and how to manage the *boxes* it runs in. Read
[The desktop](desktop.md) first if you have not used the Start menu; for the file format and the
machinery underneath, see [Packs and boxes](../03-architecture/packs-and-boxes.md) and
[kdos-appbox](../04-programs/kdos-appbox.md).

KDOS compiles its own desktop from source, but not the large graphical applications. Those are
described by a *catalogue* shipped on the medium and built on your machine, in containers, from
Debian packages. What KDOS does compile is listed in
[The ports catalogue](../06-reference/ports-catalogue.md), and how that build runs is told in
[How KDOS is built](../05-developer/how-kdos-is-built.md).

## What an alien app is

An *alien app* is a graphical application that this repository does not compile. It runs in a
*box*: a rootless Podman container of its own, named after the application's catalogue id
(`app.krita`, `app.gimp`). A box shares your home directory by default, so the files you work on
are the same files inside and outside it. It is a packaging boundary, not a security boundary
against you; see [The security model](../03-architecture/security-model.md).

An alien app reaches your machine by one of two routes, and the route decides what kind of box it
gets:

- **Installed from the catalogue**, it is a stack of container images this machine builds: a base
  system, a shared runtime (for example the GTK or Qt libraries), and the application on top. Its
  box, a *store box*, is created over that top image, and the container engine keeps the box's
  writable layer in its own storage.
- **Imported from an exported set**, it is a *pack*: a read-only filesystem image whose hash, and
  signature where there is one, the pack daemon `kdos-packd` checks before it mounts anything. An
  exported pack holds the application's whole flattened root, base and runtime included. Its box, a
  *pack box*, is that pack with a writable layer composed over it, kept under
  `~/.local/share/kdos/boxes/<name>/`.

Either way the application behaves like ordinary system software. It has a launcher in the Start
menu, it opens the file types it claims, it can be your default handler for a type, and it has a
command you can type at a prompt. The container shows only on the first launch of a session, which
is slower.

The line between what KDOS compiles and what it runs in boxes is drawn on purpose: no browser,
office suite or CAD package is ported to build natively, and none is planned. See
[Decisions](../01-philosophy/decisions.md), and
[How KDOS differs](../01-philosophy/how-kdos-differs.md#applications) for how other distributions
deliver the same software.

## What the catalogue carries

The catalogue is the file `/usr/share/kdos/appstore/catalogue` (in the source tree,
`src/packages/kdos-appbox/catalogue`). It describes software rather than containing it: nothing is
baked into the ISO, and an application is built by Podman on the machine that asks for it. The
store, the installer and `kdos app` all read this one file, so a row added to it is offered by all
three.

Counted in the shipped file:

| Row kind | Count |
|---|---|
| Applications (`app`) | 180 |
| Shared runtimes (`runtime`) | 7 |
| Bases (`base`) | 2 |
| Data sets (`data`) | 2 |
| Groups (distinct names on `group` rows) | 7 |
| Commands meant to be typed (`cmd`) | 33, in 22 applications |

The runtimes are `rt-gtk`, `rt-qt`, `rt-kde` (built on `rt-qt`), `rt-media`, `rt-sci`,
`rt-electron` and `rt-wine`. Each application is built on one runtime, or directly on a base, and
each runtime on a base, so a runtime is built and stored once however many applications use it. The
two bases are:

- `base` — Debian 13 (trixie), built from the `debian:trixie-slim` image, with what every graphical
  application needs: fonts, icon themes, the Mesa GL and VA-API libraries, D-Bus and the XDG
  utilities;
- `alpine` — the `alpine:3.24.1` image, about 3 MB of busybox and musl, a clean scratch system to
  try something in. It has no packages of its own, and no application is built on it.

Most rows are packages from the Debian archive. One application Debian does not carry, VSCodium,
is fetched as a `.deb` from its own release page and handed to Debian's package manager, which
resolves its dependencies normally. How the Debian packages are pinned is described under
[The Debian snapshot](#the-debian-snapshot).

### What the rows cover

Between them the rows cover free software across most fields: office and documents, browsers and
mail, raster and vector graphics, photography, 3D and CAD, slicing and CNC, electronics design and
circuit simulation, audio production, video editing and streaming, development environments,
science and mathematics, astronomy, GIS and mapping, amateur radio and software-defined radio,
emulators and games, KDE's application suite, and Wine.

Groups are how the store and the installer offer the catalogue:

| Group | What it holds |
|---|---|
| `essential` | Firefox ESR, LibreOffice, GIMP, Zathura and KeePassXC |
| `office` | Documents, spreadsheets and reading |
| `creative` | Images, audio and video |
| `dev` | Programming and electronics |
| `science` | Computation, modelling and data |
| `make` | 3D modelling, slicing and CAD |
| `games` | Games and emulators |

The installer ticks `essential` by default. The base and runtime an application needs come with it;
they are never chosen on their own.

## Finding and installing

`kdos app` is the command-line half of the store:

```sh
kdos app list                        # what is installed here
kdos app list --all                  # every application and data set, with its state
kdos app groups                      # the seven groups and what is in each
kdos app search image                # match id, name, category and summary
kdos app info app.krita              # name, category, state, what it is built on, a size estimate
kdos app install app.krita           # one application
kdos app install creative            # a whole group
kdos app install app.krita --dry-run # print what would be built, without building it
kdos app install --pending           # the applications chosen during installation
```

`kdos app show` is another name for `info`. Every verb that builds, removes, exports or imports runs
`kdos-appbox` to do the work, so the command line and the store cannot disagree about what
installing means.

Installing builds the application on this machine, one image per layer: installing Krita builds
`kdos/base`, then `kdos/rt-qt`, then `kdos/app.krita`, in that order, and then creates the box. The
next Qt application finds the base and the runtime already built and needs only one more pass of
Debian's package manager. `--dry-run` prints the generated container build files instead, and
counts a shared runtime once, as a real install would. Sizes are shown as estimates everywhere:
what the package manager resolves depends on the snapshot, and shared layers are stored once however
many applications use them.

When you install several applications and one fails, the others stay installed and the failure is
named, as `N of M did not install`. Nothing is rolled back.

An application that is useless without a data set names it with a `needs` row, and installing the
application builds the data set too. KiCad is the one such pair: installing `app.kicad` also builds
`data.kicad-packages3d`, the 3D model library, which is kept out of `app.kicad` because of its size.
`data.tesseract-langs`, language files for Tesseract OCR, belongs to no application and is installed
by name. A data set is built but not grafted into its application's box; see
[What does not work](#what-does-not-work).

`kdos app install --pending` reads `/var/lib/kdos/apps-pending`, where the installer records the
groups you ticked when it did not install them: it had neither a set to import nor a network, or
the import or the build failed. After a failure the file is kept, so running the command again does
the rest. The file belongs to root and the command runs as you, so it stays in place after a run in
which everything installed too; a further `--pending` then tries the same groups again and reports
each application as not installed because its box exists. Delete the file once everything has
installed:

```sh
sudo rm /var/lib/kdos/apps-pending
```

The first login after installation says in a notification that the chosen applications are ready to
install, once; delete `~/.config/kdos/apps-offered` to see it again.

Two limits apply before you start:

- **Installing needs a network and takes minutes**: several for one application, the better part
  of an hour for a large group. An exported set installs offline instead; see
  [Carrying a set to another machine](#carrying-a-set-to-another-machine).
- **On a live session, what you install does not outlast the session.** Installing works there:
  the container engine's storage is pinned to `fuse-overlayfs`, which can stack its layers on the
  live session's overlay. But the images and the box are written under `$HOME`, which on a live
  session is that overlay, held in memory and lost at power-off unless the session has a
  persistence store (`kdos persist`; see
  [Boot and init](../03-architecture/boot-and-init.md#the-live-medium-and-persistence)). Install
  KDOS to disk to keep them. `kdos doctor` reports a `$HOME` on an overlay.

### The store

`kdos-store` is the same catalogue as a window, titled *Applications*. Open it from the Start menu,
or with `kdos menu summon setup.applications`. Its tabs are `Groups`, `All`, up to nine category
tabs ordered most-populated first, and `Installed`. The tab row holds twelve tabs at most, `Other`
never gets one, and an application whose category has no tab is found under `All`. Each row shows
the application's name, the runtime it is built on and an estimate of its size; on the `Groups` tab
each row shows the group, how many applications it holds and their size. An application that is
already installed is marked `[·]` and cannot be ticked. The footer keeps a running size estimate of what you
have ticked.

| Key | Action |
|---|---|
| `Left`, `Right` | Previous or next tab |
| `Space` | Tick or untick the row; on the `Groups` tab, every application in the group |
| `Enter` | Install the application under the cursor; on the `Groups` tab, tick or untick the group |
| `F5` | Install everything ticked |
| `F6` | Import the set at `~/apps.ktar` |
| `F7` | Export everything ticked to `~/apps.ktar` |
| `r` | Refresh, after an install finishes |

The store builds nothing itself. An install, import or export runs `kdos-appbox` in a terminal
window of its own, so you can read the package manager's output and close the store while it works.
On a virtual console with no terminal emulator to open, the store says so and points you to
`kdos app install`. The store takes no file name: to export to or import from another path, use
`kdos app` at a prompt.

## Carrying a set to another machine

```sh
kdos app export ~/apps.ktar creative app.vscodium   # on a machine that has them installed
kdos app import ~/apps.ktar                          # on the other machine: the SELECTION ids
kdos app import ~/apps.ktar app.krita                # only part of the set
```

Export flattens each installed application's image, with its base and runtime, into one pack and
puts the set in one `.ktar` file, together with an index of every pack's hash. An application named
in the selection but not installed here is skipped with a message. The index is signed when the
variable `KDOS_PACK_KEY` names a readable key file; without one the set still exports and imports,
and the export states that the index is unsigned. Import does not read the index.

Import hands each pack to `kdos-packd`, which checks the pack itself, against the hash the pack
carries and against its signature where it has one, so a tampered pack is refused rather than run. An imported application is
therefore verified where one built from the catalogue is not, and no network is needed at any
point. Importing needs `kdos-packd` running and an account in `wheel`, because the daemon's socket
accepts only root and `wheel`. A pack the daemon refuses is named and skipped; the rest of the set
still imports, and the command ends with a count of what imported and what failed.

The archive holds a `SELECTION` file, plain text with one id per line, so a set can be read,
compared and edited by hand. A group named on the export command line is written as a `group` line
and its members are not listed under it. An import with no ids reads only the id lines and skips
every `group` line, so it imports the applications that were named individually and not the
members of a group. In the example above, `kdos app import ~/apps.ktar` imports VSCodium alone; the
creative applications the archive carries are imported by naming them:

```sh
kdos app import ~/apps.ktar app.gimp app.krita   # members of a group, named one by one
```

The installer looks for a set on a stick too; see
[Installation](installation.md#8-applications) for what it does with one. On a live session an
imported application runs, but its box's writable layer is held in memory and lost at power-off.

## Launching

Start an installed application from its row in the Start menu or the launcher, by typing its
command, or by id with `kdos app launch`:

```sh
krita                      # the command on your PATH
kdos app launch app.krita  # by id; installs it first if it is not installed
```

![The launcher: full-screen search over the same application index the Start menu uses](../../screenshots/launcher.png)

The command is a *shim*: a symbolic link named after the application that points at `kdos-appbox`,
which reads its own name, looks the application up and starts it in its box. Shims for applications
you install go in `~/.local/bin`, which the login profile puts on your `PATH`; the table that maps
each shim to its box is `~/.local/share/kdos/alien-apps`. `kdos app launch` on an application that
has no launcher, because it is a command-line tool, says so and names `kdos app info`.

A first launch is slow, because a container has to be created or started and its first process
has to finish setting the box up before anything runs in it. A cold launch, with no container at
all, has been measured at about 18 seconds; a launch into a box that is already running skips all
of that. That is why the Start menu marks boxed applications `[box]`, and why a "Starting
app" notification appears while a box comes up.

It is also why, when you log in, the session runs `kdos-appbox warmup` in the background at
`nice` 10. The warmup reads your pinned applications from `~/.config/kdos/favorites`, where each
line is a desktop id optionally followed by a launch code such as `code=WW`, and starts the boxes
of up to eight of them, one at a time. It takes the whole line as the desktop id, so a line that
carries a launch code names no desktop entry and is skipped; every line of the shipped list
carries one, so on a new account the warmup starts no box until you pin a boxed application
without a code. A second warmup while one is running does nothing.

To see where a launch spent its time, read `$XDG_RUNTIME_DIR/kdos-appbox.trace`, which gets one
timestamped line per stage.

### Adding a terminal program to the menu

`kdos app tui` gives a terminal program a Start-menu entry of its own, without editing a desktop
file by hand. The Start menu's route `setup.tui` runs the same command.

```sh
kdos app tui add "Disk usage" ncdu                     # prints the entry's name, kdos-tui-disk-usage
kdos app tui add btop btop --icon utilities-system-monitor --category System
kdos app tui ls                                        # the entries this command made
kdos app tui rm disk-usage                             # remove one
```

| Option | Effect |
|---|---|
| `--float` | Write `X-KDOS-Float=true`: ask for an unanchored window |
| `--size COLSxROWS` | Write `X-KDOS-Size`: the terminal's size, at least `4x2` |
| `--icon NAME` | The menu icon (default `system-run`) |
| `--category X` | The menu category (default `Utility`) |

`--float` and `--size` are hints that only `kdos-term` honours. An entry this command writes names
no terminal (it carries no `X-KDOS-Term` key), so the desktop opens it in `foot`, which is not
given either hint: the window opens at `foot`'s default size and the compositor places it.

The display name becomes the file name: lower case, with every run of characters other than
letters and digits turned into one dash. Entries are written to
`~/.local/share/applications/kdos-tui-<name>.desktop`; the prefix keeps an entry from shadowing a
shipped one of the same name. `rm` takes the name with or without the prefix, and removes only an
entry this command wrote, which it recognises by the `X-KDOS-TUI=true` key. `kdos app tui` needs
neither `kdos-packd` nor membership of `wheel`.

## Opening files

Double-clicking a file, or running `kdos-appbox open <path>`, finds the file's type from its name
and then a program to open it. The lookup is the freedesktop one: the file name against the shared
MIME database's glob table, where the longest matching suffix wins, then `mimeapps.list` (default
applications first, then added associations), then each `mimeinfo.cache`. Boxed applications are
found exactly like host ones, because installing one writes its file types into the same places.

```sh
kdos-appbox open report.pdf             # open with the default handler
kdos-appbox open --print report.pdf     # print the type, the candidates and the handler, and open nothing
kdos-appbox open --choose report.pdf    # pick a handler
```

When more than one program claims the type and none is set as the default, `kdos-appbox open` asks,
with `kdos-openwith`; `--choose` asks whatever the defaults say. `kdos-openwith` is also the "Open
with" dialog of the desktop, and its *Always use this application* box makes the choice the default
for that type. A boxed application can be the default handler for a type. When nothing claims a
type, the request goes to the system's `xdg-open` script. One call opens up to four files, all with
the handler chosen for the first.

`xdg-open` on KDOS is another name for the same program, so links from any program go to your chosen
handler. Links clicked *inside* a box open on the host: a boxed application asks the desktop portal
(a session service that carries requests such as "open this link" out of a box; see
[Portals](../03-architecture/session.md#portals)) to open them, and the portal launches the host's
handler, so a help link in a boxed application opens in your browser.

Printing works from inside a box. The host's print service socket, `/run/cups`, is shared into a box
when the box is created, and the application's own print dialog talks to it directly. Because a
box's shared directories are fixed at creation, a box created while the print service was not
running cannot print until it is created again; see
[Profile changes and re-creating a box](#profile-changes-and-re-creating-a-box).

## Commands that live in boxes

Not all boxed software is an application you click. Twenty-two catalogue applications carry a
program meant to be typed, thirty-three in all: `wine`, `gmic`, `ngspice`, `solve-field`, `cp2k`,
`grib_ls`, `gmx`, `java`, `glxgears` and the rest, listed in the catalogue as `cmd` rows. They are
solvers, converters, toolchains and tools driven from a prompt, and they get no menu entry: a
launcher for `wine` with no arguments would open nothing. To see them all:

```sh
grep '^cmd' /usr/share/kdos/appstore/catalogue
```

Run one in its application's box with `-b`:

```sh
kdos-appbox -b app.wine run wine setup.exe
kdos-appbox -b app.gromacs run gmx mdrun -h
```

`-b` (or `--box`) names the box. Without it, `kdos-appbox run` works out the box from an installed
application's launcher, and a command-line tool has none.

Where a shim exists, typing the name is shorter. Shims are made for the programs an application's
own desktop entries name, and for `wine`, `winecfg` and `winetricks` wherever the box carries them.
Most command-only tools have no desktop entry and so no shim; use `kdos-appbox -b` for those. A shim
is never made under the name of a host tool (`git`, `gnuplot`, `mpv`, `python3`, the shell and
file tools, and KDOS's own commands among them), because `~/.local/bin` comes ahead of the system
directories on `PATH` and the shim would take the name over from the host program.

## Updating

An application is rebuilt only when you ask for it. `kdos app install` reuses every image that
already exists, so installing an application that is already here builds nothing: each image
reports that it is already here, the step that creates the box fails because the box exists, and
the command ends by reporting `1 of 1 did not install`. To rebuild an application, remove it and
install it again:

```sh
kdos app remove app.krita
kdos app install app.krita
```

This rebuilds the application's own image, and any runtime that no other installed application
was using. Neither the base image nor the pulled `debian:trixie-slim` is removed, so under
`snapshot = auto` the rebuild reads the same snapshot date as before. Packages move to a newer snapshot only when the catalogue names a later
date, or when this machine pulls a newer `debian:trixie-slim`.

An imported set behaves differently. Each exported pack is stamped with the time of the export as
its version, and `kdos-packd` keeps one superseded version of each pack on disk when a newer one is
imported, so an update can be undone. That is `retain` in
[`/etc/kdos/packd.conf`](../06-reference/configuration.md#etckdospackdconf), which ships set to `1`;
`retain = 0` keeps none.

Importing a newer set installs the new pack even for an application whose box already exists, but
the step that creates the box then fails, and the import reports `<name>: the pack installed and the
box did not` and counts the application as failed. To move such a box onto the new pack, remove the
box first with `kdos-box remove <name>` and then import; the box's writable layer and settings stay,
and the new box is composed on the newest version of each pack.

### The Debian snapshot

The Debian packages come from a pinned snapshot of the Debian archive, set by the catalogue's
`snapshot` key:

| Value | Effect |
|---|---|
| `auto` (shipped) | The date is read from the sources file inside the `debian:trixie-slim` image this machine pulled, so the packages installed on top always match the system under them |
| a date, such as `20260824T000000Z` | Every machine builds against the same snapshot |
| `off` | The live Debian archive; builds are not reproducible |

The `debian:trixie-slim` tag moves as Debian publishes new images, so under `auto` two machines
agree on the snapshot only when they pulled the same image. If the date cannot be read out of the
image, the build warns that it is using the live archive and carries on rather than refusing.

## Removing

```sh
kdos app remove app.krita     # one application
kdos app remove creative      # a whole group
```

Removing an application:

- removes its box; the box's settings in `~/.config/kdos/boxes/<name>.conf` stay, so installing it
  again picks them up;
- deletes the application's image, and any runtime image no other installed application uses;
- never deletes the base image, which every application sits on;
- regenerates the launchers, shims and file-type associations from what remains.

What happens to the work inside the box depends on the kind of box. Your own files in your home
directory are never touched, because the box shares your home directory. A store box's writable
layer, meaning anything the application wrote outside your home directory, lives in the container
and is removed with it. A pack box's writable layer stays in `~/.local/share/kdos/boxes/<name>/`,
and its packs stay in `kdos-packd`'s store.

## Boxes

Every application gets its own box, named after it. You do not need to manage boxes to use
applications, but `kdos-box` manages them directly:

```sh
kdos-box list                                      # every box: its image, state and profile
kdos-box enter app.krita                           # a shell inside one, in a new terminal window
kdos-box create scratch base=image:alpine:3.24.1   # a new box; base= also takes pack:<id>
kdos-box start | stop | restart | remove <name>
kdos-box gc                                        # stop idle boxes now (--dry-run to preview)
```

`kdos-box enter` sets `KDOS_BOX` to the box's name inside it, which a shell prompt can show. The
full command set, including `run`, `apps`, `export` and `import`, is in
[kdos-appbox](../04-programs/kdos-appbox.md#kdos-box).

### A pack box's writable layer

Five commands work on the writable layer under `~/.local/share/kdos/boxes/<name>/`, and so apply
to pack boxes (imported applications and boxes created with a `pack:` base). A store box keeps its
writable layer in the container engine's storage, where these commands do not reach.

```sh
kdos-box snapshot app.krita before-plugin          # a full copy of the writable layer, tagged
kdos-box snapshots app.krita                       # list them
kdos-box rollback app.krita before-plugin          # put one back
kdos-box clone app.krita krita-test                # a new box on the same base, with the work copied
kdos-box freeze app.krita                          # what the box changed, captured as one pack
```

`kdos-box freeze` packs only what the box has *written* over its base, which on a real working box
is a small fraction of its whole root. A pack box is created over mounted packs rather than a
container image, so there is no image to commit, and freezing is the way to capture its state.

To start a second box from the same software with an empty workspace, read the first box's `base`
with `kdos-box profile <name>` and pass it to `kdos-box create`.

### Profiles

A box's settings live in `~/.config/kdos/boxes/<name>.conf`, its *profile*. `kdos-box profile
<name>` shows it, and `kdos-box profile <name> key=value` changes it. The Boxes page of
`kdos-settings` edits the same file. Every key maps onto something the box enforces, and the printed
profile marks any setting it cannot deliver on this machine. See
[kdos-appbox](../04-programs/kdos-appbox.md#box-profiles) for the keys.

### Profile changes and re-creating a box

Namespaces (the kernel namespaces that separate the box's processes, network and users from the
host's) and shared directories are set when a box is created and cannot be changed on a running
container, so a profile change to them takes effect only when the box is created again. Remove the
box with `kdos-box remove <name>`; its profile is kept. Then:

- for a pack box, the next launch creates the box again from the kept profile;
- for a store box, run `kdos app install <id>`, which finds every image already built and creates
  only the box.

### Idle boxes

Your session runs `kdos-box gc` every ten minutes. It stops a running box whose profile sets
`autostop` and which has been running for longer than that `autostop` time. It asks the compositor
first: a box with a window on the screen is never stopped, and when there is no session to ask,
nothing is stopped. `autostop` is off (`0`) unless you set it, for example with `kdos-box profile
app.krita autostop=30m` or on the Boxes page of `kdos-settings`.

## Accessibility inside a box

The host runs no accessibility registry, so the desktop cannot be read by a screen reader. A boxed
application can use its own toolkit's accessibility support, which the box's image carries. It is
off by default, so that no application waits at start-up for a registry that is not there;
`touch ~/.config/kdos/a11y` turns it on for every box, and `KDOS_A11Y=1` for one launch. See
[Accessibility](accessibility.md).

## What does not work

- **Applications that need raw block devices**: partitioners, SMART tools, disk utilities. A
  rootless container cannot open a disk, so their launchers are left out even when a package
  carries them. Those jobs belong to the native tools on the host (`parted`, `testdisk`,
  `ddrescue`, `partclone`), which run as root.
- **Applications that require a particular compositor's private protocols**, such as KDE's
  screenshot tool Spectacle, which needs KWin. Screenshots are the host's `kdos-shot`.
- **Microsoft core fonts for Wine**, and `winetricks`. Both download Windows components at run
  time, which an offline system cannot promise, so a Windows program asking for Arial gets a
  substitute.
- **GLX for X11 applications.** Xwayland is built without GLX, so an X11 client that draws through
  GLX gets no OpenGL. X11 clients that use EGL, and Wayland-native applications, are unaffected.
- **Data sets reaching their applications.** The catalogue's `graft` and `boxgraft` rows say where
  a data set should appear, but neither `kdos app install` nor an exported pack applies them. A
  data set installed from the catalogue is built as an image of its own, and KiCad in its box does
  not see the 3D model library.

## See also

- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the pack format, composition and the
  catalogue's machinery
- [kdos-appbox](../04-programs/kdos-appbox.md) — the launcher, the box manager, and every profile key
- [The desktop](desktop.md) — the Start menu, the launcher and the file manager
- [Theming](theming.md) — how boxed applications get the palette
- [The kdos command](../04-programs/kdos-command.md) — `kdos app` and `kdos doctor` in full
- [Known gaps](../06-reference/known-gaps.md) — what does not exist yet, across the system

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 8.* Previous: [7. The desktop](desktop.md) · [Contents](../README.md) · Next: [9. Theming](theming.md)
