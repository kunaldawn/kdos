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

## What is on the medium

The catalogue is defined by `ports/appbox/packs.conf`:

| | |
|---|---|
| Applications | 181 |
| Shared runtimes | 7 |
| Base packs | 2 |
| Data packs | 2 |
| Boxed commands with no graphical launcher | 33 |

The runtimes are `rt-gtk`, `rt-qt`, `rt-kde`, `rt-media`, `rt-sci`, `rt-electron` and `rt-wine`.
An application pack is a difference over one of them, and each runtime is a difference over the
base — so a runtime's bytes are on the medium once no matter how many applications use it.

The catalogue covers the best free software per segment: office and documents, browsers and mail,
raster and vector graphics, photography, 3D and CAD, slicing and CNC, EDA and circuit simulation,
audio production, video editing and streaming, development environments, science and mathematics,
astronomy, GIS and mapping, amateur radio and SDR, emulators and games, KDE's application suite,
and Wine.

**The recommended set** — what an installer ticks by default — is Firefox, LibreOffice, GIMP,
Zathura and KeePassXC, plus the base and the runtimes those need.

If your medium has no catalogue, it was built without `make bootstrap-packs` or
`make fetch-packs`. See [Getting started](getting-started.md).

## Finding and installing

```sh
kdos app list              # what is installed here
kdos app list --all        # everything on the medium as well
kdos app search image      # match name, id and summary
kdos app show app.krita    # size, version, what it needs
kdos app install app.krita
```

`kdos app show` tells you what a pack **needs**. Some applications are useless without a dataset —
KiCad without its 3D models, Tesseract without language data — so those packs name a data pack in
`needs`, and this says whether it is here.

**From the desktop**, the Start menu is the discovery surface. Every category lists the
applications the medium would put there under an `ON THE MEDIUM` rule, search matches them, and
choosing one **installs the pack and opens the application in the same action**. The next time you
open the menu it is listed as installed.

On a live session, installing mounts the pack directly off the medium without copying it, so it
is close to instant. On an installed system it copies the pack into the store first.

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

Not all boxed software is an application. Thirty-three packs carry a **command** instead of, or
as well as, a launcher — `wine`, `gmic`, `ngspice`, `solve-field`, `cp2k`, `grib_ls` and the rest.
These are solvers and tools driven from a prompt, so they get a shim on your PATH and
deliberately no menu entry: a launcher for `wine` with no arguments opens nothing.

```sh
wine setup.exe
```

A pack with neither a launcher nor a command is a pack nothing on the host can reach, which is a
packaging bug rather than a feature.

## Updating and rolling back

```sh
kdos app sources           # where updates are looked for
kdos app update            # update everything with a newer version available
kdos app rollback app.krita
```

A **source** is a directory with a `PACKAGES` index in it — the medium the pack daemon reports,
plus anything listed in `/etc/kdos/pack-sources`, one path per line. On a distribution whose medium
*is* the software library, the interesting case is "the stick I just wrote is newer than the disk".

**A URL is never written in that file, and there is no line you can uncomment to make the machine
reach the network.** That is `kdos app update --online <url>`, an argument given each time, so it
is visible at the moment it is used — where a setting is invisible until the day it surprises
somebody. Nothing KDOS does on its own leaves the machine.

Where the old pack is still on disk, the update takes a **delta** and reconstructs the new pack
locally; where it is not, it takes the whole pack.

**Nothing in that path has to be trusted.** The reconstruction lands in a staging directory and is
verified where it is mounted — hashed against the pack's own footer and checked against the
signing key — so a tampered delta produces a pack that fails there and cannot make the machine
install anything the index did not already name.

`retain` in `/etc/kdos/packd.conf` decides how many previous versions are kept; the default is 1,
which is what makes `rollback` possible. `retain = 0` is an honest off, and rollback then says no
earlier version is kept rather than failing. Old versions are swept after an install and at no
other time — a sweep on a timer would be a background job deleting your rollback while you were
deciding whether to use it.

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
