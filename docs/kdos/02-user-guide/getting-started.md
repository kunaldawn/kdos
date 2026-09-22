# Getting started

Getting from a clone of this repository to a running KDOS desktop. This page covers building an
image, writing it to a medium, the boot, the first login, and starting a session. Read it before
[Installation](installation.md), which puts the result on a disk.

## There is no download

KDOS is not published as an ISO. You build the image yourself, from this repository, and that is
the intended path rather than a temporary state — a distribution whose point is that it is
compiled from source does not begin with a binary.

Budget for it honestly:

| | |
|---|---|
| Wall time, first build | Hours. The whole host is compiled, including gcc twice and the kernel |
| Disk | Tens of gigabytes for `build/`, plus 7.1 GB of upstream tarballs in the clone |
| Network | Needed once, to clone. The build itself runs with no network |
| Installed on your machine | Nothing. Everything happens inside a container |

Subsequent builds are far shorter, because phases are snapshotted and a change usually needs only
a narrow rebuild — see [Developing](../05-developer/developing.md).

## What you need

A Linux host with **docker** (or podman) and enough disk. Nothing else is installed on the host:
the build image carries the compilers, and the two host-side helpers that need a toolchain
(`ports/fetch` and the pack bake) re-execute themselves inside containers of their own.

To run the result in a virtual machine you also want `qemu-system-x86_64`, OVMF firmware at
`/usr/share/ovmf/OVMF.fd`, and `/dev/kvm`.

## Build an image

```sh
git lfs install       # BEFORE the clone, not after
git clone <this repository> kdos
cd kdos
make build            # compile everything (no network at all)
```

There is no fetch step: the upstream tarballs are **in the tree**, through Git LFS, and the
`sha256 =` in each recipe sits beside the bytes it verifies.

**`git lfs install` has to have run before the clone.** Without it the working tree holds 129-byte
pointer files where the archives should be, and the first port to unpack one fails on a corrupt
archive rather than on anything that names the cause. `git lfs pull` repairs a clone made without
it.

`make build` builds the container image, then runs the orchestrator inside it with
`--network none`. The result is:

```
build/iso-build/kdos.iso
```

**The ISO carries no applications**, and there is no step that would put any on it. The medium
ships the catalogue — what each application is, as a chain of Debian packages — and the machine
that wants one builds it with podman. See [Applications](applications.md) for using the store and
[Packs and boxes](../03-architecture/packs-and-boxes.md) for how one is built.

**If the build fails**, read the failing step's log under `build/logs/` and check
[Build troubleshooting](../05-developer/build-troubleshooting.md), which catalogues the recurring
failures by symptom.

## Try it in a virtual machine first

```sh
make run        # boot the ISO, creating build/kdos.qcow2 as a blank disk
make rundisk    # boot that disk instead, after you have installed to it
```

`make run` uses plain virtio-vga, where wlroots falls back to its software renderer — so the
desktop works but **the CRT pass does not run**, since it declines anything that is not GLES2.
`make run-hw` boots the same image through a containerised QEMU with virgl, which is the
configuration where the phosphor shader is actually on. See [Theming](theming.md).

## Write the medium

The ISO is a hybrid image: write it to a USB stick as a raw byte stream. It boots four ways and
`dd` carries all of them — BIOS and UEFI, from an optical drive and from a stick. The two El
Torito records serve the optical cases; the partition table serves the written ones, because a
firmware reading a stick never looks in a boot catalogue. The image carries an EFI System
Partition for UEFI and boot code in its first sector for BIOS, so there is nothing to add
afterwards and no separate "make it bootable" step.

The ISO9660 filesystem starts at the first sector, so the stick is also the boot medium the
initramfs looks for: it mounts each whole-disk node as iso9660 and takes the first one carrying
`system.sfs`, which on a written stick is the device itself rather than a partition on it.

```sh
sudo dd if=build/iso-build/kdos.iso of=/dev/sdX bs=4M status=progress conv=fsync
```

Once you have one working stick, the running system can copy itself to another without a host
computer at all:

```sh
sudo kdos clone /dev/sdb
```

`kdos clone` takes the image's length from the image's own self-description rather than from the
device, refuses the medium it booted from and anything mounted or named in `fstab`, and verifies
the copy by re-reading it with the page cache dropped. See
[Administration](administration.md#copying-and-rebuilding-the-medium).

## Keep what you change

A live session's writes land in RAM and go when the machine is powered off. To keep them, make a
**persistence store** — one ext4 filesystem labelled `KDOS_PERSIST`, which the initramfs uses as
the overlay's upper layer in place of the tmpfs:

```sh
sudo kdos persist create
```

With no device named it uses the free space **after** the image on the medium you booted from, and
nothing already there is moved or rewritten. `kdos persist` on its own reports whether a store
exists and whether this session is writing to it.

The store is found by its label and not by a path, so it can equally live on a second stick or on
an internal disk — `kdos persist create /dev/sdb` puts it there. Only the label matters, which is
what keeps it working when USB enumerates in a different order.

Two things follow from it being an overlay upper. A store is used from the **next** boot, not the
one that created it. And if a change ever stops the desktop coming up, the **KDOS Live (clean
session)** entry in the boot menu ignores the store for one boot without deleting it.

## Boot

KDOS boots on **BIOS and UEFI alike**, through [Limine](https://limine-bootloader.org/) — one
bootloader with one menu, so the machine looks the same either way. Select the stick in your
firmware's boot menu and the menu appears, then the kernel starts. **It counts down for ten
seconds**, so the normal entry boots without you doing anything; press any key during that time
to stop the countdown and pick the verbose entry, the clean session, or the memory test, which
are reachable only from the menu. The memory test is a UEFI payload and is not offered on a BIOS
boot.

The screen you see during boot is [the splash](../03-architecture/boot-and-init.md), which draws
a CRT power-on directly to the framebuffer and names each stage as it completes. The stages tell
you where a failed boot stopped:

| Stage | If it stops here |
|---|---|
| `DEVICE MANAGER` | udev did not come up |
| `FILESYSTEM MODULES` | the initramfs lacks the module for your root filesystem |
| `BOOT SLOT` | the A/B state file is unreadable |
| `UNLOCKING` | the encrypted root passphrase was refused three times |
| `ROOT DEVICE` / `BOOT MEDIA` | the root or the medium did not appear within ten seconds |
| `MOUNTING ROOT` / `OVERLAY ROOT` | the root filesystem or the live overlay would not mount |
| `SWITCHING ROOT` | the handover to the real root failed |
| `MOUNTING FILESYSTEMS` onward | you are in `rcS`, and the failing service names itself |

The progress bar deliberately stops one segment short of full until the splash is dismissed, so a
boot never shows 100% before it has finished.

## First login

The system ships **one human account**:

| | |
|---|---|
| User | `kdos` |
| Password | `kdos` |
| Groups | `wheel`, plus the hardware groups |

The terminals are laid out like this:

| Terminal | What it gives you |
|---|---|
| `tty1` | The desktop. Autologin as `kdos` on the live medium; a login on an installed system |
| `tty2` | An ordinary login prompt — **the recovery console** |
| `ttyS0` | A serial login, used by the test rig |

Switch between them with `Alt+F1` and `Alt+F2`.

On `tty1` the **desktop** comes up on its own: `kdos-login` autologins the account `login.conf`
names and the shell's profile starts it. If it does not start, you are left at a shell rather than
at nothing, which is the point — a session that fails is a machine you can still fix, and `tty2` is
a plain login whatever `tty1` does.

Behind it is the login banner, drawn one raster line at a time with a bright beam leading the fill.
Any keypress skips the rest of the animation.

The virtual terminal is running the KDOS VT font at 16x32 — 512 glyphs, loaded by
[`kdos-getty`](../03-architecture/boot-and-init.md) rather than by an init script. That font is
why parts of this system deliberately restrict themselves to a small glyph set: see
[the design language](../03-architecture/design-language.md).

## The session

There is **no display manager and no greeter**: a display manager is a privileged process whose
only job is to run the thing the profile is about to run anyway, and on a single-user workstation
it buys nothing. `login.conf` names the account tty1 logs in; comment the key out and the ordinary
password prompt appears instead.

If the session exits you are returned to the tty, with its log at
`$XDG_RUNTIME_DIR/kdos-comp.log`. The full sequence is in
[The session](../03-architecture/session.md).

## Try these first

Once the desktop is up, open a terminal with `Super+Return` and run:

```sh
kdos help                # every command on the system, grouped by what it answers
kdos doctor              # checks the things that actually break on this distribution
kdos status              # what this machine is and what it is running
kdos app list            # the application catalogue on your medium
kdos theme amber         # retint the entire session, live
```

`kdos theme amber` is the quickest way to see what this desktop is: the panel, the desktop icons,
the window frames, the wallpaper and the CRT shader all change colour in one signal, without
restarting anything. Switch back with `kdos theme phosphor`.

![The keybinding card, which opens on first login. `Super+F1` brings it back](../../screenshots/keys.png)

**If you ticked applications during the install and the machine had no network at the time**, the
first session says so and offers them rather than starting anything: building an application is
podman and apt, and that is not a thing to have happen unannounced on a machine you have just
booted. `kdos app install --pending` runs it when you are ready, and the store lists the same set.

## Where to go next

- To put it on a disk: [Installation](installation.md)
- To learn the desktop: [The desktop](desktop.md)
- To get applications: [Applications](applications.md)
- To change something: [Developing](../05-developer/developing.md)

## See also

- [Installation](installation.md) — the installer, page by page
- [The desktop](desktop.md) — panel, menus, windows, keybindings
- [Administration](administration.md) — services, networking, hardware, updates
- [Developing](../05-developer/developing.md) — build targets and the fast iteration loops
- [Boot and init](../03-architecture/boot-and-init.md) — what each boot stage actually does
