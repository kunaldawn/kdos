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
	--enable-libiscsi \
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
	--enable-usb-redir \
	--enable-smartcard \
	--disable-u2f \
	--disable-canokey \
	--enable-spice-protocol \
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
	--enable-gtk \
	--enable-vte \
	--enable-sdl \
	--disable-sdl-image \
	--enable-opengl \
	--disable-virglrenderer \
	--enable-spice \
	--disable-xen \
	--disable-docs \
	--disable-debug-info \
	--disable-guest-agent \
	--disable-werror \
	--disable-install-blobs \
	--disable-containers \
	--disable-download
make
make DESTDIR=$PKG install

fw=$PKG/usr/share/qemu
install -d "$fw/firmware"
(
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
export CC="gcc -std=gnu11" EXTRA_OPTFLAGS=-Wno-error

make -C roms bios vgabios qboot PYTHON=python3 FIRMWARE_EXTRAVERSION=-kdos

patch -p1 -i "$PORT_SRC/ipxe-arch-i386.patch"
patch -p1 -i "$PORT_SRC/edk2-gcc-no-werror.patch"
make -C roms pxerom efirom PYTHON=python3 NO_WERROR=1 \
	EXTRA_CFLAGS=-std=gnu11 BUILD_TIMESTAMP="$SOURCE_DATE_EPOCH"

mkdir -p optionrom-build
printf '%s\n' "TOPSRC_DIR=$SRC" 'CC=gcc' 'OBJCOPY=objcopy' 'PYTHON=python3' \
	> optionrom-build/config.mak
make -C optionrom-build -f "$SRC/pc-bios/optionrom/Makefile"

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

truncate -s 64M pc-bios/edk2-aarch64-code.fd pc-bios/edk2-arm-vars.fd

for xlen in 64 32; do
	mkdir -p "$SRC/opensbi-rv$xlen"
	make -C roms/opensbi LLVM=1 PLATFORM=generic PLATFORM_RISCV_XLEN=$xlen \
		REPRODUCIBLE_FLAGS=-Wno-error O="$SRC/opensbi-rv$xlen"
	cp "$SRC/opensbi-rv$xlen/platform/generic/firmware/fw_dynamic.bin" \
		"pc-bios/opensbi-riscv$xlen-generic-fw_dynamic.bin"
done
)

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

for f in edk2-riscv-code.fd edk2-riscv-vars.fd; do
	bzip2 -dc "pc-bios/$f.bz2" > "$fw/$f"
	chmod 644 "$fw/$f"
done

for f in 50-edk2-x86_64-secure 60-edk2-x86_64 60-edk2-aarch64 60-edk2-riscv64; do
	d=$(<"pc-bios/descriptors/$f.json")
	printf '%s\n' "${d//@DATADIR@//usr/share/qemu}" > "$fw/firmware/$f.json"
done
