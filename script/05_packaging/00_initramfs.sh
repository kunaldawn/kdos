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

echo "Building Initramfs..."

# Setup workspace
INITRAMFS=/kdos/build/initramfs
rm -rf $INITRAMFS
mkdir -p $INITRAMFS
cd $INITRAMFS

# Create Directory Structure
mkdir -p bin dev proc sys run mnt/iso newroot etc lib boot

# Install Basic Config
cp /etc/passwd etc/passwd
cp /etc/group etc/group

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

# Install blkid and dependencies
cp /usr/bin/blkid bin/blkid
cp /usr/lib/libblkid.so.1 lib/libblkid.so.1
cp /usr/lib/libuuid.so.1 lib/libuuid.so.1

# Install eudev and dependencies
cp /sbin/udevd bin/udevd
cp /sbin/udevadm bin/udevadm
cp /usr/lib/libudev.so.1 lib/libudev.so.1
cp /usr/lib/libkmod.so.2 lib/libkmod.so.2
cp /usr/lib/liblzma.so.5 lib/liblzma.so.5
cp /usr/lib/libz.so.1 lib/libz.so.1
cp /usr/lib/libzstd.so.1 lib/libzstd.so.1

# Install udev rules and helpers
mkdir -p lib/udev/rules.d etc/udev/rules.d
cp -r /usr/lib/udev/rules.d/* lib/udev/rules.d/ 2>/dev/null || true
cp -r /etc/udev/rules.d/* etc/udev/rules.d/ 2>/dev/null || true
cp -r /usr/lib/udev/* lib/udev/ 2>/dev/null || true

# Install kmod and overlay module
cp /usr/bin/kmod bin/kmod
ln -sf kmod bin/modprobe
ln -sf kmod bin/insmod
ln -sf kmod bin/depmod

# Detect Kernel Version & Copy necessary modules
KERNEL_VER=$(ls /lib/modules | sort -V | tail -n 1)
if [ -z "$KERNEL_VER" ]; then
    echo "Error: No kernel modules found in /lib/modules!"
    exit 1
fi
echo "Using Kernel Version: $KERNEL_VER"
MOD_DIR=lib/modules/$KERNEL_VER
mkdir -p $MOD_DIR

# Function to copy module and dependencies recursively
run_depmod() {
    if [ ! -f /lib/modules/$KERNEL_VER/modules.dep ]; then
        echo "Running depmod..."
        depmod -a $KERNEL_VER
    fi
}
run_depmod

copy_module() {
    local MOD=$1
    
    # Check if builtin
    if grep -q -w "$MOD" /lib/modules/$KERNEL_VER/modules.builtin 2>/dev/null; then
        echo "Module $MOD is builtin, skipping."
        return
    fi
    
    local MOD_PATH=$(modinfo -k $KERNEL_VER -n $MOD 2>/dev/null)
    
    # Fallback if modinfo fails (e.g. for .zst or if depmod is stale)
    if [ -z "$MOD_PATH" ] || [ "$MOD_PATH" = "(builtin)" ]; then
        MOD_PATH=$(find /lib/modules/$KERNEL_VER -name "$MOD.ko*" -print -quit)
    fi

    if [ -z "$MOD_PATH" ]; then
        echo "Warning: Module $MOD not found"
        return
    fi
    
    # Destination path structure (strip /lib/modules/VER/)
    local REL_PATH=${MOD_PATH#*/lib/modules/$KERNEL_VER/}
    local DEST=$MOD_DIR/$REL_PATH
    
    # If .zst, update destination to .ko
    if [[ "$DEST" == *.zst ]]; then
        DEST=${DEST%.zst}
    fi
    
    if [ -e "$DEST" ]; then
        return
    fi
    
    # Create directory
    mkdir -p $(dirname $DEST)
    
    # Copy and decompress if needed
    if [[ "$MOD_PATH" == *.zst ]]; then
        echo "Copying and decompressing $MOD..."
        zstd -d -c "$MOD_PATH" > "$DEST"
    else
        echo "Copying $MOD..."
        cp "$MOD_PATH" "$DEST"
    fi
    
    # Recursively copy dependencies
    local DEPS=$(modinfo -k $KERNEL_VER -F depends "$MOD_PATH" 2>/dev/null | tr ',' ' ')
    for DEP in $DEPS; do
        copy_module $DEP
    done
}

# Core Modules for Booting (Storage, FS, Input, etc.)
MODULES="overlay squashfs isofs cdrom sr_mod loop sd_mod ata_piix ahci libahci virtio virtio_blk virtio_pci virtio_scsi xhci-pci xhci-hcd ehci-pci ehci-hcd ohci-pci ohci-hcd usb-storage uas"

for MOD in $MODULES; do
    copy_module $MOD
done

# Copy modules.order and modules.builtin for depmod
cp /lib/modules/$KERNEL_VER/modules.order $MOD_DIR/
cp /lib/modules/$KERNEL_VER/modules.order $MOD_DIR/
cp /lib/modules/$KERNEL_VER/modules.builtin $MOD_DIR/
if [ -f /lib/modules/$KERNEL_VER/modules.builtin.modinfo ]; then
    cp /lib/modules/$KERNEL_VER/modules.builtin.modinfo $MOD_DIR/
fi

# Copy System.map for depmod
if [ -f /boot/System.map-$KERNEL_VER ]; then
    cp /boot/System.map-$KERNEL_VER $INITRAMFS/boot/System.map-$KERNEL_VER
fi

# Regenerate module dependencies for the initramfs
echo "Generating dependency map..."
if [ -f boot/System.map-$KERNEL_VER ]; then
    depmod -b . -F boot/System.map-$KERNEL_VER $KERNEL_VER
else
    depmod -b . $KERNEL_VER
fi

# Create Init Script
cat > init <<EOF
#!/bin/bash
export PATH=/bin

# Redirect stdout/stderr to console
exec >/dev/console 2>&1

echo "KDOS Init Starting..."

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts

# Populate /dev
echo "Populating /dev..."
udevd --daemon
echo "Triggering udev events..."
udevadm trigger --type=subsystems --action=add
udevadm trigger --type=devices --action=add
udevadm settle

echo "Loading essential filesystem modules..."
modprobe -v loop || echo "Modprobe loop failed"
modprobe -v isofs || echo "Modprobe isofs failed"
modprobe -v squashfs || echo "Modprobe squashfs failed"
modprobe -v overlay || echo "Modprobe overlay failed"

# Check for loop device (create if missing)
if [ ! -e /dev/loop0 ]; then
    mknod /dev/loop0 b 7 0
fi

echo "Welcome to KDOS"

# Parse Boot Parameters
for i in \$(cat /proc/cmdline); do
    case "\$i" in
        root=UUID=*)
            ROOT_UUID="\${i#root=UUID=}"
            ;;
    esac
done

if [ -n "\$ROOT_UUID" ]; then
    # Disk Boot Mode
    echo "Waiting for root device \$ROOT_UUID..."
    
    # Wait for device to appear (timeout 10s)
    for i in \$(seq 1 10); do
        ROOT_DEV=\$(blkid -U "\$ROOT_UUID")
        if [ -n "\$ROOT_DEV" ]; then
            break
        fi
        sleep 1
    done
    
    if [ -n "\$ROOT_DEV" ]; then
        echo "Found root device: \$ROOT_DEV"
        mount "\$ROOT_DEV" /newroot
        
        if [ -x /newroot/sbin/init ]; then
            # Move Mountpoints
            mount --move /dev /newroot/dev
            mount --move /proc /newroot/proc
            mount --move /sys /newroot/sys

            # Switch Root
            echo "Switching root..."
            exec switch_root /newroot /sbin/init
        else
            echo "Error: /sbin/init not found on root device!"
        fi
    else
        echo "Error: Root device with UUID=\$ROOT_UUID not found!"
    fi
    
    # Fallback to shell if disk boot fails
    echo "Disk boot failed. Dropping to shell..."
    exec /bin/sh
fi

# Live ISO Boot Mode (Fallback)
mkdir -p /mnt/iso
echo "Searching for KDOS boot media..."

# Try to mount CDROM/ISO
# Wait a bit for devices to settle
sleep 2

FOUND=0
for dev in /dev/sr* /dev/sd* /dev/vd* /dev/nvme*; do
    [ -e "\$dev" ] || continue
    echo "Checking \$dev..."
    if mount -t iso9660 "\$dev" /mnt/iso; then
        if [ -f /mnt/iso/system.sfs ]; then
            echo "Found KDOS media on \$dev"
            FOUND=1
            break
        fi
        umount /mnt/iso
    fi
done

if [ "\$FOUND" == "1" ]; then
    echo "Found KDOS Media, Mounting system..."
    
    # Mount System SquashFS using explicit loop
    mkdir -p /mnt/system
    
    # Find a free loop device
    LOOPDEV=\$(losetup -f)
    if [ -z "\$LOOPDEV" ]; then
        LOOPDEV=/dev/loop0
        [ -e /dev/loop0 ] || mknod /dev/loop0 b 7 0
    fi
    
    echo "Associating \$LOOPDEV with /mnt/iso/system.sfs..."
    # Force read-only (-r) to avoid "Read-only file system" error
    if losetup -r "\$LOOPDEV" /mnt/iso/system.sfs; then
        echo "Mounting \$LOOPDEV to /mnt/system..."
        if mount -t squashfs -o ro "\$LOOPDEV" /mnt/system; then
             echo "System mounted successfully."
             
             # Setup OverlayFS
             mkdir -p /mnt/overlay
             mount -t tmpfs tmpfs /mnt/overlay
             mkdir -p /mnt/overlay/upper /mnt/overlay/work /newroot
             
             echo "Mounting OverlayFS..."
             modprobe overlay
             mount -t overlay overlay -o lowerdir=/mnt/system,upperdir=/mnt/overlay/upper,workdir=/mnt/overlay/work /newroot
             
             # Check if switch root dir is valid
             if [ ! -d "/newroot" ]; then 
                echo "Error: /newroot is not a directory"
                exec /bin/sh
             fi

            # Create missing mountpoints in newroot
            mkdir -p /newroot/dev /newroot/proc /newroot/sys /newroot/run /newroot/tmp
            mkdir -p /newroot/mnt/iso /newroot/mnt/system /newroot/mnt/overlay

            # Move Mountpoints
            mount --move /dev /newroot/dev
            mount --move /proc /newroot/proc
            mount --move /sys /newroot/sys
            
            # Move Backing Mounts
            mount --move /mnt/iso /newroot/mnt/iso
            mount --move /mnt/system /newroot/mnt/system
            mount --move /mnt/overlay /newroot/mnt/overlay
            
            # Switch Root
            echo "Switching to new root..."
            if [ -x /newroot/sbin/init ]; then
                # Stop udevd
                udevadm control --exit
                exec switch_root /newroot /sbin/init
            else
                echo "Error: /sbin/init not found in new root!"
                ls -l /newroot/sbin/init
                exec /bin/sh
            fi
        else
            echo "Failed to mount system.sfs"
            exec /bin/sh
        fi
    else
        echo "Failed to setup loop device for system.sfs"
        exec /bin/sh
    fi
else
    echo "Failed to find KDOS installation media."
    exec /bin/sh
fi

echo "Boot failed. dropping to shell"
exec /bin/sh
EOF
chmod +x init

# Pack Initramfs
find . | cpio -o -H newc | gzip -9 > ../initramfs.cpio.gz
echo "Initramfs created at $INITRAMFS"

ls /kdos/build