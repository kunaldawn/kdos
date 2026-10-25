## About
[QEMU]{.idx} emulates a whole machine, and with KVM runs one at the speed of the host processor.
A container shares the host's kernel, so QEMU is what KDOS has for running another operating
system: testing a kernel, opening a disk image whose contents are not trusted, booting the
installer of another distribution. It is the emulator under [libvirt](#port-libvirt) and
virt-manager, and QEMU is also what KDOS's own test rig drives. The port builds three system emulators, the
`qemu-img`, `qemu-nbd` and `qemu-storage-daemon` tools, and the guest firmware those emulators
boot, compiled from the sources in the tarball's `roms/`.

## Flags
- `--target-list=x86_64-softmmu,aarch64-softmmu,riscv64-softmmu` — build three system
  emulators: this machine's own architecture, the aarch64 boards the tree cross-compiles for, and
  riscv64. The target list is the largest lever on the size of the package.
- `--libexecdir=/usr/lib/qemu` — the directory of the helper programs QEMU starts, such as
  `qemu-bridge-helper`, which QEMU looks for there by the path compiled in. `qemu-bridge-helper` is installed without its setuid bit and with no
  `/etc/qemu/bridge.conf`, so `-netdev bridge` serves only root, and only once root writes an
  `allow <bridge>` line there (see
  [A qemu guest cannot join a host bridge as an ordinary user](../../../../docs/kdos/06-reference/known-gaps.md#a-qemu-guest-cannot-join-a-host-bridge-as-an-ordinary-user));
  `--enable-passt` is the unprivileged network, through [passt](#port-passt).
- `--enable-gtk` — with `--enable-vte`: the window QEMU opens by default, a GTK 3 Wayland client.
  [vte3](#port-vte3) gives the window its text tabs, where the monitor and the serial console
  appear. meson links `libX11` into it whenever GTK's X11 backend is installed, with no option to
  stop it, so [libX11](#port-libX11) is in `depends`.
- `--enable-sdl` — `-display sdl`, through [sdl2-compat](#port-sdl2-compat).
- `--enable-opengl` — GL scanout in the GTK and SDL windows and on the D-Bus display, and the
  `egl-headless` display, through [libepoxy](#port-libepoxy) on Mesa's EGL.
- `--disable-virglrenderer` — with `--disable-rutabaga-gfx`: no 3D for the guest
  (`virtio-vga-gl`). virglrenderer is not a port, so a guest renders in software.
- `--enable-curses` — a text-mode console, `-display curses`.
- `--enable-vnc` — with `--enable-vnc-jpeg`, `--enable-png` and `--enable-vnc-sasl`: a guest's
  screen served to any VNC viewer. KDOS's own `testing/vnc-shot.py` drives a virtual machine that
  way.
- `--enable-spice` — with `--enable-spice-protocol`: the SPICE display that `remote-viewer` and
  virt-manager attach to, with a shared clipboard.
- `--enable-usb-redir` — the `usb-redir` device, which takes a USB device redirected from
  spice-gtk or `usbredirect`, through [usbredir](#port-usbredir).
- `--enable-smartcard` — the emulated CCID reader a host smart card is passed through, built on
  [libcacard](#port-libcacard).
- `--enable-libiscsi` — the `iscsi://` block driver, which opens a LUN with no kernel initiator.
- `--enable-tpm` — the TPM backend, which talks to a [swtpm](#port-swtpm) process. Like the
  [virtiofsd](#port-virtiofsd) daemon behind `-device vhost-user-fs-pci`, swtpm is a separate
  program a guest's command line names, not a library this build links.
- `--enable-capstone` — disassembly: `-d in_asm,out_asm` and the monitor's `x/i` print
  instructions instead of bytes. [capstone](#port-capstone) is in `depends`, because with the
  switch named a missing one is a configure error.
- `--enable-pipewire` — with `--enable-alsa`, `--disable-pa`, `--disable-jack`,
  `--disable-sndio` and `--disable-oss`: the guest's sound goes to [PipeWire](#port-pipewire)
  directly, or through ALSA.
- `--enable-fdt=system` — link the [dtc](#port-dtc) port's `libfdt`. The tarball carries no copy,
  only `subprojects/dtc.wrap`, a git checkout meson would fetch, which `--disable-download`
  forbids.
- `--disable-download` — keep configure off the network. With downloads allowed, its default,
  configure's Python environment drops the offline flag and goes to the network for packages the
  tarball already carries (see
  [A build that reaches the network](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-build-that-reaches-the-network)).
- `--disable-containers` — without it, configure answers a missing cross compiler by planning to
  build firmware in a podman image, which a build with no network cannot pull.
- `--disable-install-blobs` — do not install `pc-bios/`, upstream's prebuilt firmware for every
  board of every architecture QEMU emulates. What ships instead is built by `build.sh` from
  `roms/`.
- `--disable-docs` — `docs/conf.py` requires `sphinx_rtd_theme` even for the manual pages, and
  that theme is not a port.
- `--enable-keyring` — with `--disable-libkeyutils`: `-object secret_keyring` reads a secret
  from the kernel keyring through the system call built in by `--enable-keyring`. QEMU links
  libkeyutils only into its crypto unit test, so leaving it out costs nothing at run time.
- `--disable-nettle` — with `--disable-gcrypt` and `--enable-gnutls`: QEMU's cipher and hash
  code uses [GnuTLS](#port-gnutls), which meson prefers unless nettle or libgcrypt is asked for.
- `--disable-libnfs` — with `--disable-libssh`, `--disable-snappy`, `--disable-numa`,
  `--disable-libdaxctl`, `--disable-brlapi`, `--disable-libcbor`, `--disable-valgrind`,
  `--disable-xkbcommon`, `--disable-sdl-image` and `--disable-auth-pam`: the `nfs://` and `ssh://`
  block drivers, snappy compression for migration, NUMA host-memory policy, `libdaxctl`, the
  braille character device, `libcbor`, Valgrind support for coroutine stacks, xkbcommon keymaps,
  SDL window icons and PAM access control for VNC are left out. Each library is a port the build
  does not link.
- `--disable-selinux` — and `mpath`, `rbd`, `rdma`, `lzfse`, `vde`, `netmap`, `blkio`, `libpmem`,
  `qpl`, `uadk`, `qatzip`, `af-xdp`, `igvm`, `u2f`, `canokey`, `sparse` and `xen`: features whose
  library is not a port.
- `--disable-guest-agent` — no `qemu-ga`, the agent that runs inside a guest.
- `--disable-malloc-trim` — do not call the C library's `malloc_trim()`, which musl does not have.
- `--disable-werror` — with `--disable-debug-info`: warnings do not stop the build, and nothing
  is compiled with `-g`.
- `--enable-kvm` — and the rest of the `--enable-` list: KVM acceleration, and each feature
  named so that a missing library stops configure.

## Patches
- `ipxe-arch-i386.patch` — puts a `.code16` or `.code32` directive ahead of `.arch i386` or `.arch i586` in the x86 assembly files iPXE shares between its BIOS and x86_64 EFI builds, and drops the directive from the two stack files. GNU as 2.41 and later reject those `.arch` lines while the assembler is still in 64-bit mode, so without the patch every one of these files fails in the `bin-x86_64-efi` build the `efi-*.rom` NIC option ROMs come from.
- `edk2-gcc-no-werror.patch` — adds `-Wno-error` after the `-Werror` in `GCC_ALL_CC_FLAGS` of edk2's `tools_def.template`, which neither edk2's build command nor the platform files can reach. It is applied before `efirom`, because that step runs edk2's setup, which copies those flags into `Conf/tools_def.txt`, and every later edk2 build reads the copy (see [An upstream `-Werror`](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-upstream--werror)).

## Notes
The firmware is built in a subshell that clears the host's `CFLAGS`, `CXXFLAGS`, `CPPFLAGS` and
`LDFLAGS`. It is freestanding code for the guest, laid out by its own linker scripts; qboot's meson
would otherwise compile `-fPIC` into a real-mode BIOS. `CC` keeps `-std=gnu11` because edk2's
BaseTools and iPXE's drivers declare functions with empty parentheses and call them with
arguments, which C23 makes an error. `EXTRA_OPTFLAGS=-Wno-error` is the last word of the flags
edk2's BaseTools compile their host programs with, after upstream's `-Werror`. Every makefile is
given `PYTHON=python3` (see
[A build calling `python`](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-build-calling-python)).

What is built, in order:

- SeaBIOS and SeaVGABIOS: `bios-256k.bin`, which `pc` and `q35` load, `bios.bin` for old machine
  types, `bios-microvm.bin`, and one VGA BIOS per emulated display adapter; and qboot, microvm's
  fast-boot BIOS.
- iPXE, with `NO_WERROR=1`: every emulated network card loads its option ROM at start-up, and a
  missing one stops QEMU. `efi-*.rom`, the default, is the BIOS image and the x86_64 UEFI driver
  joined by edk2's `EfiRom`; `pxe-*.rom` is the BIOS-only image old machine types use.
- The option ROMs `-kernel` boots through (`linuxboot_dma.bin`, `multiboot_dma.bin`, `pvh.bin`,
  `kvmvapic.bin`), built from QEMU's own makefile. configure builds them only when a probe links a
  `-m32` object against a 32-bit libgcc, which the tree's gcc, built without multilib, does not
  have; the ROMs link `-nostdlib` and need none.
- OVMF for x86_64, plain and Secure Boot with SMM, and ArmVirtQemu for aarch64, through QEMU's own
  `edk2-build.py` with a config of the three builds only. aarch64 is compiled by
  [clang](#port-clang), which cross-compiles for every target the llvm port builds; x86_64 by the
  native gcc; [acpica](#port-acpica)'s `iasl` compiles the ACPI tables and [nasm](#port-nasm) the
  x86 assembly. `MAKEFLAGS` is removed for this step: edk2 runs one make per module on its own
  threads, and an inherited `-j` races a module's `.efi` against the firmware volume that packs
  it. The aarch64 images are then padded to 64 MiB, the size the `virt` machine's flash banks
  require.
- OpenSBI, the firmware every riscv machine loads by default, with `LLVM=1` (clang and `ld.lld`)
  for the position-independent image it needs, in both widths, since `qemu-system-riscv64` also
  runs rv32 CPUs. Each output directory is created first and passed absolute, because the
  makefile resolves `O=` with `readlink -f`.

The riscv64 `virt` UEFI image is the one image installed as upstream built it, unpacked from
`pc-bios/`; see
[What is not built from source](../../../../docs/kdos/01-philosophy/why-kdos.md#what-is-not-built-from-source).
Firmware is built only for what the three targets boot by default or through their firmware
descriptors; the BMC boards inside `qemu-system-aarch64` have none (see
[qemu carries firmware only for its three targets' default machines](../../../../docs/kdos/06-reference/known-gaps.md#qemu-carries-firmware-only-for-its-three-targets-default-machines)).

The firmware descriptors in `/usr/share/qemu/firmware` are how libvirt and other front ends find a
UEFI image and its variable-store template for a machine. `configure_file` fills their
`@DATADIR@` only when it installs the blobs, so `build.sh` writes them with the path filled in.
The x86_64 variable store is named `edk2-i386-vars.fd` because the i386 and x86_64 OVMF builds
share one layout, and the descriptors name it so.

`qemu` builds in `43_toolkits` because its display backend depends on GTK 3 and vte directly.
