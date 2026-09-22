# Applications

Getting graphical software onto a KDOS machine, launching it, and getting rid of it. KDOS builds
the desktop; applications come from a catalogue of containerised packages on the medium. This
page covers using them. For how the format works, see
[Packs and boxes](../03-architecture/packs-and-boxes.md).

## What an alien app is

An **alien app** is a graphical application that is not compiled by this repository. It ships as
a **pack** — one signed filesystem image — and runs inside a **box**, a rootless container built
from that pack stacked over a shared runtime and a base.

From your side it behaves like ordinary system software. It has a launcher in the Start menu, it
handles the file types it claims, it can be a default handler, and it has a command you can type
at a prompt. The container is an implementation detail everywhere except the first launch, which
is slower.

The boundary is deliberate and is where the build cost is: no browser, office suite or CAD
package is native-ported, and none ever will be. See
[Decisions](../01-philosophy/decisions.md).

## What the catalogue carries

The catalogue is `/usr/share/kdos/appstore/catalogue`, shipped on the medium.
It describes software rather than carrying it: nothing is baked into the ISO,
and an application is built by podman on the machine that asks for it.

| | |
|---|---|
| Applications | 183 |
| Shared runtimes | 7 |
| Base rows | 2 |
| Data rows | 2 |
| Groups | 7 |
| Boxed commands with no graphical launcher | 36, across 25 rows |

The runtimes are `rt-gtk`, `rt-qt`, `rt-kde`, `rt-media`, `rt-sci`, `rt-electron` and `rt-wine`.
An application is built `FROM` one of them and each runtime `FROM` the base, so a runtime's layers
are stored once no matter how many applications use it.

The catalogue covers the best free software per segment: office and documents, browsers and mail,
raster and vector graphics, photography, 3D and CAD, slicing and CNC, EDA and circuit simulation,
audio production, video editing and streaming, development environments, science and mathematics,
astronomy, GIS and mapping, amateur radio and SDR, emulators and games, KDE's application suite,
and Wine.

**Groups** are how the store and the installer offer it: `essential`, `office`,
`creative`, `dev`, `science`, `make` and `games`. `essential` is what an
installer ticks by default — Firefox, LibreOffice, GIMP, Zathura and KeePassXC
— and the base and runtimes those need arrive through the parent chain rather
than by being listed.

## Finding and installing

```sh
kdos app list              # what is installed here
kdos app list --all        # everything in the catalogue as well
kdos app search image      # match name, id and summary
kdos app info app.krita    # size estimate, its chain, what it needs
kdos app install app.krita # one application
kdos app install creative  # a whole group
```

`kdos app info` tells you what an application **needs**. Some are useless without a dataset —
KiCad without its 3D models, Tesseract without language data — so those rows name a data row in
`needs`, and it is installed with them.

**Installing builds it here.** A row is an image `FROM` the row below it, so installing Krita
builds the base, the Qt runtime and Krita itself, in that order. The next Qt application reuses
that runtime and is one apt pass rather than three — which is also why the size the catalogue
shows is an **estimate**: what apt resolves on the day depends on the snapshot, and the shared
layers are counted once on disk however many applications name them.

**It needs a network and it takes minutes**, where a pack was a mount. An exported set installs
offline instead — see [Carrying a set to another machine](#carrying-a-set-to-another-machine).

**It does not work on a live session.** `$HOME` is on overlayfs there, so a box's overlay upper
has nowhere to go and `kdos-box create` refuses. Install to disk first, or import a set.

## Carrying a set to another machine

```sh
kdos app export ~/apps.ktar creative app.vscodium
kdos app import ~/apps.ktar          # on the other machine
```

Export flattens each installed application into a signed pack and puts the set
in one file with an index. Import stages each pack through the pack daemon,
which **hashes it and checks its signature where it mounts it** — so an imported
application is verified where a store-installed one is not, and no network is
needed at any point.

It is also what the installer reads off a stick when there is no network during
an install, and the only way to get software onto a live session.

A pack the daemon refuses is named and skipped; the rest of the archive still
imports. With no signing key the set still exports and still imports, every hash
is still checked, and the export says plainly that it was not signed.

The `SELECTION` file inside is flat text — one id per line — so a set can be
read, diffed and edited by hand.

## Launching

Three ways, all equivalent:

```sh
krita                 # the shim on your PATH
kdos app launch app.krita
```

or the Start menu.

![The launcher: full-screen search over the same application index the Start menu uses](../../screenshots/launcher.png)


`kdos app launch` installs the pack first if it is not installed, so the click that installs is
the click that opens.

**A first launch is slow.** It has to create and start a container. Measured on the reference
machine: about **18 seconds cold**, **0.3 seconds warm**, and roughly half a second for a second
window. That is why the Start menu marks boxed applications `[box]`, and why the applications you
have pinned are warmed in the background when you log in.

Stage timings for a launch are appended to `$XDG_RUNTIME_DIR/kdos-appbox.trace` if you want to see
where the time went.

## Opening files

Double-clicking a file, or `kdos-appbox open <path>`, resolves the file's type from its name and
then finds a handler. Boxed applications are found by exactly the same lookup as host ones,
because the launcher generation writes their MIME associations into the same places.

```sh
kdos-appbox open --print report.pdf    # resolve and print, do not run
```

`kdos-openwith` picks a different handler for one file. A boxed application can be a **default**
handler for a type, and links clicked inside a box open on the host — a box gets its own
`xdg-open` that hands the request to the host through the portal, so a help link in a boxed
application opens in your browser rather than going nowhere.

**Printing works from inside a box.** The host's print socket is shared into every box and the
application's own print dialog talks to it directly; there is no portal print backend and none is
needed. Because a shared directory is fixed when the container is created, a box created before
the print service was running does not have it — `kdos-appbox recreate <box>` fixes that.

## Commands that live in boxes

Not all boxed software is an application. Twenty-five packs declare a **command** instead of, or as
well as, a launcher — `wine`, `gmic`, `ngspice`, `solve-field`, `cp2k`, `grib_ls`, `glxgears` and
the rest. These are solvers, benchmarks and tools driven from a prompt, and they deliberately get
no menu entry: a launcher for `wine` with no arguments opens nothing.

`kdos app show <pack>` prints the commands a pack declares, and the box manager runs one by name:

```sh
kdos app show app.wine
kdos-appbox -b app.wine run wine setup.exe
```

**A shim on your PATH is the shorter route, and it is written from what a pack's own desktop
entries name.** A pack that carries no entry at all — which is most of the command-only ones — is
reached through `kdos-appbox -b` as above. A shim that took its name from a program the host also
carries would shadow the host's copy, which is why a pack whose binaries duplicate a host port's
names declares no command.

A pack with neither a launcher nor a declared command is a pack nothing on the host can reach,
which is a packaging bug rather than a feature.

## Updating

```sh
kdos app install app.krita     # rebuilds it against the current snapshot
```

**There is no separate update verb.** An application is a stack of images this
machine built; rebuilding it is what an update is, and `install` over an
existing one does exactly that. What it is built against is the catalogue's
`snapshot`, so two machines on the same commit build the same thing.

**An imported set is different**: its packs carry versions, and `kdos-packd`
keeps one superseded version so `kdos app` can put a replaced pack back. That
is `retain` in [`packd.conf`](../06-reference/configuration.md).

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
kdos-box create scratch base=pack:alpine
kdos-box freeze scratch                # capture what you changed, as a pack
```

A box's settings live in `~/.config/kdos/boxes/<name>.conf`, and the Boxes page of `kdos-settings`
edits them without your needing to know that. Every key maps onto something that is actually
enforced — and the profile says out loud where it cannot enforce something, rather than pretending.

`kdos-box freeze` is worth knowing about: it packs only what the box has *written* over its base,
which on a real working box is a very small fraction of the merged root. It is also the only way
to capture a pack box's state, since there is no image to commit.

Idle boxes are stopped automatically every ten minutes, but never one that still has a window on
the screen.

## What does not work

Stated plainly so you stop looking:

- **Applications that need raw block devices** — partitioners, SMART tools, recovery tools — are
  deliberately not in the catalogue and get no launcher, because a rootless container cannot do
  anything useful with them. Those jobs are native tools on the host, which is where root is.
- **Applications that require a specific compositor's private protocols**, such as KDE's
  screenshot tool. Screenshots are the host's `kdos-shot`.
- **Microsoft core fonts for Wine.** They are fetched from the network at run time and nothing in
  the image may depend on that, so a Windows program wanting Arial gets a substitute.
- **X11 clients get no OpenGL.** Xwayland is built without GLX. Wayland-native applications are
  unaffected.

## See also

- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the format and the machinery
- [kdos-appbox](../04-programs/kdos-appbox.md) — the launcher, the box manager, and profiles
- [The desktop](desktop.md) — the Start menu and the file manager
- [Theming](theming.md) — how boxed applications get the palette
- [The kdos command](../04-programs/kdos-command.md) — `kdos app` in full
