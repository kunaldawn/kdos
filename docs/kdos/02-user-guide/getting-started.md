# Getting started

This page is for anyone who wants to try KDOS: it takes you from a clone of the repository to a
running KDOS desktop. You build an image, write it to a USB stick or boot it in a virtual machine,
log in, and then decide whether to keep the session in RAM or install it to a disk.

Read it top to bottom the first time. When you are done you will have a bootable image, know how to
log in and get back out, and know where each next step is documented. The installer itself is
covered page by page in [Installation](installation.md).

## You build the image

No v0.2 image is published. The one release on
[github.com/kunaldawn/kdos](https://github.com/kunaldawn/kdos/releases) carries the v0.1 ISO, an
earlier line this book does not describe. A v0.2 system is one you compile from this repository.
That is the intended path: a distribution whose point is that it is built from source does not
begin with somebody else's binary.

Plan for the cost before you start.

| | |
|---|---|
| Wall time, first build | Hours — plan for most of a day. The whole system is compiled, including GCC several times over and the kernel |
| Disk | On the order of 100 GB under `build/`, most of it the phase snapshots that make later builds short. Plus about 8.9 GB of upstream source archives that `make fetch` downloads |
| Network | Needed to clone, to run `make fetch`, and once for the first `make build`, which builds its container image from Alpine Linux packages. The compile itself runs with the network switched off |
| Installed on your machine | Nothing but Docker. Every compiler runs inside a container |

Later builds are much shorter. Each phase is snapshotted, and most changes need only a narrow
rebuild — see [Developing](../05-developer/developing.md) for the targeted loops.

## What you need

- **A Linux host with Docker and plenty of free disk.** `make build` runs `docker` directly. The
  build image carries the whole toolchain, so nothing else is installed on your machine.
- **Optionally, a host C compiler (`cc`).** `make fetch` compiles a small recipe reader on the host.
  With a compiler it does that and runs quickly; without one it hands the whole fetch to a
  container of its own (Docker or Podman, whichever is on your `PATH`). `make fetch-check` has no
  container fallback and needs the compiler. See
  [What a development machine needs](../05-developer/developing.md#what-a-development-machine-needs).
- **To run the result in a virtual machine:** `qemu-system-x86_64`, OVMF firmware at
  `/usr/share/ovmf/OVMF.fd`, and `/dev/kvm`. Without `/dev/kvm` QEMU still starts, very slowly.

## Build an image

```sh
git clone <this repository> kdos
cd kdos
make fetch
make build
```

### Fetching the sources

A clone carries the recipes, not the upstream source archives. `make fetch` downloads every file a
recipe pins with a `sha256 =` line: 1,254 file names across 1,012 ports, which are 1,232 distinct
files and about 8.9 GB (8.3 GiB), because several ports share one archive — the LLVM tarball alone
serves eight of them.

For each file it stops at the first copy that verifies against the recipe's hash, looking in this
order:

1. the port's own directory;
2. the download cache, `ports/.srccache/`;
3. the KDOS source archive — GitHub release assets on `kunaldawn/kdos-sources`, each named by the
   file's `sha256`;
4. the upstream URL in the recipe's `source =` line;
5. for a Rust, Go, Node, Python or Haskell port only, regenerating the port's vendored dependency
   bundle.

Nothing that fails its hash is kept. Downloads land in the cache and are hard-linked into each
port's directory, so switching branches fetches nothing twice. Set `KDOS_SRCCACHE` to one shared
path and a second checkout on the same machine fetches nothing either. When the archive cannot be
reached, every file comes from upstream instead.

A vendor bundle that has to be regenerated is always built inside a Docker or Podman container
(`kdos-fetch`), even on a host with a compiler, because it needs the exact Cargo, Go, npm, pip and
Cabal versions the build will use. Set `KDOS_FETCH_HOST=1` to generate it with your host's own
toolchains instead. A clone whose archive is complete never needs that container. See
[Where sources come from](../05-developer/developing.md#where-sources-come-from).

`make fetch` is safe to re-run: a file already
present and verified is not downloaded again. `make fetch-check` lists, offline, anything still
missing or corrupt, and exits 1 if it finds any. A build started without the fetch fails on the
first port whose archive is absent.

### Building

`make build` builds the container image (`os-dev`), then runs the build orchestrator inside it with
`--network none`. Building that image pulls Alpine Linux and installs its packages, so the first
`make build` on a machine needs the network too; once the image exists, Docker reuses it and the
whole build runs offline. The result is:

```
build/iso-build/kdos.iso
```

`make build` refuses to start while that ISO is open in another program — a running `make run`,
for example — because the last step rewrites it. Stop the virtual machine first, or set
`ALLOW_ISO_IN_USE=1` to override.

A few variables change what the build produces:

| Variable | Effect |
|---|---|
| `BUILD_ARGS="…"` | Flags for the orchestrator, such as `--fresh`, `--continue-from <phase>` or `--rebuild <port>`. See [Build system](../05-developer/build-system.md) |
| `KDOS_ISO_SOURCES=1` | Copies the ports tree, `src/` and `script/` onto the ISO, so the stick can rebuild itself. Roughly doubles the image |
| `KDOS_PACK_KDOS=1` | Also packs the finished system as a base pack named `kdos`, written to `build/kdos-base` |

The ISO carries no graphical applications, and no build step puts any on it. It ships the
*catalogue* — a description of each application as a chain of Debian packages — and the machine
that wants one builds it with Podman. See [Applications](applications.md) for using the catalogue,
and [Packs and boxes](../03-architecture/packs-and-boxes.md) for the format.

When a build fails, read the failing step's log under `build/logs/` and check
[Build troubleshooting](../05-developer/build-troubleshooting.md), which lists the recurring
failures by symptom.

## Try it in a virtual machine first

```sh
make run        # boot the ISO; creates build/kdos.qcow2 as a blank 20 GB disk if missing
make rundisk    # boot that disk instead, once you have installed to it
```

Both give the guest 4 GB of memory, all your CPU threads, UEFI firmware, sound and a network. The
screen comes up at 1920x1080; set `KDOS_RES` to change it, for example
`make run KDOS_RES=2560x1440`.

`make run` attaches a plain virtio-vga adapter, on which the compositor falls back to software
rendering. The desktop works, but the phosphor shader — the CRT-style post-process — does not run,
because the compositor applies it only on a GLES2 renderer. `make run-hw` (and `make rundisk-hw`)
boots the same image through a containerised QEMU with virgl on your GPU, which is the
configuration where the shader is on. It needs Docker with the NVIDIA Container Toolkit. See
[Theming](theming.md#the-phosphor-pass).

## Write the medium

The ISO is a hybrid image. Write it to a USB stick as a raw byte stream, replacing `/dev/sdX` with
your stick — everything on that device is lost:

```sh
sudo dd if=build/iso-build/kdos.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

That one command covers all four ways the image boots: BIOS and UEFI, from an optical drive and
from a stick. The image carries both El Torito boot records for optical drives, and a partition
table with an EFI System Partition plus BIOS boot code in its first sector for a stick. There is no
separate "make it bootable" step.

Once you have one working stick, a running KDOS system can copy itself to another without a host
computer:

```sh
sudo kdos clone            # list the devices it may write to
sudo kdos clone /dev/sdb   # copy this boot medium onto /dev/sdb
```

`kdos clone` takes the image's length from the image itself rather than from the device, refuses
the medium it booted from along with anything mounted or named in `fstab`, and verifies the copy by
reading it back with the page cache dropped. See
[Administration](administration.md#copying-and-rebuilding-the-medium).

## Boot

KDOS boots on BIOS and UEFI alike through [Limine](https://limine-bootloader.org/), so the menu
looks the same either way. Select the stick in your firmware's boot menu and the KDOS menu appears.

The menu counts down for ten seconds and then boots the first entry, so an ordinary boot needs no
keystroke. Press any key during the countdown to stop it and choose something else:

| Entry | Boots |
|---|---|
| KDOS Live | The normal session |
| KDOS Live (clean session) | The same, ignoring the persistence store for one boot |
| KDOS Live (verbose) | Every kernel message on the console, at `loglevel=7` |
| Memory Test (memtest86+) | memtest86+ instead of the kernel. UEFI only — the entry is hidden on a BIOS boot |

### Reading the splash

What you see during boot is the KDOS splash, a CRT power-on drawn directly to the framebuffer that
names each stage as it starts. If a boot stops, the last stage named tells you where:

| Stage | If it stops here |
|---|---|
| `DEVICE MANAGER` | udev did not come up |
| `RAID` | Software RAID assembly. Shown only when `mdadm` is in the initramfs; finding no array is normal |
| `FILESYSTEM MODULES` | Loads the loop, ISO, squashfs and overlay modules. This stage never stops the boot itself: a module it could not load shows up as a failure at `SYSTEM IMAGE` or `OVERLAY ROOT` |
| `BOOT SLOT` | An installed system could not find or mount the partition holding its boot state within ten seconds |
| `VOLUME GROUPS` | An LVM volume group would not activate. Not fatal by itself; the root lookup says whether the root was in it |
| `UNLOCKING` | The encrypted root's passphrase was refused three times |
| `ROOT DEVICE` | An installed system's root partition did not appear within ten seconds |
| `BOOT MEDIA` | No device carrying the KDOS image appeared within ten seconds |
| `SYSTEM IMAGE` | The system image on the medium would not mount |
| `MOUNTING ROOT`, `OVERLAY ROOT` | The root filesystem or the live overlay would not mount |
| `SWITCHING ROOT` | The handover to the real root failed |
| `MOUNTING FILESYSTEMS` onward | You are in the service scripts, and the failing service names itself |

When the initramfs cannot continue, it leaves you at a shell rather than rebooting, so you can look
around. The progress bar stops one segment short of full until the splash is dismissed, so a boot
never shows 100% before it has finished. [Boot and init](../03-architecture/boot-and-init.md)
describes what happens at each stage.

## Log in

The image ships one human account:

| | |
|---|---|
| User name | `kdos` |
| Password | `kdos` |
| Groups | `wheel` (which grants `sudo`), plus `tty`, `dialout`, `audio`, `video`, `cdrom`, `render`, `input`, `kvm`, `users`, `seat` and `lpadmin` |

On the live image the root password is also `kdos`. The [installer](installation.md) asks for your
own user name and password, and locks root by default.

The terminals are laid out like this:

| Terminal | What it gives you |
|---|---|
| `tty1` | The desktop. Logs in as `kdos` automatically on the live medium; asks for a password on an installed system |
| `tty2` | An ordinary login prompt — the recovery console |
| `ttyS0` | A login shell on the serial line, after one keypress |

At a text console, switch with `Alt+F1` and `Alt+F2`. From the desktop, use `Ctrl+Alt+F2` to reach
the recovery console and `Alt+F1` to come back.

On `tty1` the desktop starts by itself. `kdos-getty` loads the console font and colours, then hands
over to `kdos-login`, which logs in the account named by `autologin` in `/etc/kdos/login.conf`. The
login shell's profile then starts the session. If the session fails to start you are left at a
shell on `tty1` rather than at a black screen, so the machine can still be fixed. `tty2` is a plain
login whatever `tty1` does.

There is no display manager and no graphical greeter. To get a password prompt on `tty1` instead of
automatic login, comment out the `autologin` line in `/etc/kdos/login.conf`.

Behind the login is the KDOS banner, drawn one raster line at a time. Any keypress skips the rest of
the animation.

The text console uses the KDOS console font at 16x32 pixels. It has only 512 glyphs, which is why
parts of this system restrict themselves to a small set of symbols — see
[the design language](../03-architecture/design-language.md).

## Keep what you change

A live session's changes are held in RAM and are gone when the machine powers off. To keep them,
create a *persistence store*: an ext4 filesystem labelled `KDOS_PERSIST`, which the boot process
uses to hold your changes instead of RAM.

```sh
sudo kdos persist create            # on the stick you booted from
sudo kdos persist create /dev/sdb   # on another device
kdos persist                        # does a store exist, and is this session using it?
```

With no device named, the store goes into the free space *after* the image on the stick you booted
from; nothing already there is moved or rewritten. Add `--yes` to skip the confirmation.

The store is found by its label rather than by a device name, so it can equally live on a second
stick or an internal disk, and it keeps working when USB devices come up in a different order.

Two things to know. A new store is used from the *next* boot, not the one that created it. And if
something you change ever stops the desktop coming up, the **KDOS Live (clean session)** menu entry
ignores the store for one boot without deleting it.

## Install to disk

A live session is fine for trying the desktop, but two things need a real disk: applications from
the catalogue, which cannot keep their files on a live session's in-memory filesystem, and anything
you want to keep without a persistence store.

```sh
sudo kinstall
```

The installer partitions a disk, copies the system, writes the bootloader, creates your account and
offers groups of applications. It is described in full in [Installation](installation.md).

## First things to try

Open a terminal with `Super+Return` and run:

```sh
kdos help                # every command on the system, grouped by what it answers
kdos doctor              # checks the things that commonly break on this system
kdos status              # what this machine is and what it is running
kdos app list --all      # the application catalogue on your medium
kdos theme amber         # retint the entire session, live
```

`kdos theme amber` is the quickest way to see what this desktop is. The panel, the desktop icons,
the window frames, the wallpaper and the phosphor shader all change colour at once, with nothing
restarted. `kdos theme bone` puts back the default. [Theming](theming.md) lists all eight accents.

![The keybinding card, which opens on first login. `Super+F1` brings it back](../../screenshots/keys.png)

The keybinding card above opens by itself on your first login. `Super+F1` opens it again at any
time.

If you chose applications during an install on a machine that had no network at the time, your
first session shows a notification saying they are ready to install, rather than starting a long
build unannounced. Run `kdos app install --pending` when you are ready, or open the store.

## Where to go next

- Put it on a disk: [Installation](installation.md)
- Learn the desktop: [The desktop](desktop.md)
- Get applications: [Applications](applications.md)
- Run the machine: [Administration](administration.md)
- Change something: [Developing](../05-developer/developing.md)

## See also

- [Installation](installation.md) — the installer, page by page
- [The desktop](desktop.md) — panel, menus, windows, keybindings
- [Administration](administration.md) — services, networking, hardware, updates
- [Developing](../05-developer/developing.md) — build targets and the fast iteration loops
- [Boot and init](../03-architecture/boot-and-init.md) — what each boot stage does
