# Login and autologin

The desktop is reached through [SDDM]{.idx}, the display manager, whose greeter runs on its own
KWin on the first console, `tty1`. This page covers that path on a running machine: which programs
make it up, the files that decide whether the machine shows the greeter or logs somebody in by
itself, how to turn [autologin]{.idx} on and off, the recovery console on `tty2`, the serial
console, and what to do when a login does not reach the desktop. What happens after the login is
on [Session and environment](session-and-environment.md).

## Installation

Every image has the login path; nothing on it is optional:

| Piece | From | What it does |
|---|---|---|
| `/usr/local/sbin/kdos-dm` | the base filesystem | Started on `tty1` by `/etc/inittab`; makes the greeter account's home, takes the locale from `/etc/locale.conf` and runs `sddm`. When SDDM is missing or exits three times within a minute it puts a password prompt on `tty1` instead, never an autologin |
| `sddm`, `sddm-helper`, `sddm-greeter-qt6` | [sddm](#port-sddm) | The display manager, the helper that opens each PAM session, and the greeter, run as the `sddm` account (uid 64) on `kwin_wayland` |
| The KDOS greeter theme | [kdos-sddm-theme](#port-kdos-sddm-theme) | The theme `kdos`, matched to the splash and the lock screen |
| The Login Screen page | [sddm-kcm](#port-sddm-kcm) | System Settings → **Login Screen (SDDM)**: the theme, autologin and the session's defaults |
| PAM's `system-login` stack and `/bin/login` | [pam](#port-pam), [util-linux-login](#port-util-linux-login) | Every login is a PAM login: `pam_turnstile` makes the runtime directory and starts the user's services, `pam_elogind` registers the session |
| `pam_kwallet5` | [kwallet-pam](#port-kwallet-pam) | Opens the user's wallet with the password typed at the greeter |
| `kdos-getty` | [kdos-tools](#port-kdos-tools) | Wraps `agetty` on `tty2`: loads the console font and palette, then hands over |
| [plasma-setup](#port-plasma-setup) | | The first-boot wizard of an OEM install, which logs in as its own account to make the first real one |

The live medium logs the `kdos` account in by itself; an installed machine shows the greeter
unless the install asked for autologin.

## Configuration

### /etc/inittab

The two console lines:

<!-- kfs:include fs/etc/inittab lines=24-51 pin=a25c6ec1fbcf -->

`kdos-dm` is started only once `rcS` has returned, and `rcS` lets go of the framebuffer
synchronously at its end, so the greeter's compositor never races the boot splash for the
display. Its log is `/var/log/sddm.log`, one boot per file, with the previous boot's in
`/var/log/sddm.log.old`.

### /etc/sddm.conf.d/

SDDM reads `/usr/lib/sddm/sddm.conf.d/` first, then this directory in name order, then
`/etc/sddm.conf`, each key's last value winning. `00-kdos.conf` is KDOS's: a Wayland greeter, the
KDOS session wrapper and no X sessions (see
[Session and environment](session-and-environment.md#how-sddm-starts-the-session)), the accounts
from uid 1000 up, and the theme, from `fs/etc/sddm.conf.d/00-kdos.conf`:

<!-- kfs:include fs/etc/sddm.conf.d/00-kdos.conf lines=56-69 pin=8cac08410de7 -->

[`kde_settings.conf`]{.idx term="/etc/sddm.conf.d/kde_settings.conf"} is the one home of
`[Autologin]`. The installer writes it, the Login Screen page of System Settings writes it, and it
overrides `00-kdos.conf`. The image ships the live medium's, from
`fs/etc/sddm.conf.d/kde_settings.conf`:

<!-- kfs:include fs/etc/sddm.conf.d/kde_settings.conf -->

`Relogin=true` logs the account in again when its session ends; Switch User still shows a greeter,
on a free VT, through [sddm](#port-sddm)'s `sddm-switch-user-greeter.patch`. An install replaces
the file:
with the new account's `[Autologin]` and `Relogin=false` on a single-account machine with an
encrypted root that asked for it, and with no `[Autologin]` anywhere else, which shows the greeter.

### The PAM stacks

`sddm` installs three stacks of its own, from `ports/core/auth/sddm/build.sh`:

<!-- kfs:include ports/core/auth/sddm/build.sh lines=39-73 pin=048145511062 -->

`sddm` is a password login and `sddm-autologin` one with no password, refused for any uid under
1000 except the OEM wizard's `plasma-setup`. `sddm-greeter` is the greeter's own session;
`pam_turnstile` is `required` there because the greeter's KWin will not start without a runtime
directory. Both user stacks include `system-login`, from `ports/core/auth/pam/build.sh`, which
`/bin/login` on the consoles uses as well:

<!-- kfs:include ports/core/auth/pam/build.sh lines=65-74 pin=4157cc080085 -->

**An autologin can start locked.** `sddm-autologin` puts `/etc/kdos/autologin-xdg` first in
`XDG_CONFIG_DIRS` for an automatic login and for nothing else. The image ships no such directory.
The installer makes it on the encrypted single-account machine that asked for autologin, with a
`kscreenlockerrc` of `[Daemon] LockOnStart=true`, so the session comes up behind the lock screen
and the disk passphrase alone does not open the desktop. Answering the installer's "no password
after the disk passphrase" leaves the directory out.

### Other files

| File | Holds |
|---|---|
| `/var/lib/sddm/` | The greeter account's home: the last user and session, the greeter's output settings, and its look in the machine's accent (`.config/kdeglobals`, `plasmarc`, `kcminputrc` and `kdos-background.png`). The image ships it with the default accent's look; `kdos-dm` makes it at every start when it is missing |
| `/etc/locale.conf` | The language the greeter speaks; see [Locale and console](locale-and-console.md) |
| `/etc/kdos/xkb.conf` | The greeter's keyboard layout, read by its KWin through locale1 |
| `/var/lib/AccountsService/icons/<user>` | An account's picture on the greeter, set on the Users page |
| `/etc/plasma-setup-done` | Present on every machine whose install made an account; its absence arms the first-boot wizard |

## Usage

### Turning autologin on or off

1. System Settings → **Login Screen (SDDM)** → **Behavior**.
2. Tick **Automatically log in**, pick the account and **Plasma (Wayland)** as the session, or untick
   it. Leave **Log in again immediately after logging off** unticked on a machine somebody leaves.
3. **Apply**. The page asks for an administrator's password and writes
   `/etc/sddm.conf.d/kde_settings.conf`. The change applies at the next boot, or the next time
   SDDM starts.

From a shell, edit `/etc/sddm.conf.d/kde_settings.conf` as root: an `[Autologin]` group with
`User=<account>` and `Session=plasma` logs that account in; no group shows the greeter.

### Choosing autologin when installing

The installer offers autologin only on a single-account install with an encrypted root, where
the disk passphrase stands in front of the desktop. See
[Users and the login](../../kdos/02-user-guide/installation.md) in the installation guide.

### Logging in on the recovery console

`tty2` is a plain login prompt with no autologin, kept as the way in when the desktop on `tty1`
does not come up.

1. Press `Ctrl+Alt+F2` from the desktop or the greeter, or `Alt+F2` at a text console.
2. Log in as your own account, or as `root` if root has a password.
3. Go back to the desktop with `Ctrl+Alt+F1`.

A login on `tty2` starts no desktop. It is an elogind session like any other, so it starts the
user's services when they are not already running, and it reaches a running desktop session's bus
(see [Session and environment](session-and-environment.md#reaching-a-running-session-from-tty2-or-ssh)).

### Logging in on the serial console

1. Connect to the machine's first serial port, `ttyS0`. The `S0` line gives `agetty` the rates
   115200, 38400 and 9600.
2. On the live medium you get a root shell with no password. On an installed system you get a
   login prompt: the installer removes `--autologin root` from the installed copy of `inittab`.

`serial-getty` sleeps instead of starting `agetty` when no UART answers at `ttyS0`, so a machine
without a serial port does not respawn a failing getty.

### Adding an account that can log in

System Settings → **Users** adds an account, its password, its picture and whether it is an
administrator, through AccountsService. From a shell, use shadow's tools, as
[Users and groups](users-and-groups.md) shows:

```bash
sudo useradd -m -G wheel,audio,video,render,input -s /bin/bash alice
sudo passwd alice
```

The account is offered on the greeter at once: SDDM lists every account from uid 1000 to 60000.

### Single-user mode

On an installed system, boot the menu's `KDOS (single user)` entry, or run `sudo telinit S` on a
running machine. The live medium's menu has no single-user entry. `sulogin`
asks for root's password. On an installed system root's password is locked unless one was set in
the installer, and `sulogin` gives no shell to a locked root; set a password first with
`sudo passwd root`. The rules are in
[Single-user and emergency mode](../../kdos/03-architecture/boot-and-init.md#single-user-and-emergency-mode).

## Tips

- `sudo kdos doctor` reports, in its Security section, whether the `kdos` account still has the
  shipped password `kdos`. The section appears only when `/etc/shadow` is readable, so run it with
  `sudo`.
- `loginctl list-sessions` lists every session elogind knows, the greeter's included, and which
  one is active; `who`, `w` and `last` read the records `utmpd` and `wtmpd` keep.
- The login banner on a console is `kdos-banner`, run from `/etc/bash.bashrc`; `KDOS_NO_ANIM=1`
  prints it without animation. See
  [The login banner](../../kdos/03-architecture/boot-and-init.md#the-login-banner).

## Troubleshooting

**`tty1` shows a text password prompt instead of the greeter.** `kdos-dm` fell back: SDDM is not
executable, there is no `sddm` account, or SDDM exited three times within a minute. Log in there
or on `tty2` and read why:

```bash
sudo grep kdos-dm /var/log/messages
sudo tail -40 /var/log/sddm.log
```

Logging out of the prompt starts `kdos-dm` again, which tries SDDM once the minute is past.

**The screen stays black after the splash, and `tty2` answers.** SDDM is running and its greeter
is not drawing; SDDM restarts a failing greeter itself, which `kdos-dm` cannot see. Read
`/var/log/sddm.log` for the greeter's errors. A greeter's KWin with no runtime directory, or one
refused the display, says so there; `loginctl list-sessions` should show a session of the `sddm`
user on `seat0`.

**The greeter comes back after the password.** The password was accepted and the session did not
start. The reason is in `~/.local/share/sddm/wayland-session.log`; see
[Session and environment](session-and-environment.md#troubleshooting).

**The machine logs in by itself although autologin was turned off.** Another file in
`/etc/sddm.conf.d/` or `/etc/sddm.conf` carries an `[Autologin]` group, and a later one wins:
`grep -rn -A3 Autologin /etc/sddm.conf.d /etc/sddm.conf`.

**The wallet asks for a password after an autologin.** `pam_kwallet5` opens the wallet with the
password typed at the greeter, and an autologin types none. Give the wallet an empty password in
KWalletManager, or log in with a password.

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the live image's automatic login and
  default password, among the desktop's open items

## See also

- [The session](../../kdos/03-architecture/session.md): what happens from the greeter to the desktop
- [The console](../../kdos/03-architecture/boot-and-init.md#the-console): `tty2` and the text consoles
- [Session and environment](session-and-environment.md), [Screen lock](screen-lock.md),
  [Users and groups](users-and-groups.md), [sudo, PAM and polkit](sudo-pam-polkit.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 407.* Previous: [406. sudo, PAM and polkit](sudo-pam-polkit.md) · [Contents](../README.md) · Next: [408. Screen lock](screen-lock.md)
