# Administration

Running a KDOS machine: services, networking, storage, users, hardware enablement, media
support, updates and diagnosis. This is the page for the ordinary jobs an owner does after the
install. For how any of it is built, follow the links into
[Architecture](../03-architecture/overview.md).

There is no configuration abstraction layer here. Every setting below is a file that takes
effect, and [Configuration](../06-reference/configuration.md) is the complete list of them.

## Services

There is no systemd. Services are scripts in `/etc/init.d`, run in numeric order by `rcS` at boot,
and supervised by `ksvc`.

```sh
service <name> start|stop|status
ksvc     <name> start|stop|status     # the same program
```

The shipped set, in boot order:

| Script | What it starts |
|---|---|
| `01_udev` | Device management, and the coldplug that loads drivers |
| `02_modules` | Modules listed in `/etc/modules-load.d` |
| `05_hostname` | The hostname |
| `10_sysctl` | Kernel parameters |
| `12_zram` | Compressed swap in RAM |
| `15_userdirs` | `/run/user/<uid>`, and a cgroup subtree per user |
| `20_dmesg`, `22_syslog` | Kernel and system logging |
| `25_nftables` | The firewall — **before** the network comes up |
| `30_network` | Basic networking |
| `35_chrony` | Time synchronisation |
| `40_dbus` | The system message bus |
| `42_networkmanager` | NetworkManager |
| `45_avahi` | mDNS |
| `45_seatd` | Seat management — the desktop needs this |
| `50_alsa` | Sound card state |
| `55_powerd` | Suspend, poweroff and reboot for the desktop |
| `55_tlp` | Laptop power management |
| `56_energyd` | Per-application energy attribution |
| `57_oomd` | Memory-pressure protection |
| `58_mountd` | Removable media |
| `59_packd` | Application packs |
| `60_bluetooth` | Bluetooth |
| `70_sshd` | SSH |
| `80_cups` | Printing |

**A daemon that cannot do its job on this machine is skipped, not started and left to fail.**
`56_energyd` checks for a readable CPU energy counter, `57_oomd` checks that the kernel's pressure
interface is writable, and `59_packd` checks the kernel can mount the pack filesystem. This
matters because a refusing daemon under a respawn loop is a boot that never settles, so the check
happens *before* supervision begins.

Two scripts are deliberately **not** supervised — `25_nftables` and `12_zram` — because in both
cases the kernel holds the result and the program is supposed to exit.

## Networking

NetworkManager is the manager, with `wpa_supplicant` for wireless, `dnsmasq` for DNS, and polkit
granting `wheel` the right to change things without a password.

| Tool | For |
|---|---|
| `kdos-net` (`Super+F4`) | The desktop network manager |
| `nmtui` | The full text interface, including 802.1X and OTP VPNs |
| `nmcli` | Scripting |

`kdos-net` covers ordinary wired and wireless networks. It has **no secret agent**, so a
passphrase is written into the connection when it is created and NetworkManager cannot come back
and ask for another one — enterprise authentication and one-time-password VPNs still need
`nmtui`.

OpenVPN is available through NetworkManager, with certificate and password authentication.
Hardware tokens are not built.

## The firewall

`/etc/nftables.conf` ships a default workstation policy and `25_nftables` loads it before the
network starts, so there is no window in which the machine is up and unfiltered.

| | |
|---|---|
| Input | **Drop** by default |
| Forward | **Drop** |
| Established and related | Accept |
| Loopback | Accept |
| ICMP and **ICMPv6** | Accept the necessary types |
| mDNS (5353), DHCPv6 (546) | Accept |

ICMPv6 is answered rather than dropped, because dropping it does not harden IPv6 — it breaks
neighbour discovery and path MTU discovery.

**Anything that should be reachable needs a rule.** sshd, a shared printer or a served corpus are
all blocked by the shipped policy; the file carries commented examples. The loader runs a syntax
check first, so an unloadable ruleset leaves the previous state standing rather than half-applying
a flush.

```sh
sudo nft -c -f /etc/nftables.conf     # check
sudo service nftables start           # apply
```

## Storage

**Swap.** `swapon -a` runs at boot after `mount -a`. The installer can create a swapfile; its
`fstab` entry is what makes it active.

**zram** gives you compressed swap in RAM. `/etc/kdos/zram.conf` has two keys:

```
size = 50
algorithm = zstd
```

`size` is a **percentage of RAM**, not an absolute size, and it is how much swap the device may
claim to hold — never how much memory it will occupy, since the compressed pages live in that same
memory.

**Removable media** are handled by a root daemon and reached through `kdos-devices` (`Super+F6`).
Everything removable is mounted `nosuid,nodev` and, by default, `noexec`; `exec = yes` in
`/etc/kdos/mountd.conf` is how you say you meant it. A setuid binary on someone else's stick is a
local root hole that predates every other consideration.

The daemon refuses to offer an internal disk, a filesystem the kernel cannot mount, anything
named in `/etc/fstab`, and the medium the system booted from.

## The session on tty1

**The console desktop is the default session**, so administering this machine starts here rather
than with the compositor. `/etc/inittab` gives `tty1` to `kdos-getty`, which loads the console font
and hands over to `kdos-con-login`; that reads `/etc/kdos/con.conf` and either logs an account in or
draws the greeter. `tty2` stays a plain getty and is the recovery console.

`/etc/kdos/con.conf` — the keys an administrator changes:

| Key | Does |
|---|---|
| `greet` | `yes` draws the login surface; `no` logs in the account `autologin` names |
| `autologin` | Which account `greet = no` logs in. **The only place the desktop's account is named** |
| `sessions` | How many workspaces, and so how many cells the pager draws |
| `terminal` | What `Super+Return` opens |
| `scrollback` | Lines a terminal window keeps after they scroll off |
| `idle_saver`, `idle_lock`, `idle_off` | Seconds from the last input, each measured from that input rather than from the step before it |
| `remote` | Whether `kdos con forward` will carry a view socket off the machine |
| `embed` | Whether a graphical application becomes a window or takes a terminal of its own |

**Renaming the desktop user rewrites `autologin`.** It is named in one place, and `kinstall` rewrites
that place; a second copy elsewhere would log in an account the installed system does not have,
leaving the machine reachable only from `tty2` — which is what the shipped file's own comment warns
about. `kdos-getty`'s fallback getty reads the same key for the same reason.

**Sessions are addressed by name, not by pid.** `kdos con ls` lists them, `attach` and `detach` move
a display on and off one, and `kill` asks a session to end rather than unlinking its sockets. See
[the `kdos` command](../04-programs/kdos-command.md#con).

**Two sockets per session, and only the view socket may be forwarded** — forwarding the surface
socket would hand the far end the right to place windows in the session. `remote = no` is enforced
by `kdos con forward` refusing, because at the accepting end a forwarded socket's peer is the local
`ssh` process running as the same user and cannot be told from a local display.

## Users and groups

One human account ships: `kdos`, in `wheel` and in the hardware groups. `wheel` is what `sudo` and
polkit grant on, and what the root daemons check with `SO_PEERCRED` before answering.

Adding a user is `useradd` and adding them to the groups you want. There is no wizard.

The desktop user's group memberships are load-bearing rather than cosmetic — `dialout`, `audio`,
`video`, `render`, `input`, `kvm`, `cdrom`, `seat` and `tty` are each what makes a class of hardware
usable without root. Removing one has a specific, silent consequence.

**`tty` is the one that is not about hardware.** `/dev/tty0` is `0620 root:tty` and `/dev/console`
is `0600 root:root`, and a session that has been backgrounded has no controlling terminal for
`/dev/tty` to resolve to — so without that group the console session cannot carry a `VT_OPENQRY` on
any device, and a graphical application that needs a terminal of its own is refused with "no free
terminal" on a machine that has plenty.

## Hardware

### Firmware

`linux-firmware` ships **whole**, unpruned, installed with upstream's own script so that the
alias symlinks drivers actually request are created. A curated subset would be a bet on which
hardware you have, and losing that bet is silent.

`sof-firmware` is separate and is required for audio on Tiger Lake and newer — both the DSP
firmware and the topology files, because firmware with no topology loads and binds nothing, which
is still silence.

`wireless-regdb` ships **prebuilt and must stay that way**. The kernel verifies upstream's
signature on it, so a locally regenerated database is rejected silently and leaves the radio in
the world regulatory domain: working, with no 5 GHz DFS channels and reduced transmit power, and
nothing anywhere saying why.

### Microcode

CPU microcode is loaded by the kernel's **early** loader, which runs before any filesystem
exists, so it rides in front of the initramfs as an uncompressed archive. Late loading is
disabled, so this is the only path.

```sh
kdos doctor        # reports the running microcode revision
```

That check matters because an initramfs rebuilt without the microcode step has **no symptom** —
the processor simply keeps whatever the firmware loaded.

### The device groups and the udev rules

The console user can open hardware because of two halves that are both required: membership of
`dialout`, and rules granting that group the device classes this system ships tools for. The group
alone grants nothing; the rules alone have no group to grant to.

| Rules file | Devices |
|---|---|
| `70-kdos-serial.rules` | USB serial adapters — FTDI, CP210x, CH341, CDC-ACM |
| `70-kdos-debug.rules` | In-circuit debuggers and programmers |
| `70-kdos-sdr.rules` | Software-defined radio front ends |
| `70-kdos-usbtmc.rules` | USB Test & Measurement: scopes, meters, function generators |
| `70-kdos-camera.rules` | PTP/MTP cameras, for gphoto2 |
| `70-kdos-scanner.rules` | Flatbed and sheet-fed scanners, for SANE |

All are `MODE="0660"` rather than world-readable: these are devices other users on a multi-user
machine have no business reading.

**One blacklist is load-bearing.** `/etc/modprobe.d/kdos-sdr.conf` blacklists `dvb_usb_rtl28xxu`,
because the kernel otherwise claims an RTL2832U dongle as a DVB-T tuner on plug-in and the SDR
library cannot open a device that is present, enumerated and listed by `lsusb`.

Without the group and the rules, every serial programmer, development board, instrument and GPS
receiver in the catalogue is installed and unopenable — which presents as a broken cable rather
than as a permission. `kdos doctor` walks the attached devices and reports each one you cannot
open, naming the group that owns it, because "add yourself to dialout" is an instruction and
"permission denied" is not.

## Media, colour and time

The host's `ffmpeg` is built with the full codec set — H.264, HEVC, VP8/VP9, AV1 encode and
decode, MP3, Opus, Vorbis, FLAC, subtitle burn-in and hardware acceleration. One encoder per
format, deliberately: a second one for the same format earns nothing.

Building it that way relicenses the shipped binary to GPL-2-or-later, and everything that links
it inherits that. `ports/core/ffmpeg/LICENSE.notice` is the record a redistributor is expected to
read.

`lcms2` is the colour management engine, and it is the only thing on the host that can apply an
ICC profile.

Time zone data is a compiled zoneinfo tree that musl reads directly. It is built "fat" rather than
slim, because applications inside boxes read the same tree through the shared filesystem and a
format the box misreads would make host and box disagree about local time on one machine.

## Input methods

`fcitx5` is the engine, with Chinese (pinyin, shuangpin and the table methods), Japanese and
Korean available. It is started by name from the session, is Wayland-only, and is configured
through text files under `~/.config/fcitx5/` — there is no configuration tool, because that tool
is Qt.

Cloud completion is compiled out. Sending what you are typing to a remote service is not something
a distribution that builds offline should do by default.

## Keeping it current

```sh
kdos cve                  # which pinned versions carry known vulnerabilities, offline
kdos update               # orchestrate a system update
kdos app update           # update application packs
```

`kdos cve` compares your pinned versions against a vendored security database. A package that
database does not carry is reported **unknown**, never clean, and the summary says how many are in
that state — a checker that counted them as fine would be reporting a number it had not earned.
The database's age is printed with every run.

Host packages come from ports built here. A signed binary host is available if you run one — see
[Packaging](../03-architecture/packaging.md) — but there is no public archive to pull from.

## Diagnosing

```sh
kdos doctor       # the things that actually break on this distribution
kdos status       # what this machine is and what it is running
kdos restarts     # which supervised services have been restarting
kdos why <thing>  # why something is the way it is
```

`kdos doctor` is the first thing to run when something is wrong. It has a third report level
besides ok and warn — **skip, with a reason** — because half of what it asks cannot be answered in
a virtual machine, and reporting those as ok would be a green line for something never tested.

Its most useful check is *device present but unopenable*, described above. It also verifies the
setuid bits, which is the worst silent failure in the system: without its setuid bit the password
checker refuses every password and locks you out of your own session.

## Copying and rebuilding the medium

```sh
sudo kdos clone /dev/sdb        # write this medium to another stick
kdos rebuild /mnt/disk/work     # rebuild the ISO from the sources on the medium
```

**`kdos clone`** is a raw copy, and deliberately nothing cleverer: the boot arrangement is
whatever the medium already carries, so a copy boots exactly what the original boots.

Its length comes from the image's own self-description rather than from the device, so copying a
3 GB image to a 64 GB stick copies 3 GB. Confirmation is typing the device **name**, not `y`.
Before writing a byte it refuses four things: the medium this system booted from, any disk with a
filesystem mounted anywhere, anything named in `/etc/fstab`, and anything smaller than the image.

The verify afterwards **drops the page cache before re-reading**, because re-reading without that
hands back the bytes this process just produced rather than the bytes the flash stored — which is
exactly what a counterfeit stick does and exactly what the verify exists to catch.

**`kdos rebuild`** rebuilds the ISO from sources carried on the medium, with no network, on a
medium built with `make build KDOS_ISO_SOURCES=1`. Its checks are the valuable half: the work
directory is refused when it is on a temporary or overlay filesystem, because a live stick's root
is RAM and a rebuild started there reports gigabytes free, eats memory, and dies hours in.

## Tuning for this machine

```sh
kdos march probe          # which instruction set levels this CPU has
kdos march run lz4        # build lz4 twice and measure
kdos march report         # the ledger: kept, reverted, unmeasurable
```

This builds a port with and without the newer instruction set, runs that port's own benchmark
against both, and keeps the flags only where the win clears both a fixed floor and the machine's
own measured noise. A win inside the noise is not a win, and the tool says so.

The report lists reverts as prominently as wins. That is the evidence the measuring is real.

## See also

- [Configuration](../06-reference/configuration.md) — every file and key named here
- [The daemons](../04-programs/daemons.md) — what each root service owns
- [Boot and init](../03-architecture/boot-and-init.md) — the boot path and the service convention
- [The kdos command](../04-programs/kdos-command.md) — every diagnostic on this page
- [Packaging](../03-architecture/packaging.md) — ports, the binhost and updates
- [Security model](../03-architecture/security-model.md) — what `wheel` means and what is not protected
