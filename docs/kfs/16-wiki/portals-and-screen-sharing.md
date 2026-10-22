# Portals and screen sharing

A [portal]{.idx} is the [D-Bus]{.idx} service an application asks when it needs something done on the
host: a file chosen, the screen captured, a link opened, the theme read, the camera or the location
handed over. This page covers which programs answer each portal on KDOS, the files that choose
them, sharing and recording the screen, the stored answers to the grant-or-deny questions, and what
to check when an application says there is nothing to capture. How the front end, the backends and
their start-up order fit together is in [Portals](../../kdos/03-architecture/session.md#portals).

## Installation

| Port | Provides |
|:--|:--|
| [xdg-desktop-portal](#port-xdg-desktop-portal) | The front end, `/usr/lib/xdg-desktop-portal`, which owns the public interfaces, its permission store, and the icon and sound validators |
| [xdg-desktop-portal-wlr](#port-xdg-desktop-portal-wlr) | The ScreenCast and Screenshot backend, `/usr/lib/xdg-desktop-portal-wlr` |
| [xdg-desktop-portal-kdos](#port-xdg-desktop-portal-kdos) | The FileChooser, Settings, AppChooser and Access backend, [`/usr/lib/xdg-desktop-portal-kdos`]{.idx} |
| [xdg-desktop-portal-gtk](#port-xdg-desktop-portal-gtk) | The Print and Email backend |
| [pipewire](#port-pipewire), [wireplumber](#port-wireplumber) | The media graph a screen capture is delivered through |
| [fuzzel](#port-fuzzel) | The list the capture backend shows to ask which screen or window to share |
| [kdos-shell](#port-kdos-shell) | [`kdos-pick`]{.idx}, the file chooser, and [`kdos-prompt`]{.idx}, the grant-or-deny dialog, which the KDOS backend runs |
| [kdos-record](#port-kdos-record) | [`kdos-record`]{.idx}, the desktop's screen recorder, a ScreenCast client |
| [geoclue](#port-geoclue) | The location service behind the Location portal |
| [kdos-tools](#port-kdos-tools) | [`kdos-shot`]{.idx}, the [screenshot]{.idx} tool, which captures through the compositor directly with `grim` rather than through a portal |

Every one is named in a phase list, so all are on the image. None is a system service. The front end and the capture backend are
started by `kdos-desktop-start` once the compositor's socket exists, in that order; the KDOS and
GTK backends are started by D-Bus when first asked, which the front end does as it starts, so both
are running within seconds of a login; GeoClue is started on the system bus by D-Bus.
A login on `tty2` or over SSH has no portals of its own, because no compositor is running for it.

## Configuration

### /usr/share/xdg-desktop-portal/kdos-portals.conf

The front end reads `<desktop>-portals.conf` from the lowercased first name in
`XDG_CURRENT_DESKTOP`, which the session sets to `KDOS`. The file is in the base filesystem, at
`fs/usr/share/xdg-desktop-portal/kdos-portals.conf`:

<!-- kfs:include fs/usr/share/xdg-desktop-portal/kdos-portals.conf -->

Each interface names exactly one backend. Why a list such as `wlr;kdos` does not fall through is in
[Portals](../../kdos/03-architecture/session.md#portals).

### /usr/share/xdg-desktop-portal/portals/kdos.portal

The KDOS backend's own declaration, from `src/desktop/xdg-desktop-portal-kdos/kdos.portal`:

<!-- kfs:include src/desktop/xdg-desktop-portal-kdos/kdos.portal -->

### ~/.config/xdg-desktop-portal-wlr/config

The capture backend's settings are per account, seeded from
`fs/etc/skel/.config/xdg-desktop-portal-wlr/config`:

<!-- kfs:include fs/etc/skel/.config/xdg-desktop-portal-wlr/config -->

Every key is described in `xdg-desktop-portal-wlr(5)`, which the port installs.

### kdos-desktop-start

The start-up order is set in `fs/usr/local/bin/kdos-desktop-start`, installed as
`/usr/local/bin/kdos-desktop-start`:

<!-- kfs:include fs/usr/local/bin/kdos-desktop-start lines=46-74 pin=0bd98f27d533 -->

### The capture keys

The screenshot and recording keys, from `fs/etc/skel/.config/kdos-comp/rc.xml`:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=269-274 pin=7e1418ed503a -->

### /etc/geoclue/conf.d/90-kdos.conf

The [geoclue](#port-geoclue) port's `build.sh` writes it:

<!-- kfs:include ports/core/gis/geoclue/build.sh lines=40-48 pin=bfca3189730d -->

An empty whitelist, with the port's `no-agent.patch`, makes GeoClue answer every client at once;
see [Access, and what it unlocks](../../kdos/03-architecture/session.md#access-and-what-it-unlocks).

### ~/.local/share/flatpak/db/

The front end keeps the answers to its grant-or-deny questions in its permission store,
`xdg-permission-store`, which writes one file per table here. The camera's answers are the entry
`camera` in the table `devices`, the screenshot answers the entry `screenshot` in `screenshot`, and
the location answers the entry `location` in `location`. Nothing on KDOS shows or edits them.

The permission store reads a table's file once, the first time the table is asked for, and keeps
it in memory. Deleting a file while the store runs changes nothing it answers, and its next write
puts the old entries back.

### ~/.config/kdos/boxes/&lt;name&gt;.conf

A box is kept off the compositor's capture protocols, so a boxed application captures the screen
only through the ScreenCast portal. A `grant =` line in its profile can open them to it; the names
and what each one costs are in
[Granting a box more than the allowlist](../../kdos/03-architecture/session.md#granting-a-box-more-than-the-allowlist).

## Usage

### Sharing the screen with an application

1. In the application, start the [screen share]{.idx term="screen sharing"} or add its screen-capture source. In OBS that is a
   **Screen Capture ([PipeWire]{.idx})** or **Window Capture (PipeWire)** source.
2. A list opens with the prompt `Share:`. It names each screen as
   `Monitor: <name> <description>` and, when the application asked for windows as well, each
   window as `Window: <title> (<id>)`.
3. Type to narrow the list, move to the screen or window, and press Enter. Esc shares nothing.
4. While the share runs, the panel shows a `SCR` lamp with the application's name. Stop the share
   from the application.

The frames reach the application through PipeWire at no more than `max_fps`. A native application
and a boxed one take the same route. The `Share:` list, its Esc and its Enter have been carried
out on the live medium through `kdos-record`, below; no share has been made from a browser or a
video-call program on a KDOS image (see Known gaps), and the `SCR` lamp has not been seen.

### Recording the screen to a file

1. Press Alt+Print, or run:

   ```bash
   kdos-record
   ```

   `kdos-record ~/clip.mkv` names the file instead of the default
   `~/Videos/YYYY-MM-DD-HHMMSS.mkv`.

2. Pick the screen from the `Share:` list. `kdos-record` prints
   `kdos-record: recording node <n> to <file> — run kdos-record again to stop`. Esc instead
   prints `kdos-record: nothing was shared (1)` and records nothing.
3. Press Alt+Print again, or run `kdos-record` again, to stop. The second run prints
   `kdos-record: stopping the recording` and signals the first, which ends the stream so the file
   is closed properly.

The recording is video only, H.264 when GStreamer's `x264enc` is installed, as it is on the image,
and VP8 otherwise. On the live medium in a virtual machine, with the compositor on its software
renderer, a recording stopped cleanly and left a 408-byte file with no video in it, both of a
still screen and of a terminal window redrawing twenty times a second. The recorder's PipeWire
node stayed `idle` and no `SCR` lamp showed while it ran: the stream was set up and no frame
arrived.
Whether frames arrive on a GPU renderer has not been tried
([Status](../../kdos/06-reference/status.md#libraries-tooling-and-documentation)).

### Taking a screenshot

| Keys | Takes |
|:--|:--|
| Print | The whole screen |
| Shift+Print, Super+Shift+P | A rectangle dragged with the pointer |

`kdos-shot` saves under `~/Pictures/Screenshots/`, as `kdos-YYYYMMDD-HHMMSS.png`, prints the
path, and copies the picture to the clipboard as `image/png`. It does not go through a portal. An application that asks the Screenshot portal is answered by
`xdg-desktop-portal-wlr`; when it asks for a screenshot without letting you choose
(a non-interactive request), the front end first puts the Access question below.

### Answering an Access question

The first time an application asks for the camera, or for a non-interactive screenshot, a
`kdos-prompt` dialog asks whether to grant it. Deny is selected when it opens, and it is also the
answer when nobody answers. The answer is stored and the question is not asked again.

Every box looks like the same host program to the front end, so one answer covers every box. A
boxed application's location is given without a question; see Known gaps.

### Asking again after an answer

Ask the permission store to delete the entry, here the camera's, from a terminal inside the
session:

```bash
gdbus call --session --dest org.freedesktop.impl.portal.PermissionStore \
  --object-path /org/freedesktop/impl/portal/PermissionStore \
  --method org.freedesktop.impl.portal.PermissionStore.Delete devices camera
```

It prints `()`; with no answer stored it fails with
`GDBus.Error:org.freedesktop.portal.Error.NotFound: No entry for camera`, which changes nothing.
`...PermissionStore.Lookup devices camera`, with the same first three lines, shows the stored
answers. Use `screenshot screenshot` for the screenshot answers. The next request asks again. Deleting an
entry forgets the answer for every application, and so for every box. Removing the file under
`~/.local/share/flatpak/db/` works only while no `xdg-permission-store` is running, for the reason
given under Configuration.

### Seeing which backend answers

```bash
grep -v '^#' /usr/share/xdg-desktop-portal/kdos-portals.conf
ls /usr/share/xdg-desktop-portal/portals/
gdbus call --session --dest org.freedesktop.portal.Desktop \
  --object-path /org/freedesktop/portal/desktop \
  --method org.freedesktop.DBus.Properties.Get org.freedesktop.portal.ScreenCast AvailableSourceTypes
```

The directory holds `gtk.portal`, `kdos.portal` and `wlr.portal`. The last command answers
`(<uint32 3>,)` when ScreenCast has a backend: screens (1) and windows (2).

## Tips

- `kdos doctor` checks the capture backend and the configuration file in its Session and Desktop
  sections: `wlr portal running` and `screen-capture portal installed and selected for KDOS`.
- A screenshot of a whole screen includes the phosphor pass; a capture of one window does not.
- `max_fps=30` bounds what a recording of a still screen costs. Raise it in your copy of the
  backend's config for smoother capture of fast motion.
- `kdos explain portal` prints every recorded reason that mentions the portals.

## Troubleshooting

**OBS lists no PipeWire screen or window capture source, or another application finds nothing to
share.**
The front end has no ScreenCast backend. It reads its backends once, when it starts, so a capture
backend that was not on the bus at that moment is missing for the whole session. Check, from a
terminal inside the session:

```bash
pgrep -af xdg-desktop-portal
dbus-send --session --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus \
  org.freedesktop.DBus.NameHasOwner string:org.freedesktop.impl.portal.desktop.wlr
```

`-f` is needed: `pgrep` matches a bare name only against the first 15 characters of a process
name, and refuses a longer pattern. A working session lists `/usr/lib/xdg-desktop-portal`,
`-wlr`, `-kdos` and `-gtk`, and the second command ends `boolean true`.

If the backend is missing, or both run and capture still fails, restart the two in the session's
order:

```bash
/usr/lib/xdg-desktop-portal-wlr -r &
sleep 1
/usr/lib/xdg-desktop-portal -r &
```

`-r` replaces a running instance. Logging out and in again does the same.

**`kdos doctor` warns `kdos-portals.conf missing`.**
Without the file ScreenCast has no backend. It belongs to the base filesystem rather than to a
package, so no `kpkg` command puts it back: recreate it as root with the contents shown under
Configuration, then restart the front end as above.

**The `Share:` list never appears.**
Run the capture backend by hand, replacing the running one, with logging, and make the request
again to see it arrive:

```bash
/usr/lib/xdg-desktop-portal-wlr -r -l DEBUG
```

**A recording cannot be seeked, or has no length.**
The pipeline was killed rather than stopped, so the muxer never wrote the file's index. Stop a
recording with Alt+Print or a second `kdos-record`, which turns the stop into an end of stream;
never kill `gst-launch-1.0` itself.

**An application's Open dialog is its own toolkit's, not `kdos-pick`.**
Either the application does not ask the portal, or the KDOS backend did not answer. D-Bus starts
the backend from
`/usr/share/dbus-1/services/org.freedesktop.impl.portal.desktop.kdos.service`; check that the file
and `/usr/lib/xdg-desktop-portal-kdos` exist, and that it owns its name once a dialog has been
asked for:

```bash
dbus-send --session --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus \
  org.freedesktop.DBus.NameHasOwner string:org.freedesktop.impl.portal.desktop.kdos
```

A native GTK application uses the portal because the login shell exports `GTK_USE_PORTAL=1`; one
started from a shell that did not read `/etc/profile.d/10-wayland.sh` draws its own dialog.

**Front-end messages.** Run the front end in the foreground with its own logging:

```bash
/usr/lib/xdg-desktop-portal -r -v
```

## Known gaps

- [A boxed application is given your location without being asked](../../kdos/06-reference/known-gaps.md#a-boxed-application-is-given-your-location-without-being-asked)
- [A video call has never been placed](../../kdos/06-reference/known-gaps.md#a-video-call-has-never-been-placed)
- [A box can be granted only the eight named groups of Wayland globals](../../kdos/06-reference/known-gaps.md#a-box-can-be-granted-only-the-eight-named-groups-of-wayland-globals)
- [An Android screen cannot be mirrored](../../kdos/06-reference/known-gaps.md#an-android-screen-cannot-be-mirrored)
- [The native applications have not been run](../../kdos/06-reference/known-gaps.md#the-native-applications-have-not-been-run)

## See also

- [Portals](../../kdos/03-architecture/session.md#portals): the front end, the backends and the start-up order
- [Recording](../../kdos/03-architecture/session.md#recording): how `kdos-record` drives the portal
- [Screen capture and the clipboard](../../kdos/03-architecture/session.md#screen-capture-and-the-clipboard):
  why a box must use the portal
- [xdg-desktop-portal-kdos](../../kdos/04-programs/daemons.md#xdg-desktop-portal-kdos): the KDOS backend
- [kdos-shot](../../kdos/04-programs/kdos-command.md#kdos-shot): screenshots
- [Fonts and theming](fonts-and-theming.md), [Diagnostics](diagnostics.md),
  [Desktop troubleshooting](desktop-troubleshooting.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 418.* Previous: [417. Accessibility](accessibility.md) · [Contents](../README.md) · Next: [419. Diagnostics](diagnostics.md)
