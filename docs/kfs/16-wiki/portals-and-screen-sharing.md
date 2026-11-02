# Portals and screen sharing

A [portal]{.idx} is the desktop's answer to a request an application cannot be trusted to carry
out itself: open a file it was not given, show the screen, take a screenshot, read the colour
scheme, open a link. On KDOS the portal front end, `xdg-desktop-portal`, passes every request to
the KDE backend, which asks the person in a KDE dialog and has KWin capture the screen into
PipeWire. This page covers the portal configuration, screenshots and recordings, sharing the
screen with another application, sharing the whole desktop over VNC or RDP, and what a boxed
application reaches through the portals. How the portals fit into the session is in
[Portals](../../kdos/03-architecture/session.md#portals).

## Installation

| Port | Provides |
|---|---|
| [xdg-desktop-portal](#port-xdg-desktop-portal) | The front end, `org.freedesktop.portal.Desktop`, and the permission store |
| [xdg-desktop-portal-kde](#port-xdg-desktop-portal-kde) | The backend: the file chooser, screen cast, screenshot, remote desktop, settings, app chooser, notification, print, inhibit, global shortcuts and access dialogs |
| [kwin](#port-kwin) | The screen and window capture the screen cast and screenshot draw on |
| [kpipewire](#port-kpipewire) | Puts KWin's capture into PipeWire streams |
| [spectacle](#port-spectacle) | Screenshots and screen recordings |
| [krfb](#port-krfb), [krdp](#port-krdp) | Sharing the whole desktop over VNC and RDP |
| [plasmoid-kdos-privacy](#port-plasmoid-kdos-privacy) | The privacy lamps, which show and stop every capture |

Neither portal is a service. The session bus starts both on the first request.

## Configuration

### Which backend answers

The front end reads `kde-portals.conf`, which [plasma-workspace](#port-plasma-workspace)
installs, because the session's `XDG_CURRENT_DESKTOP` is `KDE`; it names the KDE backend for every
interface. A file of your own in `~/.config/xdg-desktop-portal/` overrides it per interface
(`portals.conf(5)`).

### Which application is asking

Built without systemd, the front end names a caller's application from the cgroup its process is
in: the scope systemd's naming would give it, `app-<launcher>-<id>-<n>.scope`. A KDOS box is
created in `app-kdos-<box>-<n>.scope`, so the portal sees each box's requests under the box's
launcher id, and the permission store remembers a choice per box. A program started from a
terminal has no such scope and an empty id, so nothing is remembered for it.

### The capture keys

`Print` takes the whole screen, `Shift+Print` a rectangle, `Meta+Shift+S` a rectangle too, and
`Alt+Print`, `Meta+R` or `Meta+Shift+R` record a rectangle, all in Spectacle; see
[Shortcuts and window rules](shortcuts-and-window-rules.md#the-kdos-shortcuts).

## Usage

### Taking a screenshot or a recording

1. Press one of the capture keys, or start **Spectacle** from Kickoff.
2. Choose what to capture: the whole screen, the current screen, the active window or a
   rectangle; or switch to **Recording**.
3. Spectacle shows the result, to annotate, copy, save or drag into another application.

From a terminal: `spectacle -b -n -f -o ~/shot.png` takes the whole screen with no window
(`-a` the active window, `-r` a rectangle).

### Sharing a screen or a window with an application

A browser, a video-call program or OBS Studio asks the ScreenCast portal. KDE's dialog lists the
screens and windows; pick one and **Share**. While it runs, the privacy lamp for the screen is lit
in the system tray; a click on it stops every screen capture that can be stopped. The program
receives a PipeWire stream and no access to the rest of the screen.

### Sharing the whole desktop over VNC or RDP

1. Start **Krfb** (VNC) or turn on System Settings → **Remote Desktop** (RDP, krdp), and set a
   password.
2. Open the port in the firewall:

   ```bash
   kdos-power firewall vnc on      # or: rdp on
   ```

3. Connect from the other machine to port 5900 (VNC) or 3389 (RDP).
4. Close the port again with `kdos-power firewall vnc off`.

Both serve the session through the RemoteDesktop and ScreenCast portals, so KWin asks before the
first connection is let in.

### Opening files from a boxed application

A boxed application's **Open** and **Save** dialogs are the KDE file dialog, drawn outside the
box, and the box receives only the file you pick, through the document portal.

## Tips

- `kdos privacy` lists what is capturing the microphone, the camera or the screen right now, and
  in which box.
- GTK applications use the portal file chooser because every login shell exports
  `GTK_USE_PORTAL=1`.

## Troubleshooting

**A request hangs or fails with no dialog.** The backend did not start. Check from Konsole:

```bash
qdbus6 org.freedesktop.portal.Desktop
qdbus6 org.freedesktop.impl.portal.desktop.kde
```

Each lists its objects when it runs. Their output goes to the session bus's log,
`$XDG_RUNTIME_DIR/ksvc/kdos-svc.dbus.log`. A dialog whose QML import is missing does not open and
the request fails; the log names the module.

**A screen share shows a black picture.** The program asked for a format KWin cannot give on
software rendering, or for a window that was minimised. Share the screen instead of the window,
or check `qdbus6 org.kde.KWin /KWin supportInformation` for the renderer.

**A box's dialog does not remember the choice.** The box's process is not in its
`app-kdos-<box>-<n>.scope`: `cat /proc/<pid>/cgroup` for the program shows where it is.

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items

## See also

- [Portals](../../kdos/03-architecture/session.md#portals): how the session starts and routes them
- [Boxes](boxes.md): what a box is given, and its launchers
- [Firewall](firewall.md): the named services `vnc` and `rdp`
- [Audio](audio.md): PipeWire, which carries every capture

<!-- book-nav -->
---

*Part XVI. System administration, chapter 442.* Previous: [441. Accessibility](accessibility.md) · [Contents](../README.md) · Next: [443. Diagnostics](diagnostics.md)
