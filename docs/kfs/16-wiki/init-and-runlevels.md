# Init and runlevels

KDOS boots with System V [init]{.idx}, sysvinit's, as process 1. This page covers the file init
reads, [`/etc/inittab`]{.idx}, the [runlevels]{.idx term="runlevel"} it defines, and the tasks that go with them: finding the
current runlevel, reloading the table after an edit, entering [single-user]{.idx term="single-user mode"} and emergency mode, and
shutting the machine down. Starting and stopping individual services is on
[Services](services.md). Every command on this page runs on a running KDOS machine, from a shell on
a console or in the desktop's terminal.

## Installation

Init is part of every KDOS image and is never enabled or disabled; there is nothing to install.
The pieces come from these ports and from the base filesystem:

- [sysvinit](#port-sysvinit) installs `init` and its other names, [`telinit`]{.idx}, `halt` (also as
  `reboot` and [`poweroff`]{.idx}), [`shutdown`]{.idx}, `runlevel` and `killall5`, all in `/sbin`:

<!-- kfs:include ports/core/base/sysvinit/build.sh lines=12-18 pin=af77623621f2 -->

- [util-linux](#port-util-linux) installs [`agetty`]{.idx}, which runs every console login, and [`sulogin`]{.idx},
  the single-user login.
- [kdos-tools](#port-kdos-tools) installs [`kdos-getty`]{.idx}, a second name of `ksvc`, which the `tty1`
  and `tty2` entries run first.
- The base filesystem supplies `/usr/local/sbin/kdos-login`, which the `tty1` entry runs to choose
  between an automatic login and a login prompt.
- [utmps](#port-utmps) keeps the login records init writes; see
  [Login records](../../kdos/03-architecture/boot-and-init.md#login-records).
- The base filesystem, `fs/` in the tree, supplies `/etc/inittab` and the scripts it names under
  [`/etc/init.d`]{.idx}: `rcS`, `rcK`, `killprocs`, `killpower` and `serial-getty`. The first bootstrap
  step copies them into the root; see [The file system](../03-bootstrap/000-file-system.md#copying-fs).

The live medium and an installed system run the same init with the same table, with one
difference: the installer removes `--autologin root` from the serial console's entry in the
installed copy of `/etc/inittab`, so an installed system asks for a login on `ttyS0` where the live
medium logs root in with no password.

## Configuration

### /etc/inittab

The whole table, as the image ships it, from `fs/etc/inittab`:

<!-- kfs:include fs/etc/inittab -->

Each line is `id:runlevels:action:process`. What each entry starts is tabulated in
[/etc/inittab](../../kdos/06-reference/configuration.md#etcinittab); the actions and the order of
a boot are explained in
[rcS and the service scripts](../../kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts).
`inittab(5)`, installed by sysvinit, is the format's reference.

### /etc/init.d/rcS

`rcS` is the `sysinit` entry. After mounting the filesystems it runs every numbered service script,
and this is the loop that does it:

<!-- kfs:include fs/etc/init.d/rcS lines=134-173 pin=403936cc08ac -->

### /etc/init.d/rcK

`rcK` is the first entry of runlevels 0 and 6. It stops the same set of services in reverse order,
leaving the firewall loaded:

<!-- kfs:include fs/etc/init.d/rcK lines=38-59 pin=b1f33d37e9bd -->

### /etc/init.d/serial-getty

The serial console's entry runs this wrapper, which sleeps when no UART answers at `ttyS0`:

<!-- kfs:include fs/etc/init.d/serial-getty -->

### The kernel command line

Two words on the [kernel command line]{.idx} change what init does at boot: `single` and `emergency`. The
initramfs passes both on to init. Their meaning is in
[Command-line parameters KDOS reads](../../kdos/03-architecture/boot-and-init.md#command-line-parameters-kdos-reads).

## Usage

### Finding the runlevel

1. Print the previous and the current runlevel:

   ```bash
   runlevel
   ```

   On a running system this prints `N 3`: no previous runlevel, and runlevel 3.

2. Print the time of the last boot from the login records `utmpd` keeps:

   ```bash
   who -b
   ```

   It prints a line such as `system boot  Oct 21 19:11`. `who -r` prints the runlevel from the
   same records, with the time init entered it, as a line such as
   `run-level 3  Oct 21 19:11  last=S`; `who` shows the `N` of "no previous runlevel" as `S`.
   After a change of runlevel the line names the new one, and `last=` the one before it.
   `runlevel` reads the records too and, should they hold no runlevel, answers from
   `/run/runlevel`, the one-character file init writes at every change of runlevel.

### Applying an edit to /etc/inittab

1. Edit `/etc/inittab` as root.
2. Tell init to read it again:

   ```bash
   sudo telinit q
   ```

An entry whose program exits at once is restarted by init until it has respawned ten times in two
minutes, and is then left alone for five minutes. Check that every program a new entry names exists
before reloading.

After an upgrade of the [sysvinit](#port-sysvinit) package, make the running init re-execute
itself, so that process 1 runs the new binary:

```bash
sudo telinit u
```

### Single-user mode

Single-user mode runs `sulogin` on the console, which asks for root's password and opens a root
shell, and starts no consoles and no desktop. Leaving that shell continues to runlevel 3.

`sulogin` refuses to open a shell while root's password is locked, which is how the installer
leaves root unless it was asked to set a root password. Set one first:

```bash
sudo passwd root
```

Then choose one of three ways in:

1. **From the boot menu, on an installed system.** Pick the `KDOS (single user)` entry. `rcS` runs
   as on every boot, so every enabled service starts, and `sulogin` runs after it. The live medium's
   menu has no such entry: its entries are `KDOS Live`, `KDOS Live (clean session)`,
   `KDOS Live (verbose)` and the memory test.
2. **From a running system.** This ends every console, the desktop session on `tty1` and the
   serial console with them:

   ```bash
   sudo telinit S
   ```

   init prints `INIT: Sending processes configured via /etc/inittab the TERM signal`, then the
   `KILL` one, and `sulogin` asks
   `Enter root password for system maintenance (or press Control-D to continue):`. A wrong
   password gets `Login incorrect` and the same question again.

3. **Emergency mode.** In the boot menu, press `E` on an entry, add the word `emergency` to the
   kernel command line and boot it. `sulogin` then runs before `rcS`, so only the mounts the
   initramfs handed over exist, and nothing from `/etc/fstab` is mounted.

Every way in is described in
[Single-user and emergency mode](../../kdos/03-architecture/boot-and-init.md#single-user-and-emergency-mode).

### Shutting down and rebooting

1. From a shell, as root, any of these asks init for runlevel 0 (power off) or 6 (reboot):

   ```bash
   sudo poweroff
   sudo reboot
   sudo shutdown -h now
   sudo shutdown -r now
   ```

2. Without `sudo`, a member of the `seat` or `wheel` group asks `kdos-powerd`, which runs
   `telinit 0` or `telinit 6` itself:

   ```bash
   kdos-power poweroff
   kdos-power reboot
   ```

   The panel's and the Start menu's power items send the same request. Who may send it is in
   [Who may ask for what](../../kdos/04-programs/daemons.md#who-may-ask-for-what).

3. At a text console, `Ctrl+Alt+Del` runs `shutdown -r now`, the `ca` entry of the table.

What happens between the request and the power going off, entry by entry, is in
[Shutdown](../../kdos/03-architecture/boot-and-init.md#shutdown).

### The recovery console

`tty2` always has a plain `agetty` login with no automatic login, whatever happens on `tty1`. When
the desktop does not come up, press `Ctrl+Alt+F2` (`Alt+F2` at a text console) and log in there.

## Tips

- `kdos why /etc/inittab` reports `package  (none — provided by fs/ or 10_bootstrap, or not installed)`: the file
  comes from the base filesystem, so no package upgrade replaces an edit to it.
- Every process field in the table is free of shell metacharacters, so init executes it directly
  rather than through `/bin/sh -c`. Keep an added entry the same way, or put the command in a script
  and name the script.
- init hands its children `PATH=/sbin:/usr/sbin:/bin:/usr/bin`, which does not include
  `/usr/local`, where the `kdos` commands live. `rcS` and `rcK` set a longer one for the services,
  but an entry of your own in `/etc/inittab` gets init's: name a program under `/usr/local` by its
  full path.
- Each console's `kdos-getty` writes its messages to `/run/kdos-getty.<tty>.log`, such as
  `/run/kdos-getty.tty1.log`. The file exists for `tty1` and `tty2` on every boot and is empty
  when the font, the colours and the keymap loaded without a complaint.
- A script in `/etc/init.d` without the `NN_` prefix, such as `serial-getty`, is never run by `rcS`
  or `rcK`. That is the place for a helper an `inittab` entry calls.

## Troubleshooting

**The splash stops on a stage and marks it failed.** The stage is named after the service script in
capitals. Log in on `tty2` and read the script's output from that boot:

```bash
cat /run/kdos-init.<name>.log
```

The table in [When a boot stops](../../kdos/03-architecture/boot-and-init.md#when-a-boot-stops)
says where to look for every other stage. The files under `/run` are gone after the next reboot.

**Single-user mode prints "Cannot open access to console, the root account is locked".** Root has no
password. Press Enter; init continues to runlevel 3. Set a root password with `sudo passwd root`
and try again.

**The system log shows init "respawning too fast" for an entry.** The program the entry names exits
at once. Run the entry's process field by hand as root to see why, correct the entry, and run
`sudo telinit q`.

**`telinit` reports `timeout opening/writing control channel /run/initctl`.** init is not reading
its request channel, and `shutdown`, `reboot` and `poweroff` go through the same channel. A
`SIGUSR2` sent to init closes it for the rest of the boot. Save your work, then:

```bash
sync
sudo reboot -f
```

`-f` calls the kernel's reboot directly and skips `rcK`, so no service stops cleanly.

**The screen is black after the splash.** The session did not start. Log in on `tty2` and follow
[The session](../../kdos/03-architecture/session.md); `kdos doctor` checks the compositor's socket
and the session's processes.

## Known gaps

- [There is no graphical login screen (greeter)](../../kdos/06-reference/known-gaps.md#there-is-no-graphical-login-screen-greeter)
- [The power button and the sleep key work only inside a graphical session](../../kdos/06-reference/known-gaps.md#the-power-button-and-the-sleep-key-work-only-inside-a-graphical-session):
  at a text console or in single-user mode, shut down with a command
- The live image's serial console is a root shell with no password: see
  [Security](../../kdos/06-reference/known-gaps.md#security)

## See also

- [rcS and the service scripts](../../kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts): how a boot runs, from init to the login
- [Shutdown](../../kdos/03-architecture/boot-and-init.md#shutdown): every step from the request to the power going off
- [/etc/inittab](../../kdos/06-reference/configuration.md#etcinittab): each entry and what it starts
- [The console](../../kdos/03-architecture/boot-and-init.md#the-console): `kdos-getty` and the console font
- [Services](services.md), [Writing a service](writing-a-service.md), [Logs](logs.md),
  [The initramfs and the rescue shell](initramfs-and-rescue.md), [Login and autologin](login-and-autologin.md),
  [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 358.* Previous: [Part XVI. System administration](intro.md) · [Contents](../README.md) · Next: [359. Services](services.md)
