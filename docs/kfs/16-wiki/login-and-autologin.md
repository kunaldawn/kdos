# Login and autologin

KDOS has no display manager. The desktop is reached by logging in on the first console, `tty1`,
and the login shell's profile starts it. This page covers that path on a running machine: which
programs make it up, the one file that decides whether `tty1` asks for a password, how to turn
[autologin]{.idx} on and off, the recovery console on `tty2`, the serial console, and what to do
when a login does not reach the desktop.

## Installation

Every image has the login path; nothing on it is optional. It is made of these packages and a few
files of the base filesystem:

- [util-linux](#port-util-linux) provides [`agetty`]{.idx}, which opens the terminal and prints the
  prompt, and [`sulogin`]{.idx}, used for single-user mode. It is built with `--disable-login` and
  `--disable-su`, so [`login`]{.idx} and `su` come from shadow.
- [shadow](#port-shadow) provides `login`, `su`, `passwd` and the account tools (`useradd`,
  `usermod`, `userdel`). It is built `--without-libpam`, so the console login checks
  [`/etc/shadow`]{.idx} itself and runs no PAM module.
- [kdos-tools](#port-kdos-tools) provides [`kdos-getty`]{.idx} in `/usr/local/sbin`, the wrapper every
  console getty in [`/etc/inittab`]{.idx} runs under; see
  [kdos-banner and kdos-getty](../../kdos/04-programs/kdos-command.md#kdos-banner-and-kdos-getty).
- [kdos-powerd](#port-kdos-powerd) provides the `autologin` verb that changes the setting without
  editing a file as root, and its client `kdos-power`.
- The base filesystem (`fs/` in the tree, see [The fs overlay](../01-preparing/fs-overlay.md))
  provides `/etc/inittab`, [`/etc/kdos/login.conf`]{.idx}, `/usr/local/sbin/kdos-login` and the account
  skeleton in `/etc/skel`.

The login path has no service script: [`init`]{.idx} starts the consoles itself from `/etc/inittab`.
Logins are recorded by `utmpd` and `wtmpd`, which `06_utmps.sh` starts; see
[Login records](../../kdos/03-architecture/boot-and-init.md#login-records).

The live medium and an installed system differ in three places:

| | Live medium | Installed system |
|:--|:--|:--|
| `tty1` | Logs in [`kdos`]{.idx term="kdos (command)"} without a password | Asks for a password, unless the [installer]{.idx}'s answer file set `autologin = yes` |
| The `kdos` password | `kdos` | The one given to the installer |
| The serial console `ttyS0` | Logs in `root` without a password | Asks for a login |

The live column is what a booted live medium shows. The installed column is what the installer's
Configure step writes into the target's `login.conf`, `inittab` and `shadow`; an installed disk
has not been booted to check it.

## Configuration

### /etc/kdos/login.conf

The only setting on the login path: the account `tty1` logs in without asking. The file comes from
`fs/etc/kdos/login.conf`:

<!-- kfs:include fs/etc/kdos/login.conf -->

The key, its default and when it applies are in
[/etc/kdos/login.conf](../../kdos/06-reference/configuration.md#etckdosloginconf). An absent,
commented or empty key means a password prompt. On an installed machine the installer edits the
line rather than rewriting the file: `autologin = <user>` when autologin was chosen,
`#autologin = <user>` when it was not; see [autologin](../../kdos/04-programs/kinstall.md#autologin).

### /etc/inittab

`init` reads the consoles from this file, `fs/etc/inittab` in the tree. The three login lines:

<!-- kfs:include fs/etc/inittab lines=22-46 pin=2543de49292e -->

`tty1` runs [`kdos-login`]{.idx} under `kdos-getty`; `tty2` runs a plain `agetty` under `kdos-getty`; the
serial line runs [`/etc/init.d/serial-getty`]{.idx}, which hands its arguments to `agetty`. The file names
no account for `tty1`. On an installed system the installer cuts `--autologin root` from the `S0`
line and leaves the rest of the file as it ships. The rest of the file, the runlevels and the
shutdown entries, is on [Init and runlevels](init-and-runlevels.md).

### /usr/local/sbin/kdos-login

The program the `tty1` line runs, a short shell script from `fs/usr/local/sbin/kdos-login`. It
reads the last `autologin` line of `login.conf` and starts `agetty` with or without
`--autologin`:

<!-- kfs:include fs/usr/local/sbin/kdos-login lines=15-28 pin=bf1adf4b4daa -->

### ~/.bash_profile

The desktop is started by the login shell, not by `init`. The skeleton every account is created
from, `fs/etc/skel/.bash_profile`, starts [`kdos-desktop`]{.idx} on `tty1` only:

<!-- kfs:include fs/etc/skel/.bash_profile lines=11-26 pin=50aeabf5d274 -->

`kdos-desktop` is not `exec`'d, so a desktop that fails to start leaves a shell prompt on `tty1`.
`/etc/profile.d/10-wayland.sh` sets up `XDG_RUNTIME_DIR` and the session variables for every login
shell, `tty1`, `tty2`, serial and SSH alike.

### Other files

| File | Provided by | Holds |
|:--|:--|:--|
| `/etc/login.defs` | [shadow](#port-shadow), as upstream ships it | `login`'s and the account tools' defaults |
| [`/etc/passwd`]{.idx}, `/etc/group`, `/etc/shadow` | `fs/etc/`, then the installer | The accounts; the installer renames `kdos` to the user name it is given |
| `/etc/sudoers.d/00-sudo` | [sudo](#port-sudo)'s `build.sh` | `%wheel ALL=(ALL) ALL`, which the `sudo` commands below rely on |

## Usage

### Turning autologin on or off

The first two need membership of `wheel`, the third needs root. Each changes only the key's line in
`login.conf`, and the change applies the next time `tty1` starts its getty.

1. With the desktop: open Settings, go to **System**, choose **Accounts…** ([`kdos-users`]{.idx}), and
   switch the autologin there. The surface is described in
   [The system surfaces](../../kdos/04-programs/kdos-shell.md#the-system-surfaces).

2. From a shell, through `kdos-powerd`:

   ```bash
   kdos-power autologin kdos      # log in kdos on tty1 without asking
   kdos-power autologin off       # ask for a password on tty1
   ```

   The daemon refuses an account that cannot log in with `err no such account`, and writes the
   off state by commenting the key line, `#autologin = <user>`, keeping the account it named. Its rules are in
   [autologin](../../kdos/04-programs/daemons.md#autologin).

3. By hand, as root: edit `/etc/kdos/login.conf` and set or comment out the `autologin` line.
   `kdos-login` reads the last such line, so leave only one.

4. To apply the change without rebooting, end the desktop session and then the shell it leaves
   on `tty1` (`exit`). `init` respawns the `tty1` line, and the new `agetty` reads the file again.

### Choosing autologin when installing

The installer's wizard installs a machine that asks for a password on `tty1`. An answer file turns
autologin on with one key:

```text
autologin = yes
```

The key, and the `username` key it names, are in
[Answer files](../../kdos/04-programs/kinstall.md#answer-files).

### Logging in on the recovery console

`tty2` is a plain login prompt with no autologin, kept as the way in when the desktop on `tty1`
does not come up.

1. Press Ctrl+Alt+F2 from the desktop, or Alt+F2 at a text console.
2. Log in as your own account, or as `root` if root has a password.
3. Go back to the desktop with Alt+F1.

A login on `tty2` does not start the desktop: `~/.bash_profile` starts it on `tty1` only. From the
desktop the compositor does the switch, not the kernel; see
[The console](../../kdos/03-architecture/boot-and-init.md#the-console).

### Logging in on the serial console

1. Connect to the machine's first serial port, `ttyS0`. The `S0` line gives `agetty` the rates
   115200, 38400 and 9600.
2. On the live medium you get a root shell with no password. On an installed system you get a
   login prompt.

`serial-getty` sleeps instead of starting `agetty` when no UART answers at `ttyS0`, so a machine
without a serial port does not respawn a failing getty.

### Adding an account that can log in

`kdos-users` lists accounts and does not create them. Use shadow's tools, as
[Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups) shows:

```bash
sudo useradd -m -G wheel,seat,audio,video,render,input -s /bin/bash alice
sudo passwd alice
```

`useradd` prints `Creating mailbox file: No such file or directory`; the image has no mail spool
directory, and the account is made all the same.

The new account can log in on `tty2` at once, and over SSH when the server is on. To make it the
one `tty1` logs in, run `kdos-power autologin alice`. Until its runtime directory exists, every
login of the account prints `mkdir: cannot create directory ‘/run/user/<uid>’: Permission denied`,
because `/etc/profile.d/10-wayland.sh` runs as the account and `/run/user` is root's. Before its
first desktop login, create the directory, and the account's control group with it, with
`sudo service start userdirs`, or reboot.

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
- `who`, `w` and `last` read the records `utmpd` and `wtmpd` keep: on the live medium `who` lists
  `kdos` on `tty1` and `root` on `ttyS0`, and `last` ends with the boot's `reboot   system boot`
  line. With the `utmps` service disabled they report nobody.
- Renaming the desktop account means changing `login.conf` with it, or `tty1` fails to log in
  and the machine is reachable only from `tty2`.
- The login banner on a console is `kdos-banner`, run from `/etc/bash.bashrc`; `KDOS_NO_ANIM=1`
  prints it without animation. See
  [The login banner](../../kdos/03-architecture/boot-and-init.md#the-login-banner).

## Troubleshooting

**`tty1` shows a login prompt although autologin is set.**
Check the key: `grep autologin /etc/kdos/login.conf`. A commented line, an empty value or a
missing file all mean a prompt. `kdos-login` reads the file only when `init` starts the `tty1`
getty, so end the session and the shell on `tty1`, or reboot.

**`tty1` logs in and drops to a shell prompt instead of the desktop.**
The login worked and the session did not come up. Either the profile never ran `kdos-desktop`, or
it ran and exited.

1. Check what the profile tests: `tty` must print `/dev/tty1`, and `command -v kdos-desktop` must
   find the program. An account created without `-m`, or with a home directory that has no
   `.bash_profile`, never runs it; copy [`/etc/skel/.bash_profile`]{.idx} into the home directory.
2. Run `kdos-desktop` from the prompt and read what it prints. The compositor's own output is kept
   in `$XDG_RUNTIME_DIR/kdos-comp.log`.

**`tty1` never logs in, and its getty keeps restarting.**
`login.conf` names an account that does not exist, and `init` respawns the `agetty` that fails
for it. Log in on `tty2` and run `kdos-power autologin off`, or `kdos-power autologin <user>`
with an account that exists.

**`kdos-power autologin` answers ``kdos-power: no kdos-powerd — `service start powerd` ``.**
The daemon is not running. Start it with `sudo service start powerd`, the command the message
names. The daemon's log is `/run/kdos-svc.kdos-powerd.log`.
See [The client](../../kdos/04-programs/daemons.md#the-client).

**`kdos-power autologin` answers `kdos-power: err not permitted`.**
The caller is not in `wheel`. `sudo kdos-powerd --explain <user>` prints what the daemon allows that
user and why, as in `alice: uid 1001, primary gid 1001, in neither seat nor wheel — refused`; see [Diagnosis and testing](../../kdos/04-programs/daemons.md#diagnosis-and-testing).

**`kdos-getty` prints `not moved into /sys/fs/cgroup/user.slice/user-<uid>/session/cgroup.procs`.**
The account's control group does not exist: `15_userdirs` creates it at boot for the accounts in
`/etc/passwd` then, so an account added since has none, and its boxed applications run with no
memory limit. Run `sudo service start userdirs`, then end the session and log in on `tty1` again.

## Known gaps

- [There is no graphical login screen (greeter)](../../kdos/06-reference/known-gaps.md#there-is-no-graphical-login-screen-greeter)
- The live medium's serial console and its autologin, in
  [Security](../../kdos/06-reference/known-gaps.md#security)
- [A fingerprint unlocks sudo and nothing else](../../kdos/06-reference/known-gaps.md#a-fingerprint-unlocks-sudo-and-nothing-else):
  the console login asks for no finger

## See also

- [The session on tty1](../../kdos/02-user-guide/administration.md#the-session-on-tty1): the user
  guide's account of the same path
- [The console](../../kdos/03-architecture/boot-and-init.md#the-console) and
  [Login records](../../kdos/03-architecture/boot-and-init.md#login-records): the mechanism
- [autologin](../../kdos/04-programs/daemons.md#autologin): the `kdos-powerd` verb
- [Users and groups](../../kdos/02-user-guide/administration.md#users-and-groups), in the user
  guide: the shipped account and its groups
- [Users and groups](users-and-groups.md), [Screen lock](screen-lock.md), [SSH server](ssh-server.md),
  [Init and runlevels](init-and-runlevels.md), [Recovering an unbootable system](recovering-an-unbootable-system.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 381.* Previous: [380. sudo, PAM and polkit](sudo-pam-polkit.md) · [Contents](../README.md) · Next: [382. Screen lock](screen-lock.md)
