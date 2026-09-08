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
| `18_timers` | Periodic jobs, one supervised `snooze` per line of `/etc/kdos/timers.d` |
| `20_dmesg`, `22_syslog` | Kernel and system logging |
| `25_nftables` | The firewall — **before** the network comes up |
| `30_network` | Basic networking |
| `35_chrony` | Time synchronisation |
| `40_dbus` | The system message bus |
| `41_polkitd` | polkit — **before** NetworkManager, which asks it on its first privileged call |
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

### Periodic jobs

There is no cron daemon. A job is a line in `/etc/kdos/timers.d/*.timer`:

```
NAME  TIMESPEC...  --  COMMAND...
```

`18_timers` starts one supervised `snooze` per line. `snooze` sleeps until its next matching time,
runs the command once, and exits; the supervisor starts it again, so it sleeps again. What a cron
daemon would add over that is a scheduler, and this machine already has a supervisor — one process
per timer, and no shared state to corrupt.

**The timespec is `snooze`'s own** — `-H 4 -M 17` for 04:17, `-d /2` for every second day — and
nothing is invented here. **`-s` is the missed-run rule**, also `snooze`'s: a machine asleep at the
slot runs the job **once** when it wakes, if it wakes within the slack, rather than once per missed
occurrence. A laptop shut for a fortnight would otherwise run fourteen catch-up jobs at breakfast.

**The command is an argument vector, not a shell line.** No pipe, no redirection, no `&&`: a
program that must write a file takes a flag naming it, which is why `kdos update check` grew
`--out`. That is what lets the table be parsed rather than sourced, and it is why a line here
cannot run something nobody wrote.

**A line that does not parse is reported and skipped**, never guessed at — a timer that silently
did not start is indistinguishable from one that has not fired yet, and the difference can be
months.

Your own jobs go in `~/.config/kdos/timers.d/`, in the same shape. Those are started by the session
at login and **die with it**: a job writing into your home has no business outliving the login that
started it, and `ksvc` could not supervise them anyway — its pidfiles are in `/run`, which is
root's.

## Networking

NetworkManager is the manager, with `wpa_supplicant` for wireless, `dnsmasq` for DNS, and polkit
granting `wheel` the right to change things without a password.

| Tool | For |
|---|---|
| `kdos-net` (`Super+F4`) | The desktop network manager |
| `kdos-netagent` | The passphrase box NetworkManager raises; started with the session |
| `nmtui` | The full text interface |
| `nmcli` | Scripting |

`kdos-net` joins a network you can see, and the passphrase you type there is written into the
profile. **Every later question is `kdos-netagent`'s.** NetworkManager never prompts on its own:
when it is activating a profile whose secret is missing or refused it asks the agents registered
with it, and fails the activation in silence if none answers. So a key that has changed since,
802.1X enterprise wireless and a VPN one-time code all arrive as a box from the agent rather than
from the window you started in.

The agent stores nothing. NetworkManager also polls its agents for saved secrets on paths nobody
is watching, and those requests are answered at once rather than with a dialog; only a request
that carries the interaction flag raises the box.

OpenVPN is available through NetworkManager, with certificate and password authentication.
Hardware tokens are not built.

## Finding a file by name

`plocate` searches an index instead of the disk, which is what makes a whole-home "where is that
file called…" instant where `fd` has to walk the tree.

**The index is yours, not the machine's.** It is rebuilt nightly at 03:05 by a timer in
`~/.config/kdos/timers.d/`, scoped to `$HOME`, and written into `~/.cache/kdos/plocate.db`, so it
can only ever contain paths you could already list. `$LOCATE_PATH` in `/etc/profile.d` is what
points `plocate` at it.

Upstream ships a setgid binary reading one shared database for the whole machine, with a
permission check per result; KDOS ships neither the bit nor the shared file. The reasoning is in
[the security model](../03-architecture/security-model.md#and-no-setgid-ones-which-is-why-plocates-index-is-per-user).

`kdos-updatedb` rebuilds it now rather than waiting for the timer.

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

## Mail

Four programs, one directory. **`~/Mail` is the Maildir**, and everything that touches mail is
pointed at it:

| Program | Does |
|---|---|
| `mbsync` | Fetches. Makes an IMAP mailbox and `~/Mail` equal in both directions |
| `notmuch` | Indexes what is there. It never fetches and never sends |
| `aerc` | Reads and writes |
| `msmtp` | Sends. `/usr/sbin/sendmail` and `/usr/bin/sendmail` are both links to it |

**`notmuch new` is the only command you type.** A `pre-new` hook runs `mbsync -a` before the scan
and a `post-new` hook tags what arrived, so one command fetches, files and indexes. Until a
`Channel` is configured the hook steps over itself and `notmuch new` just indexes.

Three files to fill in, all shipped commented-out:

```
~/.mbsyncrc                            the server, and mode 600
~/.msmtprc                             the outgoing server, and mode 600
~/.config/notmuch/default/config       your own address
```

The fourth, `~/.config/aerc/accounts.conf`, is **not** shipped: aerc refuses to start on one that
anyone but you can read, and its own wizard writes it at mode 600 the first time you run `aerc`.
Let the wizard do it.

**Six kinds of part render as text, and nothing else does.** `~/.config/aerc/aerc.conf` names one
script, `kdos-part`, for everything that is not already plain text; a type it does not list gets
aerc's own *No filter configured* card and its `:open`, `:save` and `:pipe` hints. The script
spools the part to a temporary file first, because `mutool` opens a document by path and has no
form that reads a pipe:

| Part | Shown as |
|---|---|
| HTML | `w3m -dump`, which lays tables out on the grid, so marketing mail is readable |
| An invitation | aerc's calendar filter, run through `gawk`. It **reads** and never imports — a filter runs every time a message scrolls past, and one that accepted meetings by being looked at would accept them all |
| PDF | `mutool draw -F txt` |
| An image | `chafa`, as coloured symbols. This row is also what turns **off** aerc's own inline picture — with no filter matching, aerc draws a jpeg or png through the terminal's graphics protocol, which `kdos-term` has. The trade is the same picture in every terminal rather than a better one in some |
| A `.docx` | `docx2txt` |

**HTML is rendered behind something that cannot reach the network.** `kdos-part` tries
`unshare --map-root-user --net` first and runs w3m behind an unroutable proxy when that is refused;
either way a tracking pixel has nowhere to go. **On the live medium it is always the proxy** — no
process there can create a user namespace, root included (see
[what is missing](../06-reference/known-gaps.md)) — which is exactly why the filter probes instead
of assuming. aerc's own HTML filter assumes, by testing whether the `unshare` *binary* exists, and
so puts `unshare: Operation not permitted` where the message should be: a filter's error output
**is** the message body here, because aerc hands it the pager's own pipe.

Keep passwords out of all of them — `pass` is on the image, and both `PassCmd "pass show …"` and
`passwordeval "pass show …"` are in the shipped templates.

**XOAUTH2 works everywhere except `mbsync`.** `aerc` speaks it itself and `msmtp` has it built in,
so an account whose provider has withdrawn application passwords can be read and sent from here —
`pizauth` mints and refreshes the token, and a `*-cred-cmd` hands it over. What that account cannot
have is a local Maildir: `mbsync` reaches XOAUTH2 only through `cyrus-sasl`, which this image does
not carry, so nothing mirrors it offline and `notmuch` has nothing to index.

## Passwords and one-time codes

**`pass` is the store**: a file per entry in a git repository with `gpg` over each. No database and
no format to migrate — if `pass` itself vanished, `gpg -d` still reads every entry.

**A site that wants a six-digit code is `pass otp`.** Save the `otpauth://` URI the site's QR code
encodes and ask for a code when you need one:

```sh
pass otp insert site/example        # paste the otpauth:// URI
pass otp site/example               # the code for right now
pass otp -c site/example            # and onto the clipboard
```

Nothing has to be enabled: `pass` reads its system extensions with no opt-in, so the command exists
as soon as the package is installed. It generates the code with `oathtool`, which is also usable on
its own — `oathtool --totp -b <secret>`.

**There is no one-time password for logging in to this machine.** `oath-toolkit`'s PAM module is
deliberately not built: a wrong line in a PAM stack is a machine nobody can log into, including the
person trying to fix it. The codes here are for other people's websites.

## Calendar and contacts

**One directory of files, the same shape as the mail.** `~/.local/share/calendars` holds calendars,
`~/.local/share/contacts` holds address books, and each is a *vdir*: a directory per collection, one
`.ics` or `.vcf` file per item. That is greppable, diffable, and backed up by copying it — and a
file with two events in it is not a vdir, which is why `khal import` exists rather than a text
editor.

| Program | Does |
|---|---|
| `khal` | Prints what is on. `khal list today 7d`, `khal import invite.ics` |
| `ikhal` | The same calendar to move around in, full screen. This is the *Calendar* menu row |
| `khard` | The address book. `khard list`, `khard show`, `khard new` |
| `vdirsyncer` | Makes a server's collection and the local vdir equal — the same job `mbsync` does for mail. Nothing is configured, so `vdirsyncer sync` does nothing and exits 0 |

**Both are configured and both are empty**, which is not the same as unconfigured: `khal` with no
`[calendars]` section and `khard` with no address book both refuse to start, so the shipped files
name a store that has nothing in it. Put a calendar in by making a directory under
`~/.local/share/calendars` and dropping `.ics` files in it — nothing needs to be registered.

**The panel's calendar reads the same store.** A day with something on it is marked, and today's
events are listed under the month; that popup asks `khal` when it opens and when you change month,
so it costs nothing while it is on screen.

**Syncing with a server is `vdirsyncer`**, and it is set up in
`~/.config/vdirsyncer/config`: a *pair* is two storages plus the rule for reconciling them. There is
no safe default for that rule — `a wins` silently discards the server's edit and `b wins` discards
yours — so the shipped example makes you choose. Run `vdirsyncer discover` once, then
`vdirsyncer sync` whenever you want; keep the password out of the file with
`password.fetch = ["command", "pass", "show", "…"]`.

**`khard list` exits 1 on an empty address book.** It prints `Found no contacts` and means it —
that is khard's answer for "nothing matched", not a failure, and anything scripting it has to read
a non-zero exit as an empty result.

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

The console user can open hardware because of two halves that are both required: membership of a
group, and a rule granting that group the device class. The group alone grants nothing; the rule
alone has no group to grant to. Most of these are `dialout`; the two display ones are `video`.

| Rules file | Devices | Group |
|---|---|---|
| `70-kdos-serial.rules` | USB serial adapters — FTDI, CP210x, CH341, CDC-ACM | `dialout` |
| `70-kdos-debug.rules` | In-circuit debuggers and programmers | `dialout` |
| `70-kdos-sdr.rules` | Software-defined radio front ends | `dialout` |
| `70-kdos-usbtmc.rules` | USB Test & Measurement: scopes, meters, function generators | `dialout` |
| `70-kdos-camera.rules` | PTP/MTP cameras, for gphoto2 | `dialout` |
| `70-kdos-scanner.rules` | Flatbed and sheet-fed scanners, for SANE | `dialout` |
| `70-kdos-i2c.rules` | The DDC/CI line of a display controller, for `ddcutil` | `video` |
| `70-kdos-backlight.rules` | The panel's brightness, for `kdos-osd` | `video` |

All grant `MODE="0660"` rather than world-readable: these are devices other users on a multi-user
machine have no business reading.

**The i2c rule is scoped, and the scoping is the point.** `/dev/i2c-*` covers the graphics cards'
DDC lines and the chipset SMBus alike, and every DIMM's SPD EEPROM hangs off the SMBus — a stray
write there is a machine that will not boot. The rule matches only adapters whose PCI parent is a
display controller (`ATTRS{class}=="0x03*"`), so the SMBus is never in it. `ddcutil` ships an
unscoped rule of its own and the recipe deletes it.

**The backlight rule grants no group, and cannot.** A backlight is a class device with no node in
`/dev`, and udev's `GROUP=`/`MODE=` apply to a node — worse, a rule carrying either is discarded
whole for such a device, taking its `RUN+=` with it. So that file runs `chgrp` and `chmod` on the
`brightness` attribute instead, on the `add` event that `01_udev.sh`'s coldplug replays at boot.

**`/dev/i2c-*` needs a module nothing autoloads.** `i2c-dev` declares no modalias, so udev can
never name it; `/etc/modules-load.d/kdos-i2c.conf` is what loads it, and without that the rule has
nothing to grant.

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
