# Applications

This page covers getting graphical software onto a KDOS machine, launching it, moving a set of
applications to another machine, and removing them. KDOS builds the desktop; graphical
applications come from a catalogue of containerised packages shipped on the medium.
[Packs and boxes](../03-architecture/packs-and-boxes.md) describes the format itself.

## What an alien app is

An alien app is a graphical application that is not compiled by this repository. It arrives by one
of two routes and runs in a *box* either way — a rootless podman container of its own, named after
the application.

Installed from the catalogue, it is a stack of podman images this machine builds: a base, a shared
runtime, the application on top, and a box over that top image. Installed from an exported set or
off the medium, it is a *pack* — one read-only filesystem image whose hash and signature are
checked before anything mounts it — composed into the box's root with the runtime and base packs
under it.

From your side it behaves like ordinary system software. It has a launcher in the Start menu, it
handles the file types it claims, it can be a default handler, and it has a command you can type
at a prompt. The container is an implementation detail everywhere except the first launch, which
is slower.

That boundary is deliberate and is where the build cost stops. No browser, office suite or CAD
package is native-ported, and none is planned. See [Decisions](../01-philosophy/decisions.md).

## What the catalogue carries

The catalogue is `/usr/share/kdos/appstore/catalogue`, shipped on the medium. It describes
software rather than carrying it: nothing is baked into the ISO, and an application is built by
Podman on the machine that asks for it.

| | |
|---|---|
| Applications | 180 |
| Shared runtimes | 7 |
| Base rows | 2 |
| Data rows | 2 |
| Groups | 7 |
| `cmd` rows — programs meant to be typed | 33, across 22 packs |

The runtimes are `rt-gtk`, `rt-qt`, `rt-kde`, `rt-media`, `rt-sci`, `rt-electron` and `rt-wine`.
An application is built `FROM` one of them and each runtime `FROM` a base, so a runtime's layers
are stored once no matter how many applications use it. The two base rows are `base` — Debian plus
what every box pays for, which is the set a graphical application needs and no more — and `alpine`,
about 3 MB of busybox and musl, carried because a clean scratch userland to try something in is
worth one line and this one needs no network to reach.

A row's parent must appear *above* it in the file: the resolver walks a chain upward in one pass,
so a forward reference is a chain it cannot close and the install fails naming the missing parent.

Between them the rows cover the best free software per segment: office and documents, browsers and
mail, raster and vector graphics, photography, 3D and CAD, slicing and CNC, EDA and circuit
simulation, audio production, video editing and streaming, development environments, science and
mathematics, astronomy, GIS and mapping, amateur radio and SDR, emulators and games, KDE's
application suite, and Wine.

Groups are how the store and the installer offer the catalogue: `essential`, `office`, `creative`,
`dev`, `science`, `make` and `games`. The installer ticks `essential` by default — Firefox,
LibreOffice, GIMP, Zathura and KeePassXC — and the base and runtimes those need arrive through the
parent chain rather than by being listed.

## Finding and installing

```sh
kdos app list              # what is installed here
kdos app list --all        # everything in the catalogue as well
kdos app groups            # the seven groups and what is in each
kdos app search image      # match name, id and summary
kdos app info app.krita    # category, state, the row it is built on, a size estimate
kdos app install app.krita # one application
kdos app install creative  # a whole group
kdos app install app.krita --dry-run   # what would be built, without building it
```

Some applications are useless without a dataset, and the dataset is a row of its own. A `needs` row
in the catalogue ties an application to one, and installing the application installs it too: KiCad
pulls `data.kicad-packages3d` that way, which is why 5 GB of 3D models is not inside `app.kicad`.
`data.tesseract-langs` is the other data row and belongs to no application — it grafts its language
files over the *host's* `tesseract`, and you install it by name.

Installing builds the application here. Each row is an image `FROM` its parent row, so installing
Krita builds the base, the Qt runtime and Krita itself, in that order. The next Qt application
reuses that runtime and is one apt pass rather than three. That is also why the size the catalogue
shows is an estimate: what apt resolves on the day depends on the snapshot, and shared layers are
counted once on disk however many applications name them.

Two limits are worth knowing before you start.

Installing needs a network and takes minutes. An exported set installs offline instead — see
[Carrying a set to another machine](#carrying-a-set-to-another-machine).

Installing does not work on a live session. `$HOME` sits on overlayfs there, so a box's overlay
upper layer has nowhere to go and `kdos-box create` refuses. Install to disk first, or import a
set.

## Carrying a set to another machine

```sh
kdos app export ~/apps.ktar creative app.vscodium
kdos app import ~/apps.ktar          # on the other machine
```

Export flattens each installed application into a pack and puts the set in one file with an index.
The index is what carries the signature: it is signed once and holds every pack's hash, so a pack
matching it is covered. Import hands each pack to the pack daemon, which verifies it — against that
index where the archive has a signed one, against the pack's own signature block otherwise — before
anything is installed. So an imported application is verified where a store-installed one is not,
and no network is needed at any point.

This is also what the installer reads off a stick when there is no network during an install, and
it is the only way to get software onto a live session.

A pack the daemon refuses is named and skipped; the rest of the archive still imports. With no
signing key (`KDOS_PACK_KEY`) the set still exports and still imports, every hash is still checked,
and the export says plainly that the index was not signed.

The `SELECTION` file inside the archive is flat text, one id per line, so a set can be read, diffed
and edited by hand.

## Launching

Three routes, all equivalent:

```sh
krita                      # the shim on your PATH
kdos app launch app.krita  # by id
```

or a row in the Start menu.

![The launcher: full-screen search over the same application index the Start menu uses](../../screenshots/launcher.png)

`kdos app launch` installs the application first if it is not installed, so the click that installs
is the click that opens.

A first launch is slow, because it has to create and start a container. Measured on the reference
machine: **18.3 seconds** cold with no container at all, **0.3 seconds** warm, and **0.55 seconds**
for a second window. That is why the Start menu marks containerised applications `[box]`, and why
the applications you have pinned are started in the background when you log in — the warm-up reads
`~/.config/kdos/favorites`, which is the set you chose, and starts the first eight of them one at a
time under `nice`.

Stage timings for one launch are appended to `$XDG_RUNTIME_DIR/kdos-appbox.trace` if you want to
see where the time went.

## Opening files

Double-clicking a file, or running `kdos-appbox open <path>`, resolves the file's type from its
name and then finds a handler. Containerised applications are found by exactly the same lookup as
host ones, because launcher generation writes their MIME associations into the same places.

```sh
kdos-appbox open --print report.pdf     # resolve and print the handler, do not run it
kdos-appbox open --choose report.pdf    # pick a handler
```

`kdos-openwith` picks a different handler for one file. A containerised application can be a
default handler for a type, and links clicked inside a box open on the host — a box gets its own
`xdg-open` that hands the request to the host through the portal, so a help link in a boxed
application opens in your browser rather than going nowhere.

Printing works from inside a box. The host's print socket is shared into every box and the
application's own print dialog talks to it directly; there is no portal print backend and none is
needed. Because a shared directory is fixed when the container is created, a box created before the
print service was running does not have it. Remove that box with `kdos-box remove <name>` and the
next launch builds it again with the socket in place.

## Commands that live in boxes

Not all containerised software is an application. Twenty-two packs carry a program meant to be
typed rather than clicked — `wine`, `gmic`, `ngspice`, `solve-field`, `cp2k`, `grib_ls`,
`glxgears` and the rest, thirty-three in all, written down as the catalogue's `cmd` rows. These are
solvers, benchmarks and tools driven from a prompt, and they deliberately get no menu entry: a
launcher for `wine` with no arguments opens nothing.

The box manager runs one by name, in the pack's own box:

```sh
kdos-appbox -b app.wine run wine setup.exe
kdos-appbox -b app.gromacs run gmx mdrun -h
```

`-b` is what makes that work: without it, `kdos-appbox run` resolves the box from the exec line of
an installed application, and a solver nothing launches has no such line.

A shim on your PATH is the shorter route where there is one, and a shim is written from what a
pack's own desktop entries name — plus `wine`, `winecfg` and `winetricks`, which launcher
generation emits wherever the image carries the binary. A pack that ships no entry naming its
program, which is most of the command-only ones, is reached through `kdos-appbox -b` as above. A
shim that took its name from a program the host also carries would shadow the host's copy, which is
why a pack whose binaries duplicate a host port's names is left without one.

A pack with neither a launcher nor a shim nor a `cmd` row is a pack nothing on the host can name,
which is a packaging bug rather than a feature.

## Updating

```sh
kdos app install app.krita     # rebuilds it against the current snapshot
```

There is no separate update verb. An application is a stack of images this machine built;
rebuilding it is what an update is, and `install` over an existing one does exactly that. What it
is built against is the catalogue's `snapshot` key, so two machines on the same commit build the
same thing.

An imported set behaves differently. Its packs carry versions, and `kdos-packd` keeps one
superseded version of each on disk rather than deleting it as the new one lands. That is `retain`
in [`packd.conf`](../06-reference/configuration.md), and it ships set to 1; `retain = 0` is an
honest off.

## Removing

```sh
kdos app remove app.krita
```

The launcher, the shim and the MIME associations go with it. A pack that was mounted from the
medium is unmounted rather than deleted, since it was never copied.

## Boxes

Every application gets its own box, named after its pack. You do not have to think about them, but
they are first-class objects when you want them:

```sh
kdos-box list                          # every box, running or not
kdos-box enter app.krita               # a shell inside one
kdos-box create scratch base=image:kdos/alpine     # also pack:<id> and box:<name>
kdos-box clone app.krita krita-test    # the software and the work
kdos-box snapshot app.krita before-plugin
kdos-box freeze scratch                # capture what you changed, as a pack
kdos-box gc                            # stop the idle ones now
```

A box's settings live in `~/.config/kdos/boxes/<name>.conf`, and the Boxes page of `kdos-settings`
edits them without your needing to know that. Every key maps onto something that is actually
enforced, and the profile says out loud where it cannot enforce something rather than pretending.

Namespaces and volumes are applied when a box is created and cannot be re-flagged on a live
container, so a profile change takes effect after `kdos-box remove <name>` and the next launch,
which builds the box again from the new profile.

`kdos-box freeze` is worth knowing about: it packs only what the box has *written* over its base,
which on a real working box is a very small fraction of the merged root. It is also the only way
to capture a pack box's state, since there is no image to commit.

Idle boxes are stopped automatically. The session runs `kdos-box gc` every ten minutes, and gc asks
the compositor first — a box with a window on the screen is never stopped, however idle its
processes look.

## What does not work

Stated plainly so you stop looking:

- Applications that need raw block devices — partitioners, SMART tools, recovery tools — are
  deliberately not in the catalogue and get no launcher, because a rootless container cannot do
  anything useful with them. Those jobs are native tools on the host, which is where root is.
- Applications that require a specific compositor's private protocols, such as KDE's screenshot
  tool. Screenshots are the host's `kdos-shot`.
- Microsoft core fonts for Wine. They are fetched from the network at run time and nothing in the
  image may depend on that, so a Windows program wanting Arial gets a substitute.
- OpenGL for X11 clients. Xwayland is built without GLX. Wayland-native applications are
  unaffected.

## See also

- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the format and the machinery
- [kdos-appbox](../04-programs/kdos-appbox.md) — the launcher, the box manager, and profiles
- [The desktop](desktop.md) — the Start menu and the file manager
- [Theming](theming.md) — how containerised applications get the palette
- [The kdos command](../04-programs/kdos-command.md) — `kdos app` in full
