# Desktop troubleshooting

This page is for a desktop that does not come up, comes up wrong, or stops answering. It covers
the path from the login on `tty1` to the [compositor]{.idx}, the files on that path, how to reach a shell
when the screen is no use, where the compositor writes what went wrong, and the faults that have a
known cause. How a session is started, step by step, is in
[Starting a session](../../kdos/03-architecture/session.md#starting-a-session); this page links to
it rather than repeating it.

## Installation

The desktop is a handful of ports and a few scripts from the base filesystem:

| Piece | From | Is |
|:--|:--|:--|
| `kdos-comp` | [kdos-comp](#port-kdos-comp) | The compositor, a fork of labwc, which also starts and supervises the [panel]{.idx} and the session daemons |
| `kdos-shell` and its other names | [kdos-shell](#port-kdos-shell) | The panel, menus, Settings, notifications and the other surfaces |
| `kdos-lock` | [kdos-lock](#port-kdos-lock) | The [lock screen]{.idx} |
| [`seatd`]{.idx} | [seatd](#port-seatd) | Hands the compositor the display and input devices; the service `seatd`, from [`/etc/init.d/45_seatd.sh`]{.idx} |
| `dbus-daemon` | [dbus](#port-dbus) | The system bus (service `dbus`) and the [session bus]{.idx} the desktop starts |
| [Xwayland]{.idx} | [xwayland](#port-xwayland) | The X server for applications with no Wayland path, started on demand |
| `kdos-getty` | [kdos-tools](#port-kdos-tools) | Loads the console font and palette before each console's getty |
| `kdos-login` | The base filesystem, `fs/usr/local/sbin/` | Logs `tty1` in, as `/etc/kdos/login.conf` says |
| [`kdos-desktop`]{.idx}, `kdos-desktop-start`, `session-common.sh` | The base filesystem, `fs/usr/local/` | Bring the session up and run the compositor |

Both services ship enabled. The desktop itself is not a service: it is started by
the login shell on `tty1`. The live medium logs `kdos` in on `tty1` with no password asked; an
installed system logs in the account the installer created, or asks for a password if [autologin]{.idx} was
turned off. See [Login and autologin](login-and-autologin.md).

## Configuration

### ~/.bash_profile

The login shell starts the desktop on `tty1` only, and does not `exec` it, so a session that fails
leaves a shell. From `fs/etc/skel/.bash_profile`:

<!-- kfs:include fs/etc/skel/.bash_profile lines=15-26 pin=30881d056419 -->

An account whose shell is zsh has the same block in `~/.zprofile`.

### /etc/inittab

`tty2` is a plain login prompt, kept for exactly this page's purpose. From `fs/etc/inittab`:

<!-- kfs:include fs/etc/inittab lines=33-39 pin=527998a111ea -->

### /usr/local/bin/kdos-desktop

The command the login shell runs. It refuses when the compositor is not installed, from
`fs/usr/local/bin/kdos-desktop`:

<!-- kfs:include fs/usr/local/bin/kdos-desktop lines=7-11 pin=382d0335a0d6 -->

On a virtio [GPU]{.idx} it then turns off hardware cursors, and when the GPU does not offer virgl, or
when there is no render node at all, it sets `WLR_RENDERER=pixman`, so the compositor renders in
software. Last it prepares the runtime
directory, the [keymap]{.idx}, the boxes and the session bus, and hands over to `kdos-desktop-start`:

<!-- kfs:include fs/usr/local/bin/kdos-desktop lines=53-61 pin=ca4700af0db5 -->

### /usr/local/bin/kdos-desktop-start

Runs the compositor in the foreground with its log in a file, and offers a restart when it crashes.
From `fs/usr/local/bin/kdos-desktop-start`:

<!-- kfs:include fs/usr/local/bin/kdos-desktop-start lines=163-224 pin=97fd93ee7f98 -->

### ~/.config/kdos-comp/rc.xml and ~/.config/kdos/comp.conf

The compositor's two files. [`rc.xml`]{.idx} holds the bindings, and two rules in it break the whole
desktop without a message; the file's own header states the first:

<!-- kfs:include fs/etc/skel/.config/kdos-comp/rc.xml lines=7-18 pin=8523e302d495 -->

The second is that `--` may not appear inside an XML comment. [`comp.conf`]{.idx} holds the KDOS keys; a
line in it that does not take effect is named in the log with its line number. Both are described
in [Configuration](../../kdos/04-programs/kdos-comp.md#configuration).

### $XDG_RUNTIME_DIR/kdos-comp.log

The compositor's log, usually `/run/user/1000/kdos-comp.log`, with the previous session's beside
it as `kdos-comp.log.old`. It is a tmpfs file and does not survive a reboot.

## Usage

### Getting a shell when the desktop is no use

1. Press Ctrl+Alt+F2. The compositor handles the key even while the screen is locked. `tty2`
   shows a plain `kdos login:` prompt.
2. Log in on `tty2` as the desktop's account: `kdos` on the live medium, password `kdos`, or the
   account the installer created.
3. Return to the desktop with Alt+F1.

If Ctrl+Alt+F2 does nothing, the compositor is not reading the keyboard. Log in over [SSH]{.idx} from
another machine if the SSH server is enabled ([SSH server](ssh-server.md)), or hold the power
button.

### Reading what went wrong

From `tty2`:

```bash
tail -n 40 /run/user/1000/kdos-comp.log
tail -n 40 /run/user/1000/kdos-comp.log.old
```

The previous session's log is the one to read after a crash and a restart; `kdos-comp.log.old`
exists only once the desktop has been started a second time since boot. Lines that begin with
`crt:` are about the phosphor pass. Lines that name `comp.conf` are settings the compositor could
not use, each with its line number, or changes that wait for the next login.

### Restarting the desktop

After a crash, `tty1` shows the last 20 lines of the log and asks:

```text
r=restart session, anything else=exit to tty:
```

Type `r` and Enter to start the session again. Anything else leaves a shell prompt on `tty1`; type
`kdos-desktop` there to start it, or `exit` to log out, after which autologin starts it again.

A third crash within 60 seconds stops the offer: `tty1` prints `kdos-comp: crashed 3 times in
60s — not restarting again.` and is left at the shell.

To stop a desktop that is still running but useless, from `tty2`:

```bash
pkill -x kdos-comp
```

The compositor takes `SIGTERM` as a request to end the session, so it exits as a logout does and
`tty1` is left at the shell prompt, with no restart question. The session bus and PipeWire stay
up, and a desktop started again on `tty1` uses them. If it does not exit, kill it:

```bash
pkill -KILL -x kdos-comp
```

A killed compositor counts as a crash, and `tty1` shows the restart question. Either way, return
to `tty1` with Alt+F1 and answer the question, or run `kdos-desktop` at the shell.

### Restarting one part of the desktop

The panel, the desktop icons, notifications and the other supervised programs are restarted by the
compositor when they exit. To restart the panel, from a terminal in the session:

```bash
pkill -x kdos-shell
```

A supervised program that keeps exiting is given up on, and a dialog names it; the limit is in
[When a child keeps crashing](../../kdos/04-programs/kdos-comp.md#when-a-child-keeps-crashing).
`kdos-comp -r` resets the count and gives it another run.

### Reloading the configuration

From a terminal inside the session:

```bash
kdos-comp -r
```

From `tty2` or SSH the command fails with `LABWC_PID not set`; send the signal instead:

```bash
pkill -HUP -x kdos-comp
```

Keys that apply at the next login, such as the fonts and `panel`, wait for a new session whatever
the reload; see [Keys applied at the next login](../../kdos/04-programs/kdos-comp.md#keys-applied-at-the-next-login).

### Starting again from the shipped configuration

1. From `tty2`, move your copies aside:

   ```bash
   mv ~/.config/kdos-comp ~/.config/kdos-comp.mine
   mv ~/.config/kdos/comp.conf ~/.config/kdos/comp.conf.mine
   ```

2. Copy the shipped ones back:

   ```bash
   cp -r /etc/skel/.config/kdos-comp ~/.config/
   cp /etc/skel/.config/kdos/comp.conf ~/.config/kdos/
   ```

3. Restart the desktop as above.

The theme files under `~/.config/kdos-comp/themerc-override` and the rest are written by
`kdos theme`; run `kdos theme <accent>` after step 2 to put them back.

### A session with debug logging

From the shell on `tty1`:

```bash
KDOS_COMP_DEBUG=1 kdos-desktop
```

Every late frame is then logged as well, so the log grows quickly. Use it for one session.

## Tips

- `kdos doctor`, run in a terminal inside the session, checks the compositor's socket, the panel,
  the portals, the setuid helpers and the session daemons in one go. See
  [Diagnostics](diagnostics.md).
- Keep `tty2` in mind before editing `rc.xml`: a broken file can leave a desktop with no working
  keys, and `tty2` is how you put it back.
- Check `rc.xml` after every edit:

  ```bash
  xmllint --noout ~/.config/kdos-comp/rc.xml
  ```

- `Super+F1` shows the key-binding card generated from your own `rc.xml`; `kdos help` prints the
  sheet built into the program.
- In a [virtual machine]{.idx} the idle timers default to off, so a blank screen there is not the idle
  policy.

## Troubleshooting

**`tty1` shows a shell prompt and no desktop.**
Read what was printed above the prompt. `kdos-desktop: no desktop installed (kdos-comp missing).`
means `/usr/bin/kdos-comp` is not installed: the root was built without the `50_desktop` phase. Otherwise the compositor exited: read
`/run/user/1000/kdos-comp.log` and type `kdos-desktop` to try again.

**The compositor exits at once, and its log names libseat or seatd.**
`seatd` is not running, or the account is not in the `seat` group:

```bash
service status seatd
cat /run/kdos-svc.seatd.log
id
```

Start it with `sudo service start seatd`; add the account with `sudo usermod -a -G seat <user>`
and log in again.

**Clicking a window does not focus it, title bars cannot be dragged, the window buttons do
nothing.**
`<default />` is missing from the `<keyboard>` or `<mouse>` section of `~/.config/kdos-comp/rc.xml`,
so every built-in binding is gone. Put it back as the first child of each, then `kdos-comp -r`. See
[The one line that must not be lost](../../kdos/04-programs/kdos-comp.md#the-one-line-that-must-not-be-lost).

**Some or all key bindings do nothing.**
`rc.xml` does not parse, and a compositor that cannot parse it loads none of its bindings. Run
`xmllint --noout ~/.config/kdos-comp/rc.xml`; the usual cause is `--` inside a comment. See
[The double hyphen may not appear inside an XML comment](../../kdos/04-programs/kdos-comp.md#---may-not-appear-inside-an-xml-comment).

**There is no panel.**
Check `panel` in `~/.config/kdos/comp.conf` is not `off`, then whether it is running:

```bash
pgrep -a kdos-shell
```

If a dialog said the panel was given up on, read the log for its last lines, then run
`kdos-comp -r` to give it another run.

**`kdos doctor` warns `WAYLAND_DISPLAY=… but … does not exist`.**
The shell was started in a session that has ended, and the variable outlived it. Open a new
terminal from the running desktop, or restart the desktop.

**The desktop is slow and there is no phosphor glow in a virtual machine.**
Without virgl, `kdos-desktop` sets `WLR_RENDERER=pixman` and the compositor renders in software,
where the phosphor pass stays off. Give the virtual machine 3D acceleration.

**The keyboard layout is US whatever `/etc/keymap` says.**
`kdos-desktop` translates the console keymap's name into an XKB layout name. When no XKB layout of
that name is installed, it prints `kdos-desktop: keymap '<name>' has no XKB layout '<layout>'` on
`tty1` and leaves the default, US, for the whole session, the lock screen included. Set a layout
XKB knows, as described in [Locale and console](locale-and-console.md), and log in again.

**X11-only applications do not open.**
`kdos doctor` reports the Xwayland socket in its Desktop section. A launcher started outside the
compositor may have no `DISPLAY`; `kdos-appbox` finds the socket in `/tmp/.X11-unix` itself, a
shell from SSH does not.

**The screen is locked and the password is refused.**
If `kdos doctor` warns `kdos-checkpass is not setuid root`, the lock screen refuses every
password. Repair it from `tty2`; see [Screen lock](screen-lock.md).

**A change in Settings or `comp.conf` seems to do nothing.**
Read the log for a line naming `comp.conf` and the line number. A key that applies at the next
login says so there.

**Screen sharing finds no screen.**
See [Portals and screen sharing](portals-and-screen-sharing.md).

## Known gaps

- [There is no graphical login screen (greeter)](../../kdos/06-reference/known-gaps.md#there-is-no-graphical-login-screen-greeter)
- [Two shipped chords and two routes show nothing](../../kdos/06-reference/known-gaps.md#two-shipped-chords-and-two-routes-show-nothing)
- [Only two ways of ending the session save the list of open windows](../../kdos/06-reference/known-gaps.md#only-two-ways-of-ending-the-session-save-the-list-of-open-windows)
- [A tray menu does not update while it is open](../../kdos/06-reference/known-gaps.md#a-tray-menu-does-not-update-while-it-is-open)
- [Workspace occupancy is inferred, not reported](../../kdos/06-reference/known-gaps.md#workspace-occupancy-is-inferred-not-reported)
- [The power button and the sleep key work only inside a graphical session](../../kdos/06-reference/known-gaps.md#the-power-button-and-the-sleep-key-work-only-inside-a-graphical-session)

## See also

- [Starting a session](../../kdos/03-architecture/session.md#starting-a-session): every step from `tty1` to the compositor
- [Supervised chrome](../../kdos/03-architecture/session.md#supervised-chrome) and
  [When a child keeps crashing](../../kdos/04-programs/kdos-comp.md#when-a-child-keeps-crashing)
- [Debugging](../../kdos/04-programs/kdos-comp.md#debugging): the compositor's log and debug switches
- [The console](../../kdos/03-architecture/boot-and-init.md#the-console): `tty1`, `tty2` and `kdos-getty`
- [Diagnostics](diagnostics.md), [Logs](logs.md), [Login and autologin](login-and-autologin.md),
  [Screen lock](screen-lock.md), [Displays and scaling](displays-and-scaling.md),
  [Portals and screen sharing](portals-and-screen-sharing.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 420.* Previous: [419. Diagnostics](diagnostics.md) · [Contents](../README.md) · Next: [421. Recovering an unbootable system](recovering-an-unbootable-system.md)
