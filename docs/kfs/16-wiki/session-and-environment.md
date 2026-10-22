# Session and environment

This page covers the graphical [session]{.idx}: the two scripts that start it after the login on
`tty1`, the environment every desktop program inherits, how to add a variable of your own, how to
end and restart a session, how to reach a running session from `tty2` or over [SSH]{.idx}, and what to
check when the desktop does not come up. The login itself, and whether `tty1` asks for a password,
is on [Login and autologin](login-and-autologin.md). What each part of the session does and why it
is started in that order is explained in [The session](../../kdos/03-architecture/session.md); this
page gives the procedures.

## Installation

The session is not a service and has no script in `/etc/init.d`. The login shell on `tty1` starts
it, and everything in it runs as the logged-in user. Its pieces:

- The base filesystem (`fs/` in the tree) supplies the two start scripts,
  `/usr/local/bin/kdos-desktop` and `/usr/local/bin/kdos-desktop-start`, the functions both source,
  `/usr/local/lib/kdos/session-common.sh`, the list writer `/usr/local/bin/kdos-session-save`, and
  `/etc/profile.d/10-wayland.sh`. No package owns them.
- [kdos-comp](#port-kdos-comp) is the [compositor]{.idx} the session runs in the foreground; it starts the
  panel and the other supervised programs. See [The panel and the launcher](panel-and-launcher.md).
- [seatd](#port-seatd) hands the compositor the display and input devices. Its service, `45_seatd`,
  runs at boot with `-g seat`, so members of `seat` may take a seat.
- [dbus](#port-dbus) provides the per-user [session bus]{.idx} the scripts start.
- [pipewire](#port-pipewire), [wireplumber](#port-wireplumber),
  [xdg-desktop-portal](#port-xdg-desktop-portal), [xdg-desktop-portal-wlr](#port-xdg-desktop-portal-wlr)
  and [fcitx5](#port-fcitx5) are started by `kdos-desktop-start` when they are installed.

The live medium and an installed system run the same session. On the live medium the home
directory is on the boot overlay, so anything a session writes there, including the files on this
page under `~/.config`, is gone at power-off unless a persistence store is set up with
[`kdos persist`](../../kdos/04-programs/kdos-command.md#kdos-persist).

## Configuration

### ~/.bash_profile

A new account's profile, from `/etc/skel`, starts the desktop on `tty1` and nowhere else:

<!-- kfs:include fs/etc/skel/.bash_profile lines=22-26 pin=b96a3af15b1c -->

`~/.zprofile` does the same for an account whose shell is zsh. The line is not `exec`, so when the
desktop exits or fails to start, the login shell carries on to a prompt.

### /usr/local/bin/kdos-desktop

The first script stops when the compositor is not installed:

<!-- kfs:include fs/usr/local/bin/kdos-desktop lines=7-11 pin=382d0335a0d6 -->

On a virtio GPU it turns the hardware cursor off (`WLR_NO_HARDWARE_CURSORS=1`), and when that GPU
has no 3D support it puts the compositor on software rendering (`WLR_RENDERER=pixman`); see
[Virtual machines](virtual-machines.md#running-kdos-as-a-guest). It then sets up the runtime
directory, the keyboard layout, the box warm-up and the session bus, and hands over to the second
script:

<!-- kfs:include fs/usr/local/bin/kdos-desktop lines=44-52 pin=ca4700af0db5 -->

### /usr/local/bin/kdos-desktop-start

The second starts audio, the portals and the input method, then runs the compositor with its
standard error kept in a log:

<!-- kfs:include fs/usr/local/bin/kdos-desktop-start lines=142-150 pin=4a1bc6f2516c -->

What it does when the compositor exits:

<!-- kfs:include fs/usr/local/bin/kdos-desktop-start lines=160-188 pin=550e5678fa39 -->

### /etc/profile.d/10-wayland.sh

Every login shell reads this file, so every program the session starts inherits what it exports.
It points a shell that the session did not start at the session's bus:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=36-41 pin=767720f2e945 -->

and it chooses each toolkit's back end:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=71-95 pin=b852ff9235db -->

Every variable it sets is tabulated in
[Shell environment](../../kdos/06-reference/configuration.md#shell-environment). A box receives
none of them; see [The environment a box receives](../../kdos/03-architecture/session.md#the-environment-a-box-receives).

### The keyboard layout

[`kdos-desktop`]{.idx} turns the console [keymap]{.idx} in [`/etc/keymap`]{.idx}, which the installer writes, into a
graphical layout. The names that differ between the two vocabularies are translated, and any other
name gives its first two letters:

<!-- kfs:include fs/usr/local/lib/kdos/session-common.sh lines=43-64 pin=343f63196495 -->

The live medium has no `/etc/keymap`, so nothing is exported and its session types in xkbcommon's
default, US.

### ~/.config/kdos-comp/environment

An optional file the compositor reads when it starts and again when it reloads its configuration,
with any `*.env` file in `~/.config/kdos-comp/environment.d/` beside it. The image ships neither.
Each `KEY=value` line replaces the value the login shell exported, for the compositor and for
everything it starts afterwards. A value may use `~` and `$VARIABLE`. For example:

```ini
XKB_DEFAULT_LAYOUT=de
XCURSOR_SIZE=48
```

Never write `FOO=$FOO:bar`: every reload appends again, and once the value passes the
compositor's size limit the line is refused. The rules are in
[`~/.config/kdos-comp/environment`](../../kdos/06-reference/configuration.md#configkdos-compenvironment).

### Files that switch something on

| File | Effect | Read |
|---|---|---|
| `~/.config/kdos/session-restore` | At login, relaunch the boxed applications listed in `~/.local/state/kdos/session` | At login |
| `~/.config/kdos/first-run` | Its absence shows the keyboard shortcut card at login; `kdos-keys` writes it once the card has been seen | At login |
| `~/.config/kdos/apps-offered` | Its absence repeats the offer to install the applications chosen during installation | At login |

## Usage

### Starting the desktop by hand

When `tty1` shows a shell prompt instead of the desktop, after a logout or with [autologin]{.idx} off:

1. Log in on `tty1`.
2. Run:

   ```bash
   kdos-desktop
   ```

   The compositor's log scrolls on the console while it starts.

### Ending the session

There are three ways out. Only the first writes the list of open applications that
[Reopening applications at login](#reopening-applications-at-login) reads.

- **From the desktop:** press `Super+Escape` and answer **Yes**. The compositor's root menu has
  **Log Out**, **Restart** and **Shut Down** rows that do the same, but a right click on the
  wallpaper opens that menu only with `desktop_icons = no` in `comp.conf`; with desktop icons on,
  the wallpaper's menu is the desktop's own, and it has none of those rows.
- **From a terminal the compositor started:** this exits at once, with no prompt:

  ```bash
  kdos-comp -e
  ```

- **From `tty2` or over SSH:** `kdos-comp -e` fails there with `LABWC_PID not set`, because that
  shell was not started by the compositor. Send the compositor `SIGTERM` by its exact name
  instead:

  ```bash
  pkill -x kdos-comp
  ```

Each of these is a clean exit, and it leaves a shell prompt on `tty1`, still logged in. Type
`exit` there to end the login; with autologin on, `tty1` logs in again and starts a new session.
fcitx5, `kdos-ime` and the capture portal end with the compositor. The session bus, PipeWire,
WirePlumber, the portal front end and the KDOS portal backend are left running, and so are the
loop that stops idle boxes and the per-user timers; a session started again on the same boot
reuses the bus and PipeWire instead of starting second copies.

### Restarting after a crash

When the compositor exits with a non-zero status, `tty1` shows the last 20 lines of its log and
asks:

```text
r=restart session, anything else=exit to tty:
```

1. Read the lines; the whole log is `$XDG_RUNTIME_DIR/kdos-comp.log`.
2. Type `r` and Enter to start the session again, or anything else for a shell prompt.

After the third crash within 60 seconds the question is not asked; the script prints
`kdos-comp: crashed 3 times in 60s — not restarting again.` and leaves the prompt. Fix the cause
and start the desktop by hand.

### Setting a variable for every desktop program

1. Create `~/.config/kdos-comp/environment` and add the line, for example `QT_SCALE_FACTOR=1.5`.
2. Reload the compositor from a terminal it started:

   ```bash
   kdos-comp -r
   ```

   From anywhere else, signal it instead, as with `pkill -HUP -x kdos-comp`. Each reload logs
   `read environment file /home/<you>/.config/kdos-comp/environment`.

3. Start the program again. A program that is already running, the panel included, keeps the
   environment it was started with; one the compositor starts after the reload, such as a panel
   it restarts, has the new value, with `~` already expanded.

For every account and every login shell, including text consoles and SSH, put the export in a file
of your own under [`/etc/profile.d/`]{.idx term="/etc/profile.d"} instead, named so that it sorts after `10-wayland.sh`, and log
in again. A file of your own keeps the change apart from `10-wayland.sh`, which is the image's.

### Reaching a running session from tty2 or SSH

A login shell on another terminal already has `XDG_RUNTIME_DIR` and, while the session bus is up,
`DBUS_SESSION_BUS_ADDRESS`. It has no display.

1. Log in as the same account on `tty2` (`Ctrl+Alt+F2`) or over SSH.
2. Talk to the desktop through the bus or the compositor's socket, which need nothing more:

   ```bash
   kdos notify "Hello from tty2"
   kdos hey list
   ```

3. To open a window on the session's screen, find the compositor's socket and name it:

   ```bash
   ls "$XDG_RUNTIME_DIR"/wayland-*
   WAYLAND_DISPLAY=wayland-0 foot &
   ```

   The socket is the name without `.lock`; use the one the listing shows. The window opens on the
   session's screen and is listed by `kdos hey list` like any other.

   An X11 program needs `DISPLAY` as well; see [Xwayland and X11 applications](xwayland-and-x11-apps.md).

### Reopening applications at login

1. Create the switch:

   ```bash
   touch ~/.config/kdos/session-restore
   ```

2. End sessions with `Super+Escape`, or with **Log Out**, **Restart** or **Shut Down** in the
   compositor's root menu; those are the routes that write the list.

At the next login each boxed application on the list is started again, two seconds apart.
Programs that are not boxed are written to the list and not restarted. Remove the file to stop.

### Changing the keyboard layout for the session

1. To change it for the console and the session together, write the console keymap name to
   `/etc/keymap` and log in again:

   ```bash
   echo de-latin1 | sudo tee /etc/keymap
   ```

2. To change only the graphical layout, set `XKB_DEFAULT_LAYOUT` (and `XKB_DEFAULT_VARIANT`,
   `XKB_DEFAULT_OPTIONS` if you need them) in `~/.config/kdos-comp/environment` and log in again.
   A value there replaces the one `kdos-desktop` derived from `/etc/keymap`.

## Tips

- `kdos why /usr/local/bin/kdos-desktop` answers `package  (none — provided by fs/ or
  10_bootstrap, or not installed)`, then the reason recorded for the fixed session-bus path: the
  scripts come from the base filesystem.
- The previous session's log is kept beside the current one as `kdos-comp.log.old`, so a crash's
  log survives one restart. A session that has not been restarted since boot has no `.old`.
- `kdos-desktop-start` exports `XDG_CURRENT_DESKTOP=KDOS` over whatever the login shell set, so
  setting it in a profile file has no effect inside the session.
- Shell aliases and functions from `~/.bashrc` do not reach desktop programs; only exported
  variables do.
- The per-user timers in `~/.config/kdos/timers.d` run under the login, not under the compositor;
  see [Timers](timers.md).

## Troubleshooting

**`tty1` prints "kdos-desktop: no desktop installed (kdos-comp missing)."** The compositor package
is not installed. `kpkg info kdos-comp` confirms it.

**`tty1` stays at a shell prompt after login.** The profile starts the desktop only when the
terminal is `/dev/tty1`, `WAYLAND_DISPLAY` is unset and `kdos-desktop` is on the path. Check
`~/.bash_profile` (or `~/.zprofile`) against the one in `/etc/skel`, then run `kdos-desktop` by
hand to see its messages.

**The screen is black, or the desktop flashes and drops back to the console.** Read the end of the
compositor's log from `tty2`:

```bash
tail -50 /run/user/$(id -u)/kdos-comp.log
kdos doctor
```

`kdos doctor`'s Session section checks the compositor's socket, the runtime directory, and that
`kdos-comp`, `kdos-shell` and `xdg-desktop-portal-wlr` are running. The compositor takes the
display through `seatd`; `service status seatd` shows whether it is up, and what its script printed
at boot is in `/run/kdos-init.seatd.log`.

**The session's keyboard layout is US although `/etc/keymap` names another.** `kdos-desktop`
printed `keymap '<name>' has no XKB layout '<layout>' — leaving the default` on `tty1`. Set the
right layout in `~/.config/kdos-comp/environment`.

**A variable set in the environment file has no effect.** Check the compositor's log for the file
being read and for a refused line:

```bash
grep -E 'environment file|ignoring environment' "$XDG_RUNTIME_DIR/kdos-comp.log"
```

Then remember that only programs started after the reload see the new value.

**A program started from `tty2` or SSH fails with "cannot open display" or finds no Wayland
display.** That shell has no `WAYLAND_DISPLAY` or `DISPLAY`; follow
[Reaching a running session from tty2 or SSH](#reaching-a-running-session-from-tty2-or-ssh).

**A boxed application has no sound, no notifications or never opens a window, while host programs
work.** The box meets the host through `$XDG_RUNTIME_DIR`; see
[The session message bus](../../kdos/03-architecture/session.md#the-session-message-bus) for what
breaks without it.

## Known gaps

- [There is no graphical login screen (greeter)](../../kdos/06-reference/known-gaps.md#there-is-no-graphical-login-screen-greeter)
- [Only two ways of ending the session save the list of open windows](../../kdos/06-reference/known-gaps.md#only-two-ways-of-ending-the-session-save-the-list-of-open-windows)
- [The power button and the sleep key work only inside a graphical session](../../kdos/06-reference/known-gaps.md#the-power-button-and-the-sleep-key-work-only-inside-a-graphical-session)
- [A live session cannot create a persistent box](../../kdos/06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box)
- The live image's automatic login and default password: see
  [Security](../../kdos/06-reference/known-gaps.md#security)

## See also

- [The session](../../kdos/03-architecture/session.md): every step from the login to the desktop, and how it ends
- [kdos-comp](../../kdos/04-programs/kdos-comp.md): the compositor's options, log and start-up files
- [Shell environment](../../kdos/06-reference/configuration.md#shell-environment): every file in `/etc/profile.d`
- [The session on tty1](../../kdos/02-user-guide/administration.md#the-session-on-tty1): the login side
- [Login and autologin](login-and-autologin.md), [Compositor keybindings](compositor-keybindings.md),
  [The panel and the launcher](panel-and-launcher.md), [Xwayland and X11 applications](xwayland-and-x11-apps.md),
  [Locale and console](locale-and-console.md), [Live persistence](live-persistence.md),
  [Portals and screen sharing](portals-and-screen-sharing.md), [Desktop troubleshooting](desktop-troubleshooting.md),
  [Timers](timers.md), [Logs](logs.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 411.* Previous: [410. Virtual machines](virtual-machines.md) · [Contents](../README.md) · Next: [412. Compositor keybindings](compositor-keybindings.md)
