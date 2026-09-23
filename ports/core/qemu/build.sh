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
# framebuffer is watched from any viewer — which is exactly how this project's
# own testing/vnc-shot.py already drives a VM. GTK is the hard rule. SDL is off
# because meson.build looks up x11 with no option to stop it and links libX11
# into the SDL front end whenever that library is installed, which it is for
# Xwayland. --disable-opengl because with GTK, SDL and virglrenderer (not a
# port) off, GL adds only the egl-headless display and GL scanout on the D-Bus
# display, and costs mesa plus libepoxy, whose port depends on libX11.
#
# --disable-docs because docs/conf.py requires sphinx_rtd_theme even for the
# man pages, and that theme is not a port.
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
	--disable-capstone \
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
	--disable-download
make
make DESTDIR=$PKG install
