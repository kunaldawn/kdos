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
| Disk | Tens of gigabytes for `build/`, plus the fetched sources and packs |
| Network | Needed once, for `make bootstrap`. The build itself runs with no network |
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
git clone <this repository> kdos
cd kdos
make bootstrap        # fetch upstream sources (needs network, once)
make build            # compile everything (no network at all)
```

`make bootstrap` downloads the upstream archive from the release assets and extracts it into
`ports/core`. Only the packfiles holding sources you are missing are fetched; each is checked
against a hash before it is unpacked, and every archive inside is verified again against the
`sha256 =` in its own recipe.

`make build` builds the container image, then runs the orchestrator inside it with
`--network none`. The result is:

```
build/iso-build/kdos.iso
```

**The application packs are separate.** They are large and are baked from Debian rather than
compiled here, so they have their own step:

```sh
make bootstrap-packs   # download the baked set from a release (no podman needed)
# or
make fetch-packs       # bake them yourself (needs network, docker/podman, ~an hour)
```

Without either, you get a working ISO with no application catalogue on it. See
[Applications](applications.md) for what the catalogue is and
[Packs and boxes](../03-architecture/packs-and-boxes.md) for how it is built.

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

The ISO is a hybrid image: write it to a USB stick as a raw byte stream.

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

## Boot

KDOS boots **UEFI only**. There is no BIOS boot path and no bootable-CD El Torito entry for one.
Select the stick in your firmware's boot menu; rEFInd appears, then the kernel starts.

The screen you see during boot is [the splash](../03-architecture/boot-and-init.md), which draws
a CRT power-on directly to the framebuffer and names each stage as it completes. The stages tell
you where a failed boot stopped:

| Stage | If it stops here |
|---|---|
| `DEVICE MANAGER` | udev did not come up |
| `FILESYSTEM MODULES` | the initramfs lacks the module for your root filesystem |
| `BOOT SLOT` | the A/B state file is unreadable |
| `UNLOCKING` | the encrypted root passphrase was refused three times |
| `ROOT DEVICE` / `BOOT MEDIA` | the root or the medium was not found |
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
| `tty1` | Autologin as `kdos` — this is where you start the desktop |
| `tty2` | An ordinary login prompt |
| `ttyS0` | A serial login, used by the test rig |

Switch between them with `Alt+F1` and `Alt+F2`.

On `tty1` you land at a shell behind the login banner, drawn one raster line at a time with a
bright beam leading the fill. Any keypress skips the rest of the animation.

The console is running the KDOS console font at 16x32 — 512 glyphs, loaded by
[`kdos-getty`](../03-architecture/boot-and-init.md) rather than by an init script. That font is
why parts of this system deliberately restrict themselves to a small glyph set: see
[the design language](../03-architecture/design-language.md).

## Start the desktop

From `tty1`:

```sh
kdos-desktop
```

There is no display manager and no graphical login. That is deliberate: a display manager is a
privileged process whose only job is to run the thing you are about to run anyway, and on a
single-user workstation it buys nothing.

`kdos-desktop` sets up the session environment, starts or reuses the per-user message bus, brings
up audio, warms the boxes for your pinned applications, and then executes the compositor. If the
compositor exits, you are returned to the tty with its log at
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
