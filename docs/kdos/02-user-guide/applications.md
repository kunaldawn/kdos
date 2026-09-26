# Applications

This page is for anyone who wants graphical software — a browser, an office suite, an image
editor — on a KDOS machine. It covers finding and installing applications, launching them, opening
files with them, moving a set of them to another machine, updating and removing them, and the
*boxes* they run in.

KDOS compiles its own desktop from source, but not the large graphical applications. Those come
from a *catalogue* shipped on the medium and are built on your machine, in containers, from Debian
packages. Start with [What an alien app is](#what-an-alien-app-is), then
[Finding and installing](#finding-and-installing). For the file format and the machinery underneath,
see [Packs and boxes](../03-architecture/packs-and-boxes.md).

## What an alien app is

An *alien app* is a graphical application that this repository does not compile. It runs in a
*box* — a rootless Podman container of its own, named after the application.

It reaches your machine by one of two routes:

- **Installed from the catalogue**, it is a stack of container images this machine builds: a base
  system, a shared runtime (for example the GTK or Qt libraries), the application on top, and a box
  over that top image.
- **Imported from an exported set**, it is a set of *packs* — read-only filesystem images whose
  hash and signature are checked before anything mounts them — stacked into the box's root with the
  runtime and base packs under the application.

Either way it behaves like ordinary system software. It has a launcher in the Start menu, it opens
the file types it claims, it can be your default handler for a type, and it has a command you can
type at a prompt. You notice the container only on the first launch, which is slower.

The desktop draws that boundary on purpose: no browser, office suite or CAD package is ported to
build natively, and none is planned. See [Decisions](../01-philosophy/decisions.md).

## What the catalogue carries

The catalogue is the file `/usr/share/kdos/appstore/catalogue`. It describes software rather than
containing it: nothing is baked into the ISO, and an application is built by Podman on the machine
that asks for it. The store, the installer and `kdos app` all read this one file.

| | |
|---|---|
| Applications | 180 |
| Shared runtimes | 7 |
| Bases | 2 |
| Data sets | 2 |
| Groups | 7 |
| Commands meant to be typed (`cmd` rows) | 33, in 22 applications |

The runtimes are `rt-gtk`, `rt-qt`, `rt-kde` (built on `rt-qt`), `rt-media`, `rt-sci`,
`rt-electron` and `rt-wine`. Each application is built on top of one runtime, and each runtime on a
base, so a runtime is stored once however many applications use it. The two bases are:

- `base` — Debian, with what every graphical application needs and nothing more;
- `alpine` — about 3 MB of busybox and musl, a clean scratch system to try something in.

The Debian packages come from a pinned snapshot of the Debian archive, set by the catalogue's
`snapshot` key. `snapshot = auto`, which the catalogue ships, takes the date from the sources file
inside the Debian image this machine pulled (`debian:trixie-slim`), so the packages installed on top
always match the system under them. That image tag moves as Debian publishes new images, so two
machines agree on the snapshot only when they pulled the same image. A literal date such as
`20260824T000000Z` pins every machine to the same snapshot; `off` reads the live Debian archive and
gives up reproducibility.

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

The installer ticks `essential` by default. The base and runtimes an application needs come with it
automatically.

## Finding and installing

```sh
kdos app list                        # what is installed here
kdos app list --all                  # everything in the catalogue as well
kdos app groups                      # the seven groups and what is in each
kdos app search image                # match name, id and summary
kdos app info app.krita              # category, state, what it is built on, a size estimate
kdos app install app.krita           # one application
kdos app install creative            # a whole group
kdos app install app.krita --dry-run # what would be built, without building it
kdos app install --pending           # the groups chosen during installation
```

Installing builds the application on this machine, one image per layer: installing Krita builds the
base, the Qt runtime and Krita itself, in that order. The next Qt application reuses the base and
the runtime and needs only one more pass of Debian's package manager. That is also why sizes are
estimates: what the package manager resolves depends on the snapshot, and shared layers are stored
once however many applications use them.

When you install several applications and one fails, the others stay installed and the failure is
named; nothing is rolled back.

Some applications are useless without a data set, and a data set is a catalogue row of its own.
Installing KiCad installs `data.kicad-packages3d` too, which is why its 3D models are not inside
`app.kicad`. `data.tesseract-langs` belongs to no application: it adds language files for the
*host's* `tesseract`, and you install it by name.

Two limits to know before you start:

- **Installing needs a network and takes minutes** — the better part of an hour for a large group.
  An exported set installs offline instead; see
  [Carrying a set to another machine](#carrying-a-set-to-another-machine).
- **Installing from the catalogue does not work on a live session.** There `$HOME` is itself an
  overlay held in memory, so a box's writable layer has nowhere to go and creating the box fails
  with a message saying so. Install KDOS to disk first, or import a set.

### The store

`kdos-store` is the same catalogue as a window: tabs for `Groups`, `All`, one per category, and
`Installed`. It shows each row's runtime, summary and whether it is installed, with a running size
estimate for what you have ticked.

| Key | Action |
|---|---|
| `Space` | Tick or untick the row |
| `Enter` | Install the row under the cursor (on a group row, tick it) |
| `F5` | Install everything ticked |
| `F6` | Import a set |
| `F7` | Export a set |
| `r` | Refresh |

An install from the store runs in a terminal window of its own, so you can read the package
manager's output and close the store while it works.

## Carrying a set to another machine

```sh
kdos app export ~/apps.ktar creative app.vscodium   # on a machine that has them
kdos app import ~/apps.ktar                          # on the other machine
kdos app import ~/apps.ktar app.krita                # only part of the set
```

Export turns each installed application into packs and puts the set in one `.ktar` file, together
with a signed index of every pack's hash. Import hands each pack to `kdos-packd`, the pack daemon,
which verifies it — against the index where it is signed, against the pack's own signature
otherwise — before installing it. So an imported application is verified where one built from the
catalogue is not, and no network is needed at any point. Importing needs `kdos-packd` running and
an account in `wheel`.

A pack the daemon refuses is named and skipped; the rest of the set still imports. Without a
signing key (`KDOS_PACK_KEY`, a path to the key file) a set still exports and imports and every
hash is still checked, and the export says plainly that the index is unsigned.

The archive holds a `SELECTION` file — plain text, one id per line — so a set can be read, compared
and edited by hand.

The installer looks for a set on a stick too; see
[Installation](installation.md#8-applications) for what it does with one. On a live session an imported application runs, but its box's writable layer is held in memory and
lost at power-off.

## Launching

Three equivalent routes:

```sh
krita                      # the command on your PATH
kdos app launch app.krita  # by id; installs it first if needed
```

or its row in the Start menu.

![The launcher: full-screen search over the same application index the Start menu uses](../../screenshots/launcher.png)

The command is a *shim*: a link named after the application that starts it in its box. Shims for
applications you install go in `~/.local/bin`, which is on your `PATH`.

A first launch is slow, because a container has to be created and started. Measured on the
reference machine: **18.3 seconds** cold with no container at all, **0.3 seconds** once the box is
running, and **0.55 seconds** for a second window. That is why the Start menu marks containerised
applications `[box]`. It is also why, when you log in, the boxes of up to eight of your pinned
applications (from `~/.config/kdos/favorites`) are started one at a time in the background, at low
priority.

To see where a launch spent its time, read `$XDG_RUNTIME_DIR/kdos-appbox.trace`.

### Adding a terminal program to the menu

`kdos app tui` gives a terminal program a Start-menu entry of its own, without editing a desktop
file by hand:

```sh
kdos app tui add "Disk usage" ncdu                     # prints the entry's name
kdos app tui add btop btop --float --size 120x40 --icon utilities-system-monitor --category System
kdos app tui ls                                        # the entries this command made
kdos app tui rm disk-usage                             # remove one
```

| Option | Effect |
|---|---|
| `--float` | Open the terminal as a floating window rather than in the tiled layout |
| `--size COLSxROWS` | The terminal's size, at least `4x2` |
| `--icon NAME` | The menu icon (default `system-run`) |
| `--category X` | The menu category (default `Utility`) |

Entries are written to `~/.local/share/applications/kdos-tui-<name>.desktop`. `rm` removes only an
entry this command created, never one the system shipped.

## Opening files

Double-clicking a file, or running `kdos-appbox open <path>`, finds the file's type from its name
and then a program to open it. Containerised applications are found exactly like host ones, because
installing one registers its file types in the same places.

```sh
kdos-appbox open report.pdf             # open with the default handler
kdos-appbox open --print report.pdf     # print which handler would open it, and do nothing
kdos-appbox open --choose report.pdf    # pick a handler
```

`kdos-openwith` picks a different handler for one file. A containerised application can be the
default handler for a type. `xdg-open` on KDOS is the same lookup, so links from any program go to
your chosen handler. Links clicked *inside* a box open on the host: a help link in a boxed
application opens in your browser rather than going nowhere.

Printing works from inside a box. The host's print service socket is shared into every box and the
application's own print dialog talks to it directly. Because a box's shared directories are fixed
when the box is created, a box created while the print service was not running cannot print.
Remove it with `kdos-box remove <name>`, and the next launch creates it again with the socket in
place.

## Commands that live in boxes

Not all containerised software is an application you click. Twenty-two catalogue applications carry
a program meant to be typed — `wine`, `gmic`, `ngspice`, `solve-field`, `cp2k`, `grib_ls`, `java`,
`glxgears` and the rest, thirty-three in all, listed in the catalogue as `cmd` rows. They are
solvers, converters, toolchains and tools driven from a prompt, and they get no menu entry: a
launcher for `wine` with no arguments would open nothing. To see them all, run
`grep '^cmd' /usr/share/kdos/appstore/catalogue`.

Run one in its application's box with `-b`:

```sh
kdos-appbox -b app.wine run wine setup.exe
kdos-appbox -b app.gromacs run gmx mdrun -h
```

`-b` names the box. Without it, `kdos-appbox run` works out the box from an installed application's
launcher, and a command-line tool has none.

Where a shim exists, typing the name is shorter. Shims are made for the programs an application's
own desktop entries name, plus `wine`, `winecfg` and `winetricks` wherever the box carries them.
Most command-only tools have no desktop entry and so no shim; use `kdos-appbox -b` for those. A
shim is never made under the name of a core host tool — `git`, `mpv`, `python3`, the shell tools
and KDOS's own commands among them — because it would take that name over from the host program.

## Updating

An application is never rebuilt behind your back, and installing one that is already here does
nothing: an image that already exists is reused as it is. To rebuild an application, remove it and
install it again:

```sh
kdos app remove app.krita
kdos app install app.krita
```

This rebuilds the application's own layer, and any runtime that nothing else was using. The Debian
base image is never removed and a runtime another installed application still uses is kept, so under
`snapshot = auto` the rebuild uses the same snapshot as before. Packages move to a newer snapshot
only when the catalogue names a later date, or when this machine pulls a newer
`debian:trixie-slim`.

An imported set behaves differently. Its packs carry versions, and `kdos-packd` keeps one
superseded version of each pack on disk when a newer one is imported, so an update can be undone.
That is `retain` in [`/etc/kdos/packd.conf`](../06-reference/configuration.md), which ships set to
`1`; `retain = 0` keeps none.

## Removing

```sh
kdos app remove app.krita     # one application
kdos app remove creative      # a whole group
```

Removing an application:

- removes its box — but keeps the box's settings, in `~/.config/kdos/boxes/<name>.conf`, and your
  work in it (the box's writable layer), in `~/.local/share/kdos/boxes/<name>`, so installing it
  again picks up where you left off;
- deletes the application's image, and any runtime image no other installed application uses;
- never deletes the base image, which every application sits on;
- regenerates the launchers, shims and file-type associations from what remains.

## Boxes

Every application gets its own box, named after it. You never have to think about boxes, but you
can manage them directly when you want to:

```sh
kdos-box list                                      # every box, running or not
kdos-box enter app.krita                           # a shell inside one
kdos-box create scratch base=image:kdos/alpine     # a new box; base= also takes pack:<id> and box:<name>
kdos-box clone app.krita krita-test                # a copy: the software and your work
kdos-box snapshot app.krita before-plugin          # also: snapshots, rollback <box> <tag>
kdos-box freeze scratch                            # what the box changed, captured as a pack
kdos-box start | stop | restart | remove <name>
kdos-box gc                                        # stop idle boxes now (--dry-run to preview)
```

`kdos-box create <name> base=box:<other>` starts from the other box's software with an empty
workspace; `kdos-box clone` copies the work as well.

A box's settings live in `~/.config/kdos/boxes/<name>.conf`; `kdos-box profile <name>` shows them
and `kdos-box profile <name> key=value` changes them. The Boxes page of `kdos-settings` edits the
same file. Every key maps onto something that is actually enforced, and the profile tells you where
something cannot be enforced rather than pretending. See
[kdos-appbox](../04-programs/kdos-appbox.md) for the keys.

Namespaces and shared directories are set when a box is created and cannot be changed on a running
container, so a profile change takes effect after `kdos-box remove <name>` and the next launch,
which creates the box again from the new profile.

`kdos-box freeze` packs only what the box has *written* over its base, which on a real working box
is a small fraction of its whole root. It is also the only way to capture the state of a box built
from packs, since there is no container image to commit.

Idle boxes are stopped for you. Your session runs `kdos-box gc` every ten minutes, which stops a box
that has sat idle past its profile's `autostop` time. It asks the compositor first: a box with a
window on the screen is never stopped, however idle its processes look.

## Accessibility inside a box

The desktop itself cannot be read by a screen reader, but a containerised application can use its
own toolkit's accessibility support. It is off by default; `touch ~/.config/kdos/a11y` turns it on
for every box, and `KDOS_A11Y=1` for one launch. See [Accessibility](accessibility.md).

## What does not work

- **Applications that need raw block devices** — partitioners, SMART tools, recovery tools. A
  rootless container cannot open a disk, so they are not in the catalogue. Those jobs are native
  tools on the host, where root is.
- **Applications that require a particular compositor's private protocols**, such as KDE's
  screenshot tool. Screenshots are the host's `kdos-shot`.
- **Microsoft core fonts for Wine.** They are downloaded at run time and nothing in the image may
  depend on that, so a Windows program asking for Arial gets a substitute.
- **OpenGL for X11 applications.** Xwayland is built without GLX. Wayland-native applications are
  unaffected.

## See also

- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the format and the machinery
- [kdos-appbox](../04-programs/kdos-appbox.md) — the launcher, the box manager, and profiles
- [The desktop](desktop.md) — the Start menu and the file manager
- [Theming](theming.md) — how containerised applications get the palette
- [The kdos command](../04-programs/kdos-command.md) — `kdos app` in full
