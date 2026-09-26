#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# CONTAINERS SHARE THE HOST KERNEL, SO NOTHING ON THIS MACHINE COULD RUN
# ANOTHER OS. That is the gap: the appbox answers "run somebody else's
# userland" and answers nothing about running somebody else's KERNEL — testing
# a kernel change, opening a disk image whose contents you do not trust,
# booting the installer of the distro you are migrating from. On a distro whose
# own test rig is qemu, the shipped system could not run it.
#
# THREE TARGETS, NOT ALL OF THEM. `--target-list` is the single biggest lever
# on the size of this port: the full set is forty-odd emulators at roughly
# 350-400 MB, and the three here cover running this machine's own architecture
# (x86_64), the boards this tree already cross-compiles for (aarch64), and the
# ISA its FPGA and embedded ring targets (riscv64).
#
# EVERY GRAPHICAL FRONT END IS OFF and there is no loss: --enable-curses gives
# a text console on the cell grid, and --enable-vnc means a guest with a real
# framebuffer is watched over VNC — on this desktop from the boxed Remmina
# (app.remmina, which carries its VNC plugin) at localhost:5900, from another
# machine's viewer, or by this project's own testing/vnc-shot.py, which drives
# a VM exactly that way. GTK is the hard rule. SDL is off
# because meson.build looks up x11 with no option to stop it and links libX11
# into the SDL front end whenever that library is installed, which it is for
# Xwayland. --disable-opengl because with GTK, SDL and virglrenderer (not a
# port) off, GL adds only the egl-headless display and GL scanout on the D-Bus
# display, and costs mesa plus libepoxy, whose port depends on libX11.
#
# --disable-docs because docs/conf.py requires sphinx_rtd_theme even for the
# man pages, and that theme is not a port.
#
# --disable-install-blobs because pc-bios/ holds upstream's prebuilt firmware
# for every board of every architecture qemu emulates; what ships instead is
# built below from the firmware sources under roms/. --disable-containers
# because configure otherwise answers a missing cross compiler by planning to
# build firmware in a podman image, which a build with no network cannot pull.
#
# --enable-tpm builds the emulator backend, which talks to a swtpm process: the
# swtpm port is the guest's TPM. -device vhost-user-fs-pci talks to the
# virtiofsd port's daemon. Both are separate programs a guest's command line
# names, not libraries this build links. -netdev bridge runs
# qemu-bridge-helper, which is installed without its setuid bit and with no
# /etc/qemu/bridge.conf, so it serves only root, and only once root writes an
# `allow <bridge>` line there; passt is the unprivileged network.
#
# --enable-capstone is what makes `-d in_asm,out_asm` and the monitor's `x/i`
# print instructions rather than bytes for the x86_64 and aarch64 guests;
# capstone is in `depends`, because a missing one would be a configure error.
# --disable-libkeyutils costs nothing at run time: qemu links libkeyutils only
# into its crypto unit test, and `-object secret_keyring` reads the kernel
# keyring through the syscall that --enable-keyring builds in.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--libexecdir=/usr/lib/qemu \
	--target-list=x86_64-softmmu,aarch64-softmmu,riscv64-softmmu \
	--enable-kvm \
	--enable-curses \
	--enable-vnc \
	--enable-slirp \
	--enable-linux-aio \
	--enable-linux-io-uring \
	--enable-virtfs \
	--enable-tools \
	--enable-gnutls \
	--enable-cap-ng \
	--enable-libusb \
	--enable-pixman \
	--enable-png \
	--enable-vnc-jpeg \
	--enable-zstd \
	--enable-fdt=system \
	--enable-curl \
	--enable-bzip2 \
	--enable-lzo \
	--enable-libudev \
	--enable-libdw \
	--enable-capstone \
	--enable-bpf \
	--enable-fuse \
	--enable-fuse-lseek \
	--enable-vnc-sasl \
	--enable-gio \
	--enable-dbus-display \
	--enable-qemu-vnc \
	--enable-passt \
	--enable-tpm \
	--enable-attr \
	--enable-iconv \
	--enable-slirp-smbd \
	--enable-stack-protector \
	--enable-multiprocess \
	--enable-hv-balloon \
	--enable-vhost-kernel \
	--enable-vhost-net \
	--enable-vhost-user \
	--enable-vhost-crypto \
	--enable-vhost-vdpa \
	--enable-vhost-user-blk-server \
	--enable-libvduse \
	--enable-vduse-blk-export \
	--enable-l2tpv3 \
	--enable-keyring \
	--enable-replication \
	--enable-colo-proxy \
	--enable-bochs \
	--enable-cloop \
	--enable-dmg \
	--enable-qcow1 \
	--enable-vdi \
	--enable-vhdx \
	--enable-vmdk \
	--enable-vpc \
	--enable-vvfat \
	--enable-qed \
	--enable-parallels \
	--disable-nettle \
	--disable-gcrypt \
	--disable-malloc-trim \
	--disable-gettext \
	--disable-selinux \
	--disable-numa \
	--disable-mpath \
	--disable-libiscsi \
	--disable-libnfs \
	--disable-libssh \
	--disable-rbd \
	--disable-rdma \
	--disable-snappy \
	--disable-lzfse \
	--disable-vde \
	--disable-netmap \
	--disable-blkio \
	--disable-libdaxctl \
	--disable-libpmem \
	--disable-qpl \
	--disable-uadk \
	--disable-qatzip \
	--disable-af-xdp \
	--disable-igvm \
	--disable-usb-redir \
	--disable-smartcard \
	--disable-u2f \
	--disable-canokey \
	--disable-spice-protocol \
	--disable-rutabaga-gfx \
	--disable-oss \
	--disable-sparse \
	--disable-auth-pam \
	--disable-brlapi \
	--disable-libcbor \
	--disable-valgrind \
	--disable-xkbcommon \
	--disable-libkeyutils \
	--enable-seccomp \
	--enable-alsa \
	--disable-pa \
	--enable-pipewire \
	--disable-jack \
	--disable-sndio \
	--disable-gtk \
	--disable-sdl \
	--disable-sdl-image \
	--disable-opengl \
	--disable-virglrenderer \
	--disable-spice \
	--disable-vte \
	--disable-xen \
	--disable-docs \
	--disable-guest-agent \
	--disable-werror \
	--disable-install-blobs \
	--disable-containers \
	--disable-download
make
make DESTDIR=$PKG install

# THE GUEST FIRMWARE, BUILT FROM roms/ — the sources the tarball carries for
# every image in pc-bios/. Only what the three targets boot by default or
# through their firmware descriptors is built. The boot ROMs of the Nuvoton
# and ASPEED BMC boards inside qemu-system-aarch64 are not, and those boards
# stop at startup naming the file they could not find unless -bios names one.
#
# The firmware is freestanding code for the guest, laid out by its own linker
# scripts, and takes no flags from the host build: qboot's meson would
# otherwise compile the host's -fPIC into a real-mode BIOS and pass its
# --build-id to the link. CC keeps -std=gnu11 because the PCCTS parser
# generator in edk2's BaseTools and iPXE's drivers are pre-C23 C that
# declares functions with empty parentheses and calls them with arguments,
# which C23 makes an error.
fw=$PKG/usr/share/qemu
install -d "$fw/firmware"
(
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
export CC="gcc -std=gnu11"

# SeaBIOS and SeaVGABIOS: bios-256k.bin is what pc and q35 load, bios.bin
# the 128 KiB image for old machine types, bios-microvm.bin microvm's; one
# vgabios per emulated display adapter. qboot is microvm's fast-boot BIOS.
make -C roms bios vgabios qboot FIRMWARE_EXTRAVERSION=-kdos

# iPXE: every emulated NIC loads its option ROM at startup and a missing one
# stops qemu, so these are required, not optional. efi-*.rom is the BIOS
# image and the x86_64 UEFI driver in one file (joined by edk2's EfiRom) and
# is the default romfile; pxe-*.rom is the BIOS-only image old machine types
# use. NO_WERROR because gcc's array-bounds analysis flags iPXE's copies of
# 6-byte MAC addresses through 16-bit words. The patch lets GNU as 2.41 and
# later assemble iPXE's shared x86 sources for x86_64.
patch -p1 -i "$PORT_SRC/ipxe-arch-i386.patch"
make -C roms pxerom efirom PYTHON=python3 NO_WERROR=1 \
	EXTRA_CFLAGS=-std=gnu11 BUILD_TIMESTAMP="$SOURCE_DATE_EPOCH"

# The option ROMs -kernel boots through. configure builds these only when a
# probe links a -m32 object against a 32-bit libgcc, which a gcc built with
# --disable-multilib does not have; the ROMs themselves link -nostdlib and
# need none, so they are built here from qemu's own makefile.
mkdir -p optionrom-build
printf '%s\n' "TOPSRC_DIR=$SRC" 'CC=gcc' 'OBJCOPY=objcopy' 'PYTHON=python3' \
	> optionrom-build/config.mak
make -C optionrom-build -f "$SRC/pc-bios/optionrom/Makefile"

# OVMF for x86_64, plain and Secure Boot with SMM, and ArmVirtQemu for
# aarch64, through qemu's own edk2 driver script and with its build options.
# The config names only the three builds: qemu's own lists every platform,
# and its padding step calls truncate --size, which toybox does not accept.
# aarch64 is compiled by clang, which is a cross compiler for every target
# the llvm port builds; x86_64 by the native gcc. iasl (acpica) compiles the
# ACPI tables and nasm the x86 assembly. MAKEFLAGS is dropped for this step:
# edk2's build runs one make per module on its own threads, and a -j
# inherited into those makefiles races a module's .efi against the firmware
# volume that packs it ("No rule to make target ... .efi").
while read -r key _ value; do
	printf -v "$key" '%s' "$value"
done < roms/edk2-version
cat > roms/kdos-edk2.config <<'EDK2'
[global]
core = edk2

[opts.common]
NETWORK_HTTP_BOOT_ENABLE       = TRUE
NETWORK_IP6_ENABLE             = TRUE
NETWORK_TLS_ENABLE             = TRUE
NETWORK_ISCSI_ENABLE           = TRUE
NETWORK_ALLOW_HTTP_CONNECTIONS = TRUE
TPM2_ENABLE                    = TRUE
TPM2_CONFIG_ENABLE             = TRUE
TPM1_ENABLE                    = TRUE
CAVIUM_ERRATUM_27456           = TRUE

[opts.ovmf.sb.smm]
SECURE_BOOT_ENABLE = TRUE
SMM_REQUIRE        = TRUE
BUILD_SHELL        = FALSE

[opts.armvirt.silent]
DEBUG_PRINT_ERROR_LEVEL = 0x80000000

[pcds.nx.broken.shim.grub]
PcdDxeNxMemoryProtectionPolicy = 0xC000000000007FD1
PcdUninstallMemAttrProtocol    = TRUE

[build.ovmf.x86_64]
conf = OvmfPkg/OvmfPkgX64.dsc
arch = X64
opts = common
plat = OvmfX64
dest = ../pc-bios
cpy1 = FV/OVMF_CODE.fd edk2-x86_64-code.fd
cpy2 = FV/OVMF_VARS.fd edk2-i386-vars.fd

[build.ovmf.x86_64.secure]
conf = OvmfPkg/OvmfPkgX64.dsc
arch = X64
opts = common
       ovmf.sb.smm
plat = OvmfX64
dest = ../pc-bios
cpy1 = FV/OVMF_CODE.fd edk2-x86_64-secure-code.fd

[build.armvirt.aa64]
tool = CLANGDWARF
conf = ArmVirtPkg/ArmVirtQemu.dsc
arch = AARCH64
opts = common
       armvirt.silent
pcds = nx.broken.shim.grub
plat = ArmVirtQemu-AARCH64
dest = ../pc-bios
cpy1 = FV/QEMU_EFI.fd  edk2-aarch64-code.fd
cpy2 = FV/QEMU_VARS.fd edk2-arm-vars.fd
EDK2
cd roms
env -u MAKEFLAGS CLANGDWARF_BIN=/usr/bin/ python3 edk2-build.py --config kdos-edk2.config \
	--version-override "$EDK2_STABLE-kdos" --release-date "$EDK2_DATE" \
	--silent --no-logs
cd ..

# The aarch64 virt machine maps both flash banks at 64 MiB and refuses an
# image of any other size on -drive if=pflash.
truncate -s 64M pc-bios/edk2-aarch64-code.fd pc-bios/edk2-arm-vars.fd

# OpenSBI, the M-mode firmware every riscv machine loads by default. LLVM=1
# builds it with clang and ld.lld: the riscv64-unknown-elf binutils cannot
# link the position-independent image OpenSBI requires. qemu-system-riscv64
# also runs rv32 CPUs, and for one it loads the 32-bit image instead, so both
# widths are built, each into its own output directory. O= is created first
# and passed absolute: the makefile resolves it with readlink -f, and a
# readlink that prints nothing for a missing path puts the build under /.
for xlen in 64 32; do
	mkdir -p "$SRC/opensbi-rv$xlen"
	make -C roms/opensbi LLVM=1 PLATFORM=generic PLATFORM_RISCV_XLEN=$xlen \
		O="$SRC/opensbi-rv$xlen"
	cp "$SRC/opensbi-rv$xlen/platform/generic/firmware/fw_dynamic.bin" \
		"pc-bios/opensbi-riscv$xlen-generic-fw_dynamic.bin"
done
)

# The x86_64 variable store is named edk2-i386-vars.fd because the i386 and
# x86_64 OVMF builds share one layout and the descriptors below say so.
for f in bios.bin bios-256k.bin bios-microvm.bin qboot.rom \
	vgabios.bin vgabios-stdvga.bin vgabios-cirrus.bin vgabios-vmware.bin \
	vgabios-qxl.bin vgabios-virtio.bin vgabios-bochs-display.bin \
	vgabios-ramfb.bin vgabios-ati.bin \
	pxe-e1000.rom pxe-eepro100.rom pxe-ne2k_pci.rom pxe-pcnet.rom \
	pxe-rtl8139.rom pxe-virtio.rom \
	efi-e1000.rom efi-e1000e.rom efi-eepro100.rom efi-ne2k_pci.rom \
	efi-pcnet.rom efi-rtl8139.rom efi-virtio.rom efi-vmxnet3.rom \
	edk2-x86_64-code.fd edk2-x86_64-secure-code.fd edk2-i386-vars.fd \
	edk2-aarch64-code.fd edk2-arm-vars.fd \
	opensbi-riscv64-generic-fw_dynamic.bin \
	opensbi-riscv32-generic-fw_dynamic.bin edk2-licenses.txt; do
	install -m644 "pc-bios/$f" "$fw/$f"
done
install -m644 optionrom-build/kvmvapic.bin optionrom-build/linuxboot_dma.bin \
	optionrom-build/multiboot_dma.bin optionrom-build/pvh.bin "$fw/"

# The one image taken as upstream built it: EDK2 for the riscv64 virt
# machine. Compiled by a riscv64 bare-metal gcc 15 it faults in DXE, before
# any boot option, where upstream's image under the same OpenSBI reaches its
# shell; this tree's riscv64-unknown-elf gcc has not been tried. See the
# firmware exception in docs/kdos/01-philosophy/why-kdos.md.
for f in edk2-riscv-code.fd edk2-riscv-vars.fd; do
	bzip2 -dc "pc-bios/$f.bz2" > "$fw/$f"
	chmod 644 "$fw/$f"
done

# The firmware descriptors are how libvirt and other front ends find a UEFI
# image and its variable-store template for a machine; configure_file fills
# @DATADIR@ only when it installs blobs.
for f in 50-edk2-x86_64-secure 60-edk2-x86_64 60-edk2-aarch64 60-edk2-riscv64; do
	d=$(<"pc-bios/descriptors/$f.json")
	printf '%s\n' "${d//@DATADIR@//usr/share/qemu}" > "$fw/firmware/$f.json"
done
