#!/bin/bash
set -e
source script/phase3.env.sh

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
set -x
export PATH=/bin

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
/bin/mdev -s

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
    mount -t squashfs /mnt/iso/system.sfs /mnt/system
    
    # Setup OverlayFS
    mkdir -p /mnt/overlay/upper /mnt/overlay/work /newroot
    mount -t overlay overlay -o lowerdir=/mnt/system,upperdir=/mnt/overlay/upper,workdir=/mnt/overlay/work /newroot
    
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
