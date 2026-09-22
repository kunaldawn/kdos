# Getting started

This page takes you from a clone of the repository to a running KDOS desktop. You build an image,
write it to a USB stick or boot it in a virtual machine, log in, and decide whether to keep the
session in RAM or install it to a disk. [Installation](installation.md) covers the installer
itself, page by page.

## There is no download

KDOS is not published as a prebuilt ISO. You compile the image from this repository, and that is
the intended path rather than a temporary state: a distribution whose whole point is that it is
built from source does not begin with somebody else's binary.

Plan for the cost before you start.

| | |
|---|---|
| Wall time, first build | Hours. The entire host is compiled, including GCC twice and the kernel |
| Disk | Tens of gigabytes under `build/`, plus 7.7 GB of upstream archives in the clone |
| Network | Needed once, to clone. The build itself runs with the network switched off |
| Installed on your machine | Nothing. Every compiler runs inside a container |

Later builds are much shorter. Phases are snapshotted, and most changes need only a narrow
rebuild — see [Developing](../05-developer/developing.md) for the targeted loops.

## What you need

A Linux host with Docker (or Podman) and enough free disk. Nothing else is installed on the host:
the build image carries the toolchain, and the two host-side helpers that need a compiler
(`ports/fetch` and the pack bake) re-execute themselves inside containers of their own.

To run the result in a virtual machine, you also want `qemu-system-x86_64`, OVMF firmware at
`/usr/share/ovmf/OVMF.fd`, and `/dev/kvm`.

## Build an image

Install Git LFS *before* you clone:

```sh
git lfs install
git clone <this repository> kdos
cd kdos
make build
```

There is no fetch step. The upstream tarballs live in the tree through Git LFS — 1022 archives
across 853 ports — and the `sha256 =` line in each recipe sits beside the bytes it verifies.

If `git lfs install` has not run before the clone, the working tree holds 129-byte pointer files
where the archives should be. The first port to unpack one fails on a corrupt archive rather than
on anything that names the cause. `git lfs pull` repairs such a clone.

`make build` builds the container image, then runs the orchestrator inside it with
`--network none`. The result is:

```
build/iso-build/kdos.iso
```

The ISO carries no graphical applications, and no build step would put any on it. The medium
ships the catalogue — a description of each application as a chain of Debian packages — and the
machine that wants one builds it with Podman. See [Applications](applications.md) for how to use
the catalogue, and [Packs and boxes](../03-architecture/packs-and-boxes.md) for the format.

When a build fails, read the failing step's log under `build/logs/` and check
[Build troubleshooting](../05-developer/build-troubleshooting.md), which catalogues the recurring
failures by symptom.

## Try it in a virtual machine first

```sh
make run        # boot the ISO; creates build/kdos.qcow2 as a blank 20 GB disk
make rundisk    # boot that disk instead, once you have installed to it
```

`make run` attaches a plain virtio-vga adapter, on which wlroots falls back to its software
renderer. The desktop works, but the phosphor shader does not run, because the compositor declines
a fullscreen post-process on anything that is not GLES2. `make run-hw` boots the same image
through a containerised QEMU with virgl, which is the configuration where the shader is actually
on. See [Theming](theming.md#the-phosphor-pass).

## Write the medium

The ISO is a hybrid image. Write it to a USB stick as a raw byte stream:

```sh
sudo dd if=build/iso-build/kdos.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

That one command covers all four ways the image boots — BIOS and UEFI, from an optical drive and
from a stick. The two El Torito records serve the optical cases; the partition table serves the
written ones, because firmware reading a stick never looks in a boot catalogue. The image carries
an EFI System Partition for UEFI and boot code in its first sector for BIOS, so there is nothing
to add afterwards and no separate "make it bootable" step.

The ISO9660 filesystem starts at the first sector, which also makes the stick the boot medium the
initramfs looks for: it mounts each whole-disk node as iso9660 and takes the first one carrying
`system.sfs`. On a written stick that is the device itself rather than a partition on it.

Once you have one working stick, a running KDOS system can copy itself to another without a host
computer:

```sh
sudo kdos clone /dev/sdb
```

`kdos clone` takes the image's length from the image's own self-description rather than from the
device, refuses the medium it booted from along with anything mounted or named in `fstab`, and
verifies the copy by re-reading it with the page cache dropped. See
[Administration](administration.md#copying-and-rebuilding-the-medium).

## Boot

KDOS boots on BIOS and UEFI alike through [Limine](https://limine-bootloader.org/) — one
bootloader with one menu, so the machine looks the same either way. Select the stick in your
firmware's boot menu and the KDOS menu appears.

The menu counts down for ten seconds and then boots the first entry, so an ordinary boot needs no
keystroke. Press any key during the countdown to stop it and choose something else:

| Entry | Boots |
|---|---|
| KDOS Live | The normal session |
| KDOS Live (clean session) | The same, ignoring the persistence store for one boot |
| KDOS Live (verbose) | Every kernel message on the console, at `loglevel=7` |
| Memory Test (memtest86+) | memtest86+ instead of the kernel. UEFI only — the payload is an EFI binary |

### Reading the splash

What you see during boot is the KDOS splash, which draws a CRT power-on directly to the
framebuffer and names each stage as it completes. If a boot stops, the last stage named tells you
where:

| Stage | If it stops here |
|---|---|
| `DEVICE MANAGER` | udev did not come up |
| `FILESYSTEM MODULES` | The initramfs lacks the module for your root filesystem |
| `BOOT SLOT` | The A/B state file is unreadable |
| `UNLOCKING` | The encrypted root passphrase was refused three times |
| `ROOT DEVICE`, `BOOT MEDIA` | The root device or the boot medium did not appear within ten seconds |
| `MOUNTING ROOT`, `OVERLAY ROOT` | The root filesystem or the live overlay would not mount |
| `SWITCHING ROOT` | The handover to the real root failed |
| `MOUNTING FILESYSTEMS` onward | You are in `rcS`, and the failing service names itself |

The progress bar deliberately stops one segment short of full until the splash is dismissed, so a
boot never shows 100% before it has finished. [Boot and init](../03-architecture/boot-and-init.md)
describes what happens at each stage.

## Log in

The image ships one human account.

| | |
|---|---|
| User name | `kdos` |
| Password | `kdos` |
| Groups | `wheel`, plus the hardware groups |

The root account has the same password on the live image. Change both during
[installation](installation.md), which asks for a user name and password and offers to lock root.

Terminals are laid out like this:

| Terminal | What it gives you |
|---|---|
| `tty1` | The desktop. Autologin as `kdos` on the live medium; a password prompt on an installed system that was set up that way |
| `tty2` | An ordinary login prompt — the recovery console |
| `ttyS0` | A serial login, used by the test rig |

Switch between them with `Alt+F1` and `Alt+F2`.

On `tty1` the desktop starts by itself: `kdos-getty` loads the console font and hands over to
`kdos-login`, which reads the account named by `autologin` in `/etc/kdos/login.conf` and logs it
in; the shell's profile then starts the session. If the session fails to start you are left at a
shell rather than at a black screen, which is the point — a machine you can still fix. `tty2` is a
plain login whatever `tty1` does.

There is no display manager and no greeter. A display manager is a privileged process whose only
job is to run the thing the profile is about to run anyway, and on a single-user workstation it
buys nothing. Comment out `autologin` and the ordinary password prompt appears instead.

Behind the login is the KDOS banner, drawn one raster line at a time with a bright beam leading
the fill. Any keypress skips the rest of the animation.

The virtual terminal runs the KDOS console font at 16x32 pixels. It carries 512 glyphs, which is
why parts of this system restrict themselves to a small glyph set — see
[the design language](../03-architecture/design-language.md).

## Keep what you change

A live session's writes land in RAM and are gone when the machine powers off. To keep them, create
a persistence store: one ext4 filesystem labelled `KDOS_PERSIST`, which the initramfs uses as the
overlay's upper layer in place of the tmpfs.

```sh
sudo kdos persist create
```

With no device named, `kdos persist create` uses the free space *after* the image on the medium
you booted from, and nothing already there is moved or rewritten. Run `kdos persist` with no
arguments to report whether a store exists and whether this session is writing to it.

The store is found by its label rather than by a path, so it can equally live on a second stick or
on an internal disk: `kdos persist create /dev/sdb` puts it there. Only the label matters, which
is what keeps it working when USB devices enumerate in a different order.

Two consequences follow from it being an overlay upper layer. A store is used from the *next*
boot, not the one that created it. And if something you change ever stops the desktop coming up,
the **KDOS Live (clean session)** menu entry ignores the store for one boot without deleting it.

## Install to disk

A live session is fine for trying the desktop, but two things need a real filesystem: containerised
applications, whose overlay upper layer has nowhere to go on a live `$HOME`, and anything you want
to survive without a persistence store.

```sh
sudo kinstall
```

The installer partitions a disk, copies the system image, writes the bootloader, creates your
account and offers optional application groups. It is described in full in
[Installation](installation.md).

## First things to try

Open a terminal with `Super+Return` and run:

```sh
kdos help                # every command on the system, grouped by what it answers
kdos doctor              # checks the things that actually break on this distribution
kdos status              # what this machine is and what it is running
kdos app list --all      # the application catalogue on your medium
kdos theme amber         # retint the entire session, live
```

`kdos theme amber` is the quickest way to see what this desktop is. The panel, the desktop icons,
the window frames, the wallpaper and the phosphor shader all change colour on one signal, with
nothing restarted. `kdos theme bone` puts back the default.

![The keybinding card, which opens on first login. `Super+F1` brings it back](../../screenshots/keys.png)

If you selected applications during an install on a machine that had no network at the time, the
first session says so and offers them rather than starting anything. Building an application means
Podman and apt, which is not a thing to have happen unannounced on a machine you have just booted.
Run `kdos app install --pending` when you are ready; the store offers the same set.

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
