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

set -e
source script/packaging.env.sh

echo "Building KDOS ISO..."

ISO_ROOT=/kdos/build/iso_root
rm -rf $ISO_ROOT
mkdir -p $ISO_ROOT

# 1. Copy Kernel and Initramfs (to EFI/BOOT for ESP)
mkdir -p $ISO_ROOT/EFI/BOOT

if [ -f /boot/vmlinuz-kdos ]; then
    cp /boot/vmlinuz-kdos $ISO_ROOT/EFI/BOOT/vmlinuz
else
    echo "Error: /boot/vmlinuz-kdos not found!"
    exit 1
fi

if [ -f /kdos/build/initramfs.cpio.gz ]; then
    cp /kdos/build/initramfs.cpio.gz $ISO_ROOT/EFI/BOOT/initramfs.cpio.gz
    # Also copy to /boot so it is included in system.sfs (and thus installed)
    cp /kdos/build/initramfs.cpio.gz /boot/initramfs.cpio.gz
else
    echo "Error: /kdos/build/initramfs.cpio.gz not found!"
    exit 1
fi

# 2. Create System SquashFS
echo "Squashing Root Filesystem..."
# Exclude pseudo filesystems, build artifacts, and caches
mksquashfs / $ISO_ROOT/system.sfs \
    -e proc sys dev tmp run mnt media var/cache var/log build kdos ports \
    -p "proc d 555 0 0" \
    -p "sys d 555 0 0" \
    -p "dev d 755 0 0" \
    -p "tmp d 1777 0 0" \
    -p "run d 755 0 0" \
    -p "mnt d 755 0 0" \
    -p "media d 755 0 0" \
    -noappend -comp xz

# 2a. The packs, on ISO9660 BESIDE system.sfs rather than inside it.
#
# They are already compressed and squashing them again buys nothing, and on the
# medium they are readable from /mnt/iso the moment the live image is up — so a
# live stick carries every application while an installed system carries what
# somebody chose. That is the same argument the sources below are placed by,
# and it is what makes `kdos app install` on a live session a MOUNT rather than
# a copy.
#
# The store excludes /var/lib/kdos/packs for the same reason: a pack that is on
# the medium must not also be inside the squashfs, or every install pays for it
# twice.
if [ -d /kdos/ports/appbox/packs ] && \
   ls /kdos/ports/appbox/packs/*.kpack >/dev/null 2>&1; then
    echo "Packs onto the medium..."
    mkdir -p $ISO_ROOT/packs
    cp -a /kdos/ports/appbox/packs/*.kpack $ISO_ROOT/packs/
    [ -f /kdos/ports/appbox/packs/PACKAGES ] && \
        cp -a /kdos/ports/appbox/packs/PACKAGES* $ISO_ROOT/packs/
    echo "Packs: $(ls $ISO_ROOT/packs/*.kpack | wc -l), $(du -sh $ISO_ROOT/packs | cut -f1)"
else
    echo "Packs: none baked — `make fetch-packs` builds them (needs a network)"
fi

# 2b. The sources, when asked for.
#
# N13: a booted stick that can rebuild its own ISO. The tree goes on the ISO9660
# filesystem BESIDE system.sfs rather than inside it, so it costs the installed
# system nothing and is readable from /mnt/iso the moment the live image is up.
#
# Opt-in because it roughly doubles the image: ports/ is 2.7 G of upstream
# tarballs that are already compressed, and squashing them again buys nothing.
# `make build KDOS_ISO_SOURCES=1` is a developer stick, not the default one.
if [ "${KDOS_ISO_SOURCES:-0}" = "1" ]; then
    echo "Copying the sources onto the ISO (this is the big one)..."
    mkdir -p $ISO_ROOT/sources
    for d in ports src script; do
        cp -a /kdos/$d $ISO_ROOT/sources/
    done
    for f in Makefile Dockerfile CLAUDE.md; do
        [ -f /kdos/$f ] && cp -a /kdos/$f $ISO_ROOT/sources/
    done
    # Build artefacts are not sources, and the appbox image chunks are already
    # in the payload the live system carries.
    rm -rf $ISO_ROOT/sources/ports/.portup-tools $ISO_ROOT/sources/ports/.kpkg-meta \
           $ISO_ROOT/sources/ports/.update-cache.json
    # A stamp, so `kdos rebuild` can say what it is about to rebuild FROM.
    cat > $ISO_ROOT/sources/SOURCES <<EOS
# The KDOS tree that built this image.
ports    $(ls /kdos/ports/core | wc -l) ports
size     $(du -sh $ISO_ROOT/sources 2>/dev/null | cut -f1)
built    $(date -u +%Y-%m-%dT%H:%M:%SZ)
EOS
    echo "Sources: $(du -sh $ISO_ROOT/sources | cut -f1)"
fi

# 3. Setup Bootloaders
## UEFI: rEFInd
echo "Configuring UEFI Boot..."
mkdir -p $ISO_ROOT/EFI/BOOT
REFIND_DIR=/usr/share/refind
if [ -d "$REFIND_DIR" ]; then
    cp $REFIND_DIR/refind_x64.efi $ISO_ROOT/EFI/BOOT/BOOTX64.EFI
    cp -r $REFIND_DIR/icons $ISO_ROOT/EFI/BOOT/
    cp -r $REFIND_DIR/drivers_x64 $ISO_ROOT/EFI/BOOT/drivers
else
    echo "Warning: rEFInd files not found at $REFIND_DIR"
fi

# The boot menu is the first pixel of KDOS, so it gets the phosphor treatment
# too. A banner needs graphics mode, which is why `textonly` is gone.
if [ -f /usr/share/kdos/boot/kdos-banner.png ]; then
    cp /usr/share/kdos/boot/kdos-banner.png $ISO_ROOT/EFI/BOOT/kdos-banner.png
else
    echo "Warning: KDOS boot banner not found — rEFInd will use its own"
fi

# The mascot as the OS selector icon, replacing rEFInd's stock tux.
OS_ICON=os_linux.png
if [ -f /usr/share/kdos/boot/os_kdos.png ]; then
    cp /usr/share/kdos/boot/os_kdos.png $ISO_ROOT/EFI/BOOT/icons/os_kdos.png
    OS_ICON=os_kdos.png
else
    echo "Warning: KDOS mascot icon not found — using rEFInd's tux"
fi

cat > $ISO_ROOT/EFI/BOOT/refind.conf <<EOF
timeout 5
banner /EFI/BOOT/kdos-banner.png
banner_scale noscale
hideui hints,badges
showtools reboot, shutdown, firmware
use_graphics_for linux

menuentry "KDOS Live" {
    loader /EFI/BOOT/vmlinuz
    initrd /EFI/BOOT/initramfs.cpio.gz
    options "root=/dev/ram0 rw console=tty0 console=ttyS0 quiet loglevel=3"
    icon /EFI/BOOT/icons/$OS_ICON
}

menuentry "KDOS Live (verbose)" {
    loader /EFI/BOOT/vmlinuz
    initrd /EFI/BOOT/initramfs.cpio.gz
    options "root=/dev/ram0 rw console=tty0 console=ttyS0 loglevel=7"
    icon /EFI/BOOT/icons/$OS_ICON
}
EOF

# 4. Create EFI Boot Image
echo "Creating EFI Boot Image..."
ISO_BUILD=/kdos/build/iso-build
mkdir -p $ISO_BUILD
dd if=/dev/zero of=$ISO_BUILD/efiboot.img bs=1M count=256
mkfs.fat -F 32 -n "KDOS_EFI" $ISO_BUILD/efiboot.img
mmd -i $ISO_BUILD/efiboot.img ::EFI
mmd -i $ISO_BUILD/efiboot.img ::EFI/BOOT
mcopy -i $ISO_BUILD/efiboot.img -s $ISO_ROOT/EFI/BOOT/* ::EFI/BOOT/
cp $ISO_BUILD/efiboot.img $ISO_ROOT/EFI/efiboot.img

# 5. Generate ISO
echo "Generating ISO..."
xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "KDOS_LIVE" \
    -e EFI/efiboot.img \
    -no-emul-boot -isohybrid-gpt-basdat \
    -o $ISO_BUILD/kdos.iso \
    $ISO_ROOT

echo "ISO Construction Complete: $ISO_BUILD/kdos.iso"
