#!/bin/bash
set -e
source script/phase3.env.sh

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

cat > $ISO_ROOT/EFI/BOOT/refind.conf <<EOF
timeout 5
textonly
showtools reboot, shutdown, firmware

menuentry "KDOS Live" {
    loader /EFI/BOOT/vmlinuz
    initrd /EFI/BOOT/initramfs.cpio.gz
    options "root=/dev/ram0 rw console=ttyS0 loglevel=3 quiet earlyprintk=serial,ttyS0"
    icon /EFI/BOOT/icons/os_linux.png
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
