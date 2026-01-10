#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#    KDOS – forged by hand.
#    KD's Homebrew OS
# ---------------------------------

set -e
source script/packaging.env.sh

echo "Building Initramfs..."

# Setup workspace
INITRAMFS=/kdos/build/initramfs
rm -rf $INITRAMFS
mkdir -p $INITRAMFS
cd $INITRAMFS

# Create Directory Structure
mkdir -p bin dev proc sys run mnt/iso newroot etc lib

# Install Toybox
cp /usr/bin/toybox bin/toybox
chmod +x bin/toybox
for cmd in $(./bin/toybox); do
    [ "$cmd" != "toybox" ] && ln -sf toybox bin/$cmd
done

# Install Libc
cp /usr/lib/libc.so lib/libc.so
ln -sf libc.so lib/ld-musl-x86_64.so.1

# Install Bash and Dependencies
cp /usr/bin/bash bin/bash
cp /usr/lib/libreadline.so.8 lib/libreadline.so.8
cp /usr/lib/libhistory.so.8 lib/libhistory.so.8
cp /usr/lib/libncursesw.so.6 lib/libncursesw.so.6
ln -sf bash bin/sh

# Create Init Script
cat > init <<EOF
#!/bin/bash
export PATH=/bin

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts

# Check for loop device (create if missing)
if [ ! -e /dev/loop0 ]; then
    mknod /dev/loop0 b 7 0
fi

echo "Welcome to KDOS"

# Find Boot Media
mkdir -p /mnt/iso
# Try to mount CDROM/ISO
for dev in /dev/sr* /dev/sd*; do
    if mount -t iso9660 \$dev /mnt/iso 2>/dev/null; then
        if [ -f /mnt/iso/system.sfs ]; then
            FOUND=1
            break
        fi
        umount /mnt/iso
    fi
done

if [ "\$FOUND" == "1" ]; then
    echo "Found KDOS Media, Mounting system..."
    
    # Mount System SquashFS
    mkdir -p /mnt/system
    mount -t squashfs -o ro,loop /mnt/iso/system.sfs /mnt/system
    
    # Setup OverlayFS
    # Mount tmpfs for overlay storage to ensure support for OverlayFS requirements
    mkdir -p /mnt/overlay
    mount -t tmpfs tmpfs /mnt/overlay
    mkdir -p /mnt/overlay/upper /mnt/overlay/work /newroot
    mount -t overlay overlay -o lowerdir=/mnt/system,upperdir=/mnt/overlay/upper,workdir=/mnt/overlay/work /newroot
    
    # Create missing mountpoints in newroot (since they might be excluded in squashfs)
    mkdir -p /newroot/dev /newroot/proc /newroot/sys /newroot/run /newroot/tmp
    
    # Move Mountpoints
    mount --move /dev /newroot/dev
    mount --move /proc /newroot/proc
    mount --move /sys /newroot/sys
    
    # Switch Root
    exec switch_root /newroot /sbin/init
else
    echo "Failed to find KDOS installation media."
    exec /bin/sh
fi
EOF
chmod +x init

# Pack Initramfs
find . | cpio -o -H newc | gzip -9 > ../initramfs.cpio.gz
echo "Initramfs created at $INITRAMFS"

ls /kdos/build
