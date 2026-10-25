# Configuring the kernel

KDOS configures its kernel from one file, [`kdos.config`]{.idx}, beside the recipe in
`ports/core/boot/linux/`. This chapter shows how the [linux](#port-linux) port's [`build.sh`]{.idx} turns
that file into a complete configuration, and the three checks that stop the build when the result
is not the kernel the file describes. It then lists the options the rest of KDOS relies on, where
each one is in `make menuconfig`, the value `kdos.config` gives it, and what stops working without
it.

## The file

`kdos.config` is a [kernel configuration]{.idx} in the minimal form `make savedefconfig` writes:
only the symbols whose value differs from the kernel's own default, each as a `CONFIG_FOO=value`
line or, for one that is off, a `# CONFIG_FOO is not set` line. Every other symbol takes its
default. A few lines are written out although they equal the default, so that a later kernel that
changes the default cannot change KDOS's kernel with them; some of the [hardening]{.idx term="hardening flags"} options at the
end of the tables below are such lines. The file starts with the KDOS banner, which Kconfig reads as comments.

The way to change the file, and why it must be regenerated inside the chroot, is in
[The kernel configuration](../../kdos/05-developer/how-kdos-is-built.md#the-kernel-configuration).

## How the build applies it

These are the lines of `build.sh` that make the configuration, before anything is compiled:

<!-- kfs:include ports/core/boot/linux/build.sh lines=17-31 pin=9ae90b985e29 -->

`make mrproper`
: Removes every generated file and any `.config` from the source tree, so the configuration
  starts from `kdos.config` alone.

`cp $PORT_SRC/kdos-logo-mono.pbm drivers/video/logo/kdos-logo-mono.pbm`
: Puts the panic-screen logo where `CONFIG_LOGO_LINUX_MONO_FILE` points, ready for the compile;
  [Building the kernel](building.md#the-logo) explains the file.

`cp $PORT_SRC/kdos.config .config` and `make olddefconfig`
: `olddefconfig` reads `.config`, gives every symbol the file does not mention its default, and
  writes the full configuration back to `.config`, the complete set kbuild needs.

`KCONFIG_WERROR=1`
: Kconfig's warnings become errors, so `olddefconfig` stops instead of going on with a value it
  had to correct: `=m` given to an option that can only be `y` or `n`, or an option selected by
  another whose own dependencies are not met.

`KCONFIG_WARN_UNKNOWN_SYMBOLS=1`
: Kconfig warns about every symbol in the file that this kernel does not define. Together with
  `KCONFIG_WERROR`, a symbol renamed or removed by a new kernel release stops the build, instead
  of vanishing from the configuration without a word.

The kept-line check
: The `grep` pair takes every `CONFIG_…=` and `# CONFIG_… is not set` line of `kdos.config` and
  prints those that do not appear, character for character, in the resolved `.config`. Any output
  stops the build. `olddefconfig` raises no warning when it changes a value whose dependencies
  are not met: an option set to `y` that depends on a module comes out as `m`, and an option whose
  dependency is off disappears. A tree with no `pahole`, for example, loses
  `CONFIG_DEBUG_INFO_BTF=y` and every line that depends on it, `CONFIG_SCHED_CLASS_EXT=y` among
  them, and this check prints each one.

The hard-asserted options
: The `for` loop requires six options to be `=y` in the resolved configuration:
  `CONFIG_RUST`, `CONFIG_DRM_PANIC_SCREEN_QR_CODE`, `CONFIG_DEBUG_INFO_BTF`,
  `CONFIG_DEBUG_INFO_BTF_MODULES`, `CONFIG_SQUASHFS_ZSTD` and `CONFIG_SQUASHFS_XZ`. Most are in
  `kdos.config` and covered by the check before, but not all: `CONFIG_DEBUG_INFO_BTF_MODULES`
  defaults to `y` once [BTF]{.idx} is on, so `savedefconfig` leaves it out of the file and only this loop
  sees it go. The first four are the options a missing tool takes away: Rust needs [`rustc`]{.idx},
  `bindgen` and libclang, and BTF needs `pahole` (see
  [Building the kernel](building.md#what-rust-and-btf-need)).

The last setting is made after these checks: inside the KDOS build, `scripts/config` points
`CONFIG_MODULE_SIG_KEY` at the persistent [signing key]{.idx}. That is in
[Building the kernel](building.md#the-module-signing-key).

### Checking a configuration yourself

The checks can be run by hand in an unpacked kernel tree inside the chroot, which has the same
`rustc`, `clang` and `pahole` the build uses:

```bash
cp /ports/core/boot/linux/kdos.config .config
KCONFIG_WERROR=1 KCONFIG_WARN_UNKNOWN_SYMBOLS=1 make olddefconfig
grep -E '^(CONFIG_[A-Za-z0-9_]+=|# CONFIG_[A-Za-z0-9_]+ is not set$)' \
    /ports/core/boot/linux/kdos.config | grep -vxFf .config
```

No output from the last command means every line of the file survived. `make rustavailable` says
whether the kernel's build finds a Rust toolchain it accepts, and why not when it does not.

On a running KDOS system, [`/proc/config.gz`]{.idx} holds the configuration of the kernel that is running,
so a single option is one command away:

```bash
zcat /proc/config.gz | grep -E 'CONFIG_DEBUG_INFO_BTF\b|CONFIG_RUST='
```

## The options KDOS depends on

The tables below are a selection, not the whole configuration. Each row names an option that some
part of KDOS needs, with:

- **Location**: the menus `make menuconfig` opens to reach it, ending with the option's own prompt
  in italics. The paths are those of this kernel's Kconfig files; some menus, such as the
  *Direct Rendering Manager* entry, hold options inside an option rather than a submenu, and
  menuconfig shows those indented under it.
- **Value**: what `kdos.config` sets: `y` for built in, `m` for a module, `n` for a
  `# … is not set` line, or the string the option is given.
- **Used for**: the part of KDOS that needs it, and what goes wrong without it.

`testing/kfscheck.py` compares every row here with `kdos.config` and fails when a value differs or
a row names an option the file does not set.

### Booting, modules and firmware

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_KERNEL_ZSTD` | General setup → Kernel compression mode → *ZSTD* | `y` | The compressor of the kernel image itself. The setup header in front of it stays uncompressed, which is where `postinstall.sh` reads the kernel's version from |
| `CONFIG_EFI` | Processor type and features → *EFI runtime service support* | `y` | UEFI runtime services. Without it there is no `/sys/firmware/efi`, and so no EFI variables |
| `CONFIG_EFI_STUB` | Processor type and features → EFI runtime service support → *EFI stub support* | `y` | Makes the kernel image an EFI application. Mixed mode, the next row, depends on it |
| `CONFIG_EFI_MIXED` | Processor type and features → EFI runtime service support → EFI stub support → *EFI mixed-mode support* | `y` | Lets the 64-bit kernel run on 32-bit UEFI firmware, the machines the medium's `EFI/BOOT/BOOTIA32.EFI` is there for |
| `CONFIG_EFIVAR_FS` | File systems → Pseudo [filesystems]{.idx term="filesystem"} → *EFI Variable filesystem* | `y` | `/sys/firmware/efi/efivars`, which `rcS` mounts on a UEFI boot. `kdos-bootctl` writes the one-boot `BootNext` entry of an update trial through it |
| `CONFIG_DEVTMPFS` | Device Drivers → Generic Driver Options → *Maintain a devtmpfs filesystem to mount at /dev* | `y` | `/dev` as a file system the kernel fills with device nodes. The [initramfs]{.idx} mounts it, and later moves it into the new root |
| `CONFIG_DEVTMPFS_MOUNT` | Device Drivers → Generic Driver Options → Maintain a devtmpfs filesystem to mount at /dev → *Automount devtmpfs at /dev, after the kernel mounted the rootfs* | `y` | The same mount made by the kernel for a boot with no initramfs. A boot with one, which every KDOS boot is, mounts it itself |
| `CONFIG_MODULES` | *Enable loadable module support* | `y` | Loadable modules: many drivers in the tables below are `m`, loaded by udev or by the initramfs |
| `CONFIG_MODULE_COMPRESS_ZSTD` | Enable loadable module support → Module compression → Module compression type → *ZSTD* | `y` | Modules are installed as `.ko.zst`. `postinstall.sh` and the image's initramfs builder decompress the ones they copy into an initramfs |
| `CONFIG_FW_LOADER_COMPRESS_ZSTD` | Device Drivers → Generic Driver Options → Firmware loader → Enable compressed firmware support → *Enable ZSTD-compressed firmware support* | `y` | The [linux-firmware](#port-linux-firmware) port installs every file compressed (`copy-firmware.sh --zstd`). Without this the kernel finds none of the names a driver asks for, and the [GPU]{.idx}, Wi-Fi and [Bluetooth]{.idx} of a real machine stay dark |
| `CONFIG_IKCONFIG` | General setup → *Kernel .config support* | `y` | Keeps the resolved configuration inside the kernel |
| `CONFIG_IKCONFIG_PROC` | General setup → Kernel .config support → *Enable access to .config through /proc/config.gz* | `y` | `/proc/config.gz`, the way to check any option of the running kernel |
| `CONFIG_DEFAULT_HOSTNAME` | General setup → *Default hostname* | `"kdos"` | The host name from the start of boot until `05_hostname.sh` sets the one in `/etc/hostname` |

### The display, the splash and the panic screen

In this table, *DRM* stands for Device Drivers → Graphics support → *Direct Rendering Manager
(XFree86 4.1.0 and higher DRI support)*.

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_DRM` | Device Drivers → Graphics support → *Direct Rendering Manager (XFree86 4.1.0 and higher DRI support)* | `y` | The display subsystem every graphics driver and the compositor go through. Built in rather than a module, because the panic screen draws a logo only from a built-in DRM |
| `CONFIG_DRM_SIMPLEDRM` | DRM → Drivers for system framebuffers → *Simple framebuffer driver* | `y` | A DRM driver for the framebuffer the firmware set up, so there is a display from the first moment of boot, before any GPU driver module loads |
| `CONFIG_SYSFB_SIMPLEFB` | Device Drivers → Firmware Drivers → *Mark VGA/VBE/EFI FB as generic system framebuffer* | `y` | Hands the firmware's framebuffer to the driver in the row above |
| `CONFIG_FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER` | Device Drivers → Graphics support → Console display driver support → Framebuffer Console support → *Framebuffer Console Deferred Takeover* | `y` | The text console does not take the framebuffer until a glyph is printed on a virtual terminal, which leaves it to [the splash](../../kdos/03-architecture/boot-and-init.md#the-splash) while the boot prints on the serial line |
| `CONFIG_DRM_I915` | DRM → *Intel 8xx/9xx/G3x/G4x/HD Graphics* | `m` | Intel graphics |
| `CONFIG_DRM_XE` | DRM → *Intel Xe2 Graphics* | `m` | Intel Xe graphics |
| `CONFIG_DRM_AMDGPU` | DRM → *AMD GPU* | `m` | AMD graphics |
| `CONFIG_DRM_NOUVEAU` | DRM → *Nouveau (NVIDIA) cards* | `m` | NVIDIA graphics |
| `CONFIG_DRM_VIRTIO_GPU` | DRM → *Virtio GPU driver* | `y` | QEMU's virtio-gpu, the display of the test rig |
| `CONFIG_DRM_BOCHS` | DRM → *DRM Support for bochs dispi vga interface (qemu stdvga)* | `y` | QEMU's standard VGA |
| `CONFIG_DRM_PANIC` | DRM → *Display a user-friendly message when a kernel panic occurs* | `y` | Draws a panic screen over whatever is on the display when the kernel panics |
| `CONFIG_DRM_PANIC_SCREEN` | DRM → Display a user-friendly message when a kernel panic occurs → *Panic screen formatter* | `"qr_code"` | Chooses the QR-code screen over the plain message |
| `CONFIG_DRM_PANIC_SCREEN_QR_CODE` | DRM → Display a user-friendly message when a kernel panic occurs → *Add a panic screen with a QR code* | `y` | The QR-code screen. `kdos.config` gives it no base URL (`CONFIG_DRM_PANIC_SCREEN_QR_CODE_URL`), so the code holds the kernel log as plain text, which a phone's camera reads without any web page |
| `CONFIG_RUST` | General setup → *Rust support* | `y` | The QR-code generator is written in Rust, and the option above depends on this one |
| `CONFIG_LOGO` | Device Drivers → Graphics support → *Bootup logo* | `y` | A logo on the panic screen. Without it the panic screen draws a built-in ASCII Tux |
| `CONFIG_LOGO_LINUX_MONO` | Device Drivers → Graphics support → Bootup logo → *Standard black and white Linux logo* | `y` | The one-bit logo, the only kind the panic screen draws |
| `CONFIG_LOGO_LINUX_MONO_FILE` | Device Drivers → Graphics support → Bootup logo → Standard black and white Linux logo → *Monochrome logo .pbm file* | `"drivers/video/logo/kdos-logo-mono.pbm"` | The KDOS mascot in place of Tux, from the file `build.sh` copies in |
| `CONFIG_LOGO_LINUX_CLUT224` | Device Drivers → Graphics support → Bootup logo → *Standard 224-color Linux logo* | `n` | Leaves the colour Tux out of the kernel, so a framebuffer console that shows a logo shows the KDOS one |

### Disks, root file systems and the slots

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_BLK_DEV_NVME` | Device Drivers → NVME Support → *NVM Express block device* | `y` | NVMe disks |
| `CONFIG_VMD` | Device Drivers → PCI support → PCI controller drivers → *Intel Volume Management Device Driver* | `m` | NVMe disks behind Intel VMD (RST). The initramfs carries it, or the installed root on such a disk is never found |
| `CONFIG_ATA` | Device Drivers → *Serial ATA and Parallel ATA drivers (libata)* | `y` | SATA disks and optical drives |
| `CONFIG_SATA_AHCI` | Device Drivers → Serial ATA and Parallel ATA drivers (libata) → *AHCI SATA support* | `y` | The SATA controller of nearly every PC |
| `CONFIG_BLK_DEV_SD` | Device Drivers → SCSI device support → *SCSI disk support* | `y` | `/dev/sd*`: SATA and USB disks |
| `CONFIG_VIRTIO_BLK` | Device Drivers → Block devices → *Virtio block driver* | `y` | A [virtual machine]{.idx}'s disks |
| `CONFIG_USB_XHCI_HCD` | Device Drivers → USB support → *xHCI HCD (USB 3.0) support* | `y` | USB 3 controllers, and with them the USB stick the medium usually boots from |
| `CONFIG_USB_STORAGE` | Device Drivers → USB support → *USB Mass Storage support* | `y` | USB sticks and disks |
| `CONFIG_USB_UAS` | Device Drivers → USB support → USB Mass Storage support → *USB Attached SCSI* | `m` | Faster USB disks that speak UAS; in the initramfs, so a medium on one is found |
| `CONFIG_BLK_DEV_SR` | Device Drivers → SCSI device support → *SCSI CDROM support* | `y` | The medium burned to a disc, or attached to a virtual machine as a CD |
| `CONFIG_EXT4_FS` | File systems → *The Extended 4 (ext4) filesystem* | `y` | The installer's default root file system |
| `CONFIG_BTRFS_FS` | File systems → *Btrfs filesystem support* | `y` | A btrfs root |
| `CONFIG_XFS_FS` | File systems → *XFS filesystem support* | `y` | An xfs root |
| `CONFIG_F2FS_FS` | File systems → *F2FS filesystem support* | `m` | An f2fs root. A module, so the initramfs carries it; an f2fs root it could not mount would install and never boot |
| `CONFIG_MD` | Device Drivers → *Multiple devices driver support ([RAID]{.idx} and [LVM]{.idx})* | `y` | The menu of software RAID and the [device mapper]{.idx} |
| `CONFIG_BLK_DEV_DM` | Device Drivers → Multiple devices driver support (RAID and LVM) → *Device mapper support* | `m` | LVM and LUKS, both device-mapper targets |
| `CONFIG_DM_CRYPT` | Device Drivers → Multiple devices driver support (RAID and LVM) → Device mapper support → *Crypt target support* | `m` | An encrypted root, opened by the initramfs from `cryptdevice=` |
| `CONFIG_VFAT_FS` | File systems → DOS/FAT/EXFAT/NT Filesystems → *VFAT (Windows-95) fs support* | `y` | The ESP. The initramfs reads the [A/B boot state](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection) off it, and each slot's kernel and initramfs live on it |
| `CONFIG_NLS_CODEPAGE_437` | File systems → Native language support → *Codepage 437 (United States, Canada)* | `y` | The code page a FAT mount uses for short names unless told otherwise; without it the ESP does not mount |
| `CONFIG_EXFAT_FS` | File systems → DOS/FAT/EXFAT/NT Filesystems → *exFAT filesystem support* | `y` | exFAT drives, which `kdos-mountd` mounts for the desktop |

### The live medium and packs

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_ISO9660_FS` | File systems → CD-ROM/DVD Filesystems → *ISO 9660 CDROM file system support* | `y` | The medium's own file system, which holds the kernel, the initramfs and `system.sfs` |
| `CONFIG_SQUASHFS` | File systems → Miscellaneous filesystems → *[SquashFS]{.idx} 4.0 - Squashed file system support* | `y` | `system.sfs`, the whole live root in one compressed image |
| `CONFIG_SQUASHFS_ZSTD` | File systems → Miscellaneous filesystems → SquashFS 4.0 - Squashed file system support → *Include support for ZSTD compressed file systems* | `y` | `110_iso.sh` compresses `system.sfs` with zstd unless told otherwise |
| `CONFIG_SQUASHFS_XZ` | File systems → Miscellaneous filesystems → SquashFS 4.0 - Squashed file system support → *Include support for XZ compressed file systems* | `y` | `KDOS_ISO_COMP=xz` builds an xz `system.sfs` instead, and the same kernel reads it |
| `CONFIG_BLK_DEV_LOOP` | Device Drivers → Block devices → *Loopback device support* | `y` | Mounts `system.sfs` from a file on the medium; `kdos-packd` mounts a pack through one where the kernel cannot take the file directly |
| `CONFIG_OVERLAY_FS` | File systems → *Overlay filesystem support* | `y` | The live system's writable root, the read-only `system.sfs` under a writable upper layer, and the root of every box built from packs |
| `CONFIG_EROFS_FS` | File systems → Miscellaneous filesystems → *[EROFS]{.idx} filesystem support* | `m` | Every pack is an EROFS image; see [Mounting](../../kdos/03-architecture/packs-and-boxes.md#mounting) |
| `CONFIG_EROFS_FS_ZIP_ZSTD` | File systems → Miscellaneous filesystems → EROFS filesystem support → EROFS Data Compression Support → *EROFS Zstandard compressed data support* | `y` | `kdos-pack` builds packs with `mkfs.erofs -zzstd`; a kernel without this cannot read their files |

### Memory

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_ZRAM` | Device Drivers → Block devices → *Compressed RAM block device support* | `m` | The compressed [swap]{.idx} in RAM that `12_zram.sh` sets up. The script skips itself when the module does not load |
| `CONFIG_ZRAM_BACKEND_ZSTD` | Device Drivers → Block devices → Compressed RAM block device support → *zstd compression support* | `y` | zstd as a [zram]{.idx} compressor, the algorithm `12_zram.sh` asks for by default |
| `CONFIG_ZRAM_DEF_COMP_ZSTD` | Device Drivers → Block devices → Compressed RAM block device support → Default zram compressor → *zstd* | `y` | zstd as the device's own default, the one kept when `/etc/kdos/zram.conf` names an algorithm the kernel lacks |
| `CONFIG_ZSWAP` | Memory Management options → Support for paging of anonymous memory (swap) → *Compressed cache for swap pages* | `y` | A compressed cache in front of disk swap. `12_zram.sh` turns it off while zram is the swap and on again when zram is removed |
| `CONFIG_PSI` | General setup → CPU/Task time and stats accounting → *Pressure stall information tracking* | `y` | `/proc/pressure/`. `kdos-oomd` blocks on a memory-pressure trigger there and does not start without it; `kdos-res` shows CPU and memory pressure from it |

### Boxes, containers and the sandbox

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_USER_NS` | General setup → Namespaces support → *User [namespace]{.idx}* | `y` | Rootless [boxes]{.idx term="box"}: podman maps your identity into each one. Chromium's sandbox uses it too; see [setuid binaries](../../kdos/03-architecture/security-model.md#setuid-binaries) |
| `CONFIG_MEMCG` | General setup → Control Group support → *Memory controller* | `y` | A box profile's `memory` key, applied as podman's `--memory` |
| `CONFIG_CGROUP_PIDS` | General setup → Control Group support → *PIDs controller* | `y` | A box profile's `pids` key, applied as `--pids-limit` |
| `CONFIG_CFS_BANDWIDTH` | General setup → Control Group support → CPU controller → Group scheduling for SCHED_OTHER → *CPU bandwidth provisioning for FAIR_GROUP_SCHED* | `y` | A box profile's `cpus` key, applied as `--cpus` |
| `CONFIG_VETH` | Device Drivers → Network device support → Network core driver support → *Virtual ethernet pair device* | `y` | The link between a container's network namespace and the bridge it sits on |
| `CONFIG_BRIDGE` | Networking support → Networking options → *802.1d Ethernet Bridging* | `y` | podman's `podman*` bridges and libvirt's `virbr*` bridges, which the firewall's drop-ins name |
| `CONFIG_TUN` | Device Drivers → Network device support → Network core driver support → *Universal TUN/TAP device driver support* | `m` | `/dev/net/tun`, through which [passt](#port-passt)'s `pasta`, the rootless boxes' network, and `slirp4netns` give a container's namespace its interface; `/etc/modules-load.d/kdos-containers.conf` loads it at boot |
| `CONFIG_SECURITY` | Security options → *Enable different security models* | `y` | The security-module framework the next four rows plug into |
| `CONFIG_SECURITY_LANDLOCK` | Security options → *Landlock support* | `y` | `kdos sandbox`, built on `libkbase`'s Landlock calls; see [the kdos command](../../kdos/04-programs/kdos-command.md#kdos-sandbox) |
| `CONFIG_SECURITY_YAMA` | Security options → *Yama support* | `y` | Restricts `ptrace` to a process's own descendants unless a program is granted more |
| `CONFIG_LSM` | Security options → *Ordered list of enabled LSMs* | `"landlock,lockdown,yama,integrity"` | The security modules turned on at boot. Landlock compiled in but missing from this list answers every request with `EOPNOTSUPP`, and the sandbox refuses to run unconfined |

### BPF and tracing

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_BPF_JIT` | General setup → BPF subsystem → *Enable BPF Just In Time compiler* | `y` | Compiles BPF programs to machine code; sched_ext below depends on it |
| `CONFIG_BPF_JIT_ALWAYS_ON` | General setup → BPF subsystem → Enable BPF Just In Time compiler → *Permanently enable BPF JIT and remove BPF interpreter* | `y` | Removes the BPF interpreter, so no BPF program runs interpreted |
| `CONFIG_CGROUP_BPF` | General setup → Control Group support → *Support for eBPF programs attached to [cgroups]{.idx term="cgroup"}* | `y` | BPF programs attached to control groups, which is how cgroup v2 controls a container's device access |
| `CONFIG_DEBUG_INFO_DWARF5` | Kernel hacking → Compile-time checks and compiler options → Debug information → *Generate DWARF Version 5 debuginfo* | `y` | The debug information `pahole` turns into BTF. `INSTALL_MOD_STRIP=1` strips it from the modules again |
| `CONFIG_DEBUG_INFO_BTF` | Kernel hacking → Compile-time checks and compiler options → *Generate BTF type information* | `y` | `/sys/kernel/btf/vmlinux`, the kernel's own description of its types, which [bpftrace](#port-bpftrace), [bcc](#port-bcc) and CO-RE programs built with [libbpf](#port-libbpf) read |
| `CONFIG_SCHED_CLASS_EXT` | General setup → *Extensible Scheduling Class* | `y` | sched_ext, CPU schedulers loaded as BPF programs. Depends on BTF, and goes with it |
| `CONFIG_KPROBES` | General architecture-dependent options → *Kprobes* | `y` | Probes on any kernel function, bpftrace's `kprobe:` |
| `CONFIG_IKHEADERS` | General setup → *Enable kernel headers through /sys/kernel/kheaders.tar.xz* | `m` | The kernel's headers inside the kernel; bcc loads the module and compiles against them |

### Input, USB and virtual machines

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_INPUT_EVDEV` | Device Drivers → Input device support → Generic input layer (needed for keyboard, mouse, ...) → *Event interface* | `y` | `/dev/input/event*`, where libinput, and through it the compositor, reads every keyboard, mouse and touchpad |
| `CONFIG_HID_MULTITOUCH` | Device Drivers → HID bus support → HID bus core support → Special HID drivers → *HID Multitouch panels* | `y` | Precision touchpads and touchscreens |
| `CONFIG_I2C_HID_ACPI` | Device Drivers → HID bus support → HID bus core support → I2C HID support → *HID over I2C transport layer ACPI driver* | `m` | The touchpads of most laptops, which sit on I²C rather than USB |
| `CONFIG_INPUT_UINPUT` | Device Drivers → Input device support → Generic input layer (needed for keyboard, mouse, ...) → Miscellaneous devices → *User level driver support* | `m` | `/dev/uinput`, for input devices made by programs; loaded when the node is first opened |
| `CONFIG_UHID` | Device Drivers → HID bus support → HID bus core support → *User-space I/O driver support for HID subsystem* | `m` | `/dev/uhid`, through which [BlueZ](#port-bluez) creates Bluetooth LE keyboards and mice |
| `CONFIG_USB4` | Device Drivers → *Unified support for USB4 and [Thunderbolt]{.idx}* | `m` | USB4 and Thunderbolt ports, which `boltd` authorises |
| `CONFIG_KVM` | Virtualization → *Kernel-based Virtual Machine ([KVM]{.idx}) support* | `m` | `/dev/kvm`, hardware virtualisation for [QEMU](#port-qemu) and [libvirt](#port-libvirt) |
| `CONFIG_KVM_INTEL` | Virtualization → *KVM for Intel (and compatible) processors support* | `m` | KVM on Intel processors |
| `CONFIG_KVM_AMD` | Virtualization → *KVM for AMD processors support* | `m` | KVM on AMD processors |
| `CONFIG_VHOST_NET` | Device Drivers → VHOST drivers → *Host kernel accelerator for virtio net* | `m` | A guest's virtio network handled in the host kernel; loaded when `/dev/vhost-net` is first opened |

### The firewall and the network

In this table, *nf_tables* stands for Networking support → Networking options → Network packet
filtering framework (Netfilter) → Core Netfilter Configuration → *Netfilter nf_tables support*.

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_NETFILTER` | Networking support → Networking options → *Network packet filtering framework (Netfilter)* | `y` | The packet-filtering framework under nftables |
| `CONFIG_NF_TABLES` | Networking support → Networking options → Network packet filtering framework (Netfilter) → Core Netfilter Configuration → *Netfilter nf_tables support* | `m` | [The firewall](../../kdos/03-architecture/security-model.md#the-firewall): `25_nftables.sh` loads `/etc/nftables.conf` into it |
| `CONFIG_NF_TABLES_INET` | nf_tables → *Netfilter nf_tables mixed IPv4/IPv6 tables support* | `y` | The `inet` family: the shipped ruleset is the one table `inet filter`, for IPv4 and IPv6 together |
| `CONFIG_NFT_CT` | nf_tables → *Netfilter nf_tables conntrack module* | `m` | `ct state established,related accept`, the first rule of the input chain; without it the ruleset does not load |
| `CONFIG_NFT_NAT` | nf_tables → *Netfilter nf_tables nat module* | `m` | The published-port DNAT of rootful podman, which its network backend, netavark, keeps in a table of its own, `inet netavark` |
| `CONFIG_NFT_MASQ` | nf_tables → *Netfilter nf_tables masquerade support* | `m` | The masquerade, in the same table, that lets containers on a podman bridge reach the network |
| `CONFIG_CFG80211` | Networking support → Wireless → *cfg80211 - wireless configuration API* | `m` | Wi-Fi configuration, and the regulatory database it loads from [wireless-regdb](#port-wireless-regdb) |
| `CONFIG_MAC80211` | Networking support → Wireless → cfg80211 - wireless configuration API → *Generic IEEE 802.11 Networking Stack (mac80211)* | `m` | The 802.11 stack most Wi-Fi drivers are built on |
| `CONFIG_IWLWIFI` | Device Drivers → Network device support → Wireless LAN → Intel devices → *Intel Wireless WiFi Next Gen AGN - Wireless-N/Advanced-N/Ultimate-N (iwlwifi)* | `m` | Intel Wi-Fi |

### Bluetooth and sound

In this table, *[ALSA]{.idx}* stands for Device Drivers → Sound card support → *Advanced Linux Sound
Architecture*.

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_BT` | Networking support → *Bluetooth subsystem support* | `m` | [Bluetooth](../../kdos/02-user-guide/administration.md#bluetooth), driven by BlueZ |
| `CONFIG_BT_RFCOMM` | Networking support → Bluetooth subsystem support → Bluetooth Classic (BR/EDR) features → *RFCOMM protocol support* | `m` | Serial links over Bluetooth Classic |
| `CONFIG_BT_HIDP` | Networking support → Bluetooth subsystem support → Bluetooth Classic (BR/EDR) features → *HIDP protocol support* | `m` | Bluetooth Classic keyboards and mice |
| `CONFIG_BT_HCIBTUSB` | Networking support → Bluetooth subsystem support → Bluetooth device drivers → *HCI USB driver* | `m` | Bluetooth adapters on USB, which includes most built into laptops |
| `CONFIG_BT_HCIUART` | Networking support → Bluetooth subsystem support → Bluetooth device drivers → *HCI UART driver* | `m` | Bluetooth adapters on a serial line |
| `CONFIG_SND` | Device Drivers → Sound card support → *Advanced Linux Sound Architecture* | `y` | ALSA, the kernel half of all sound; PipeWire plays through it |
| `CONFIG_SND_HDA_INTEL` | ALSA → HD-Audio → *HD Audio PCI* | `m` | The HD-Audio controller of most desktops and older laptops |
| `CONFIG_SND_SOC_SOF_TOPLEVEL` | ALSA → ALSA for SoC audio support → *Sound Open Firmware (SOF) platforms* | `y` | The menu of the SOF drivers, which bind the audio DSP of recent Intel laptops and play through [sof-firmware](#port-sof-firmware); without the firmware the driver binds and makes no sound |
| `CONFIG_SND_USB_AUDIO` | ALSA → USB sound devices → *USB Audio/MIDI driver* | `m` | USB headsets, interfaces and MIDI devices |
| `CONFIG_SND_ALOOP` | ALSA → Generic sound devices → *Generic loopback driver (PCM)* | `m` | `snd-aloop`, a loopback card the test rig uses as a capture device |

### Hardening and module signing

The six options after the first two are the hardening lines described in
[The kernel](../../kdos/03-architecture/security-model.md#the-kernel), written out even where they
equal the kernel's default.

| Option | Location | Value | Used for |
|:-----------------|:-------------|:---|:-----------------|
| `CONFIG_SECURITY_LOCKDOWN_LSM` | Security options → *Basic module for enforcing kernel lockdown* | `y` | Selects `CONFIG_MODULE_SIG`, and with it `CONFIG_MODULE_SIG_ALL` at its default, so the build signs every module; see [Module signing](../../kdos/03-architecture/security-model.md#module-signing) |
| `CONFIG_MODULE_SIG_KEY_TYPE_ECDSA` | Cryptographic API → Certificates for signature checking → Type of module signing key to be generated → *ECDSA* | `y` | The kind of key the kernel generates for itself when `CONFIG_MODULE_SIG_KEY` is left at its default, as it is in a build outside the KDOS tree |
| `CONFIG_X86_X32_ABI` | Binary Emulations → *x32 ABI for 64-bit mode* | `n` | Removes the x32 system-call entry path, which no program on KDOS uses |
| `CONFIG_UEVENT_HELPER` | Device Drivers → Generic Driver Options → *Support for uevent helper* | `n` | No program forked as root for every device event; `udevd` reads events from netlink |
| `CONFIG_PM_AUTOSLEEP` | Power management and ACPI options → *Opportunistic sleep* | `n` | Suspend only when user space asks, as `kdos-powerd` does |
| `CONFIG_LDISC_AUTOLOAD` | Device Drivers → Character devices → Enable TTY → *Automatically load TTY Line Disciplines* | `n` | No line-discipline module loaded on behalf of an unprivileged user |
| `CONFIG_PROC_MEM_FORCE_PTRACE` | Security options → Allow /proc/pid/mem access override → *Require active ptrace() use for access override* | `y` | Writing another process's read-only memory through `/proc/<pid>/mem` needs an attached debugger |
| `CONFIG_BUG_ON_DATA_CORRUPTION` | Security options → Kernel hardening options → Hardening of kernel data structures → *Trigger a BUG when data corruption is detected* | `y` | Corrupted kernel lists stop the offending task instead of logging and going on |

## Options the file does not set

Some settings KDOS relies on are not lines of `kdos.config` at all: they are the kernel's
defaults, or follow from an option that is. The kept-line check compares only the lines the file
writes, so a new kernel release that changes one of these defaults changes the kernel with no
error, and `kfscheck.py` keeps them out of the tables for the same reason. These are the values
`olddefconfig` gives them from `kdos.config`:

- `CONFIG_MODULE_SIG=y`, selected by `CONFIG_SECURITY_LOCKDOWN_LSM`; `CONFIG_MODULE_SIG_ALL=y` and
  `CONFIG_MODULE_SIG_SHA512=y`, its defaults; and `CONFIG_MODULE_SIG_FORCE` off, so a module with
  no valid signature still loads, and taints the kernel.
- `CONFIG_DEBUG_INFO_BTF_MODULES=y`, BTF for every module, on by default once BTF is. It is the one
  setting here that the build does check, in `build.sh`'s loop.
- `CONFIG_EROFS_FS_BACKED_BY_FILE=y`, which lets `kdos-packd` mount a pack straight from its file
  rather than through a loop device.
- `CONFIG_CFG80211_REQUIRE_SIGNED_REGDB=y`: cfg80211 loads the regulatory database only with a
  valid signature, which is why [wireless-regdb](#port-wireless-regdb) is a port of its own.
- `CONFIG_SND_SOC_SOF=m`, the SOF driver core, chosen by the menu option in the sound table.
- `CONFIG_MICROCODE=y` with `CONFIG_MICROCODE_LATE_LOADING` off: microcode reaches the processor
  only from the start of the initramfs; see
  [Microcode](../../kdos/03-architecture/boot-and-init.md#microcode).
- `CONFIG_PANIC_TIMEOUT=0`: a panicking kernel waits for a reset instead of rebooting, unless the
  command line carries `panic=`, as an update trial's entry does.
- `CONFIG_BLK_DEV_INITRD=y` and `CONFIG_RD_GZIP=y`: the initramfs, which is gzip-compressed.

On a running system, `zcat /proc/config.gz` shows them with everything else.

## See also

- [Building the kernel](building.md): the compile, the signing key, reproducibility, the logo
- [Installing the kernel](installing.md): what `postinstall.sh` does with the new kernel
- [The kernel configuration](../../kdos/05-developer/how-kdos-is-built.md#the-kernel-configuration): changing `kdos.config`
- [The kernel](../../kdos/03-architecture/security-model.md#the-kernel): the hardening options

<!-- book-nav -->
---

*Part XIV. The kernel, chapter 336.* Previous: [Part XIV. The kernel](intro.md) · [Contents](../README.md) · Next: [337. Building the kernel](building.md)
