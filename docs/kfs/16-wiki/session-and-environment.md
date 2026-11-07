# Session and environment

This page covers the graphical [session]{.idx}: how SDDM starts a Plasma session through the KDOS
session wrapper, the environment every desktop program inherits, where the KDOS defaults for KDE
programs live, how to add a variable of your own, the per-user services that outlive a session,
how to end and restart a session, and how to reach a running session from `tty2` or over
[SSH]{.idx}. The login screen itself, and whether a machine logs in by itself, is on
[Login and autologin](login-and-autologin.md). What each part of the session does and why it is
started in that order is explained in [The session](../../kdos/03-architecture/session.md); this
page gives the procedures.

## Installation

The session is not a service and has no script in `/etc/init.d`. SDDM starts it after a login,
and everything in it runs as the logged-in user. Its pieces:

- [sddm](#port-sddm), the display manager, started on `tty1` by `/usr/local/sbin/kdos-dm` from
  `/etc/inittab` (see [Login and autologin](login-and-autologin.md)).
- [elogind](#port-elogind), which registers each login as a session, tracks which one is active
  on the seat, and hands KWin the display and input devices.
- [turnstile](#port-turnstile), whose PAM module makes the runtime directory `/run/user/<uid>` at
  a user's first login and starts that user's own services under `ksvc --user`: the session bus,
  PipeWire, WirePlumber, pipewire-pulse, the user's timers and the boxes' cleanup. They stop at the
  user's last logout.
- [plasma-workspace](#port-plasma-workspace), which installs `startplasma-wayland`,
  `plasma_session`, `ksmserver`, `plasmashell` and the session file
  `/usr/share/wayland-sessions/plasma.desktop`; [kwin](#port-kwin), the compositor; and the rest of
  the Plasma set, which the [kde-plasma](#port-kde-plasma) meta port names.
- The base filesystem (`fs/` in the tree) supplies the session wrapper
  `/usr/local/bin/kdos-session-wrapper`, the functions it shares,
  `/usr/local/lib/kdos/session-common.sh`, the login profile `/etc/profile.d/10-wayland.sh`, the
  user services in `/etc/kdos/user-services/`, and the KDOS defaults for KDE programs in
  `/usr/share/kdos/xdg/`. No package owns them.

The live medium and an installed system run the same session. On the live medium the home
directory is on the boot overlay, so everything a session writes there is gone at power-off
unless the medium has a persistence store; see [Live persistence](live-persistence.md).

## Configuration

### How SDDM starts the session

SDDM reads `/etc/sddm.conf.d/00-kdos.conf`. Its `[Wayland]` group names the greeter's compositor
and the command a session is started with, and its `[X11]` group offers no X session at all:

<!-- kfs:include fs/etc/sddm.conf.d/00-kdos.conf lines=35-49 pin=6d391837ffe9 -->

SDDM runs `kdos-session-wrapper` as the user, after PAM has opened the session, with the
`Exec=` line of `plasma.desktop` as its one argument. The wrapper re-runs itself as a login shell
so `/etc/profile`, `/etc/profile.d` and `~/.bash_profile` reach Plasma; checks that the session
bus at `$XDG_RUNTIME_DIR/bus` answers and points `DBUS_SESSION_BUS_ADDRESS` at it, refusing to start
the session when it does not, so the greeter comes back; and settles the environment before
anything reads it, from `fs/usr/local/bin/kdos-session-wrapper`:

<!-- kfs:include fs/usr/local/bin/kdos-session-wrapper lines=124-137 pin=003f91a5c8f0 -->

`kdos_session_xdg_dirs` puts the KDOS defaults into `XDG_CONFIG_DIRS` (below), the
`QT_IM_MODULE` and `GTK_IM_MODULE` variables are cleared so Qt and GTK reach the input method
through KWin, and `TERM`, `LINES` and `COLUMNS` are cleared because the session has no
terminal. `kdos_session_boxes` warms up the boxes behind the pinned favourites in the background.
Then `startplasma-wayland` runs. KDOS has no systemd user instance, so the session is always
Plasma's classic one, in which `plasma_session` starts `kded6`, `ksmserver`, `plasmashell` and the
autostart entries itself; the KDOS default that says so, from
`fs/usr/share/kdos/xdg/startkderc`:

<!-- kfs:include fs/usr/share/kdos/xdg/startkderc lines=8-18 pin=df42059fddd3 -->

### Where the KDOS defaults live

Every KDOS default for a KDE program is a file in [`/usr/share/kdos/xdg/`]{.idx term="/usr/share/kdos/xdg"}:
`kwinrc`, `kglobalshortcutsrc`, `kscreenlockerrc`, `powerdevilrc`, `plasmarc`, `breezerc`,
`startkderc` and the rest. The directory is the last entry of `XDG_CONFIG_DIRS`, after
`/etc/xdg`, so KDE's configuration cascade reads, for each key, the first of:

| Order | Directory | Whose |
|---|---|---|
| 1 | `~/.config` | The person's own: everything System Settings saves |
| 2 | `~/.config/kdedefaults` | What the global theme last applied |
| 3 | `/usr/share/kdos/xdg-vm`, `/usr/share/kdos/xdg-live` | Overlays, each only where it applies: in a virtual machine, no idle dimming or timed lock; on the live medium, no file index, no timed lock, no wallet and no update notice |
| 4 | `/etc/xdg` | The machine's administrator |
| 5 | `/usr/share/kdos/xdg` | KDOS |

So a file the administrator puts in `/etc/xdg` overrides the KDOS default key by key, and a
person's own setting overrides both. `/etc/xdg/kdeglobals` carries one key, the global theme, which
the installer writes from the accent chosen at installation.

The order is set in two places, so a session started by SDDM and a shell started any other way
agree: `kdos_session_xdg_dirs` in `/usr/local/lib/kdos/session-common.sh`, which also adds the two
overlays, and every login shell's profile, from `fs/etc/profile.d/10-wayland.sh`:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=60-63 pin=1a5ef55bd582 -->

### The environment every program inherits

`/etc/profile.d/10-wayland.sh` runs for every login shell, the session wrapper's included. It
fills in `XDG_RUNTIME_DIR` and `DBUS_SESSION_BUS_ADDRESS` for a shell no PAM session started (an
`su` without `-l`), names the desktop for a shell outside the session so files opened from a
console open in the desktop's applications, and chooses the toolkits' back ends:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=78-101 pin=0ed63f6cc1a0 -->

`/usr/share/kdos/xdg/plasma-workspace/env/kdos.sh` is the environment script Plasma itself
reads. `startplasma` sources every `plasma-workspace/env/*.sh` along `XDG_CONFIG_DIRS`, the
person's own `~/.config/plasma-workspace/env/` last, and takes every variable they leave set into
the session:

<!-- kfs:include fs/usr/share/kdos/xdg/plasma-workspace/env/kdos.sh lines=31-49 pin=d4b375c5794b -->

`kdos-shortcuts.sh` beside it copies the KDOS global shortcuts into a person's
`~/.config/kglobalshortcutsrc` once each; see
[Shortcuts and window rules](shortcuts-and-window-rules.md).

### The per-user services

turnstile starts a user's services at the first PAM login, the greeter's or a console's, and
stops them at the last logout. An SSH login opens no PAM session, because OpenSSH is built
without PAM, so it starts nothing and holds nothing open. Its configuration, from
`fs/etc/turnstile/turnstiled.conf`:

<!-- kfs:include fs/etc/turnstile/turnstiled.conf -->

`backend = ksvc` runs the scripts in [`/etc/kdos/user-services/`]{.idx term="/etc/kdos/user-services"}
under the user's own `ksvc`, each in name order: `10_dbus.sh` (the session bus at
`$XDG_RUNTIME_DIR/bus`), `20_pipewire.sh`, `21_wireplumber.sh`, `22_pipewire-pulse.sh`,
`25_mpd.sh`, `30_timers.sh` (the timers in `/etc/kdos/user-services/timers.d/` and the user's own)
and `40_boxes.sh`. A user service's log is `$XDG_RUNTIME_DIR/ksvc/kdos-svc.<name>.log`.

### The keyboard layout

The layout the login screen and every new session start with is the machine's, in
[`/etc/kdos/xkb.conf`]{.idx}, which [kdos-settingsd](#port-kdos-settingsd) serves as
`org.freedesktop.locale1`. The greeter's KWin reads it before anybody types, and `startplasma`
copies it into each session; a person's layout on the Keyboard page of System Settings, saved in
`~/.config/kxkbrc`, comes after it and wins. From `fs/etc/kdos/xkb.conf`:

<!-- kfs:include fs/etc/kdos/xkb.conf -->

## Usage

### Starting a session

Log in at the SDDM greeter, which offers one session, **Plasma (Wayland)**. A machine set to log
in by itself starts the session with no greeter at all; see
[Login and autologin](login-and-autologin.md).

From a console login on `tty2`, a second session can be started by hand. It runs with the
environment of that login shell and is registered with elogind as the `tty2` session:

```bash
startplasma-wayland
```

### Ending the session

**Log Out**, **Restart** and **Shut Down** are in the Application Launcher's Leave buttons and on
`Ctrl+Alt+Del`, which opens the logout screen. At the end of every session Plasma runs the scripts
in `plasma-workspace/shutdown/` along `XDG_CONFIG_DIRS`. The KDOS ones run in name order:
`kdos-boxes.sh` records the running boxes for the next login, then `kdos.sh` stops every box that
has run past its profile's `autostop`. The user's services keep running until the user's last login ends.

From a terminal inside the session:

```bash
qdbus6 org.kde.Shutdown /Shutdown logout
```

### Restarting part of a running session

```bash
plasmashell --replace &          # the panel and the desktop
kquitapp6 kded6; kded6 &         # the background modules
service --user restart pipewire  # one of your own services
service --user status            # all of your services
```

KWin cannot be replaced from inside the session it draws; a KWin that dies ends the session,
and SDDM shows the greeter again.

### Setting a variable for every desktop program

1. Make the directory: `mkdir -p ~/.config/plasma-workspace/env`.
2. Write a script ending in `.sh` that exports the variable, for example
   `~/.config/plasma-workspace/env/editor.sh` containing `export EDITOR=micro`.
3. Log out and back in. `startplasma` sources the script after the KDOS ones, so its value wins.

A variable meant for login shells as well, consoles and SSH included, belongs in
`~/.bash_profile` instead; the session wrapper reads that file too.

### Reaching a running session from tty2 or SSH

A console login of the same user shares the user's services, so it reaches the session bus and
everything on it. An SSH login has no runtime directory of its own: the login profile points it at
the one a still-open login of the same user keeps, so it reaches the bus only while that login
lasts, and `loginctl` shows no session for it:

```bash
echo "$DBUS_SESSION_BUS_ADDRESS"     # unix:path=/run/user/<uid>/bus
qdbus6 org.kde.KWin /KWin supportInformation | head
loginctl list-sessions               # every session elogind knows, and which is active
```

A graphical program started from there needs the session's display:
`WAYLAND_DISPLAY=wayland-0 kate`.

### Reopening applications at login

Plasma saves the open applications when a session ends and restarts the ones that support session
management at the next login. System Settings → **Session** → **Desktop Session** sets whether it
does, and which applications it leaves out.

### Changing the keyboard layout for the session

1. System Settings → **Keyboard** → **Layouts**: add a layout and turn on **Configure layouts**.
   The change applies at once and is yours alone.
2. For the machine's own layout, the one the login screen uses, see
   [Locale and console](locale-and-console.md).

## Tips

- `kdos doctor` checks that the session bus, KWin, plasmashell and elogind answer, and says which
  does not.
- `~/.bash_profile` runs before Plasma: a command there that waits holds the session behind the
  login screen, and one that prints writes into the session log.
- The session log is `~/.local/share/sddm/wayland-session.log`; SDDM's own is
  `/var/log/sddm.log`, with the previous boot's in `/var/log/sddm.log.old`.

## Troubleshooting

**The greeter comes back right after a login.** The session wrapper refused, or Plasma failed.

1. Log in on `tty2` (`Ctrl+Alt+F2`).
2. `tail -30 ~/.local/share/sddm/wayland-session.log`. A line from `kdos-session-wrapper` names
   what it refused, for example that the session bus does not answer.
3. `service --user status dbus` for the bus, and `grep kdos-session /var/log/messages` for
   the refusal in syslog.

**The desktop draws in Breeze's colours, with upstream's defaults.** The session has no
`/usr/share/kdos/xdg` in `XDG_CONFIG_DIRS`. Check it from Konsole: `echo "$XDG_CONFIG_DIRS"`. A
session started some way other than through SDDM or a login shell skips both places that add it.

**Notifications, single-instance applications or a box's portals do nothing.** The program is
on a private session bus rather than the user's. `echo "$DBUS_SESSION_BUS_ADDRESS"` in it must
read `unix:path=/run/user/<uid>/bus`.

**A variable set in `~/.config/plasma-workspace/env` is missing.** The script must end in `.sh`
and export the variable; an `unset` in it does not reach the session. Log out fully: the scripts
are read only at the start of a session.

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items, among them the
  live image's automatic login and default password

## See also

- [The session](../../kdos/03-architecture/session.md): every step from the login to the desktop, and how it ends
- [Shell environment](../../kdos/06-reference/configuration.md#shell-environment): every file in `/etc/profile.d`
- [Login and autologin](login-and-autologin.md): the greeter, autologin and the console logins
- [Desktop troubleshooting](desktop-troubleshooting.md): a desktop that does not come up
- [Services](services.md): `service --user`, and the system's own services

<!-- book-nav -->
---

*Part XVI. System administration, chapter 435.* Previous: [434. Virtual machines](virtual-machines.md) · [Contents](../README.md) · Next: [436. Shortcuts and window rules](shortcuts-and-window-rules.md)
