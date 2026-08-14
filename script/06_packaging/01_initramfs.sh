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

# Install util-linux switch_root, replacing toybox's.
# toybox switch_root only wipes the initramfs and chroot()s -- it never does
# mount(newroot, "/", MS_MOVE). That leaves the mount-namespace root as the
# (now empty) initramfs rootfs with the real root parked at /newroot, so any
# process that JOINS a mount namespace via setns() -- podman exec, distrobox
# enter, nsenter -m -- gets the empty rootfs as "/" and every path is ENOENT.
rm -f bin/switch_root
cp /usr/sbin/switch_root bin/switch_root

# Install the boot splash. Static, so it needs nothing else here, and it keeps
# running across switch_root: its FIFO lives in /dev (devtmpfs is moved into the
# new root, not remounted), so one process spans the initramfs and the real root
# without the screen ever going black between them.
if [ -x /usr/bin/kdos-splash ]; then
    cp /usr/bin/kdos-splash bin/kdos-splash
    mkdir -p usr/share/kdos
    cp /usr/share/kdos/splash.psf usr/share/kdos/splash.psf
else
    echo "Warning: kdos-splash not installed — booting without the splash"
fi

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

# Install kdos-bootctl, which decides WHICH root to boot when the machine has
# two. rEFInd cannot count boots — that is a systemd-boot feature — so the
# counting is ours and it has to happen here rather than in rcS: a kernel that
# boots into a wedged userland must still spend an attempt.
if [ -x /usr/bin/kdos-bootctl ]; then
    cp /usr/bin/kdos-bootctl bin/kdos-bootctl
elif [ -x /usr/bin/kdos-tools ]; then
    cp /usr/bin/kdos-tools bin/kdos-bootctl
else
    echo "Note: kdos-bootctl not installed — no A/B slot selection at boot"
fi

# Install cryptsetup, for an encrypted root.
#
# Copied WITH its libraries and skipped entirely when it is not installed: an
# initramfs that half-carries a cryptsetup is an initramfs that fails at the
# passphrase prompt instead of at build time. `ldd` is not available here, so
# the list is explicit — and if one is missing the boot says "cryptsetup: not
# found" rather than something subtler.
if [ -x /usr/sbin/cryptsetup ]; then
    cp /usr/sbin/cryptsetup bin/cryptsetup
    for _l in libcryptsetup.so.12 libdevmapper.so.1.02 libjson-c.so.5 \
              libpopt.so.0 libssl.so.3 libcrypto.so.3 libargon2.so.1 \
              libuuid.so.1 libblkid.so.1 libz.so.1; do
        [ -f /usr/lib/$_l ] && cp /usr/lib/$_l lib/$_l
    done
    HAVE_CRYPT=1
else
    echo "Note: cryptsetup not installed — the initramfs cannot unlock a LUKS root"
    HAVE_CRYPT=0
fi

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

# dm-crypt and the ciphers a LUKS2 default header actually uses. Carried
# unconditionally: they are small, and a kernel that has them built in makes
# copy_module a no-op anyway.
# vfat, so the initramfs can read the boot state off the ESP.
MODULES="$MODULES vfat nls_cp437 nls_iso8859-1"
# Root filesystems the installer can create. ext4 and btrfs are built in and
# copy_module is a no-op for them; xfs is CONFIG_XFS_FS=m, and an xfs root the
# initramfs cannot mount installs perfectly and never boots again.
MODULES="$MODULES xfs"
MODULES="$MODULES dm-crypt dm-mod aes_generic aes_x86_64 aesni-intel xts sha256_generic sha512_generic crypto_null algif_skcipher"

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
mkdir -p /dev/shm
mount -t tmpfs -o nosuid,nodev tmpfs /dev/shm

# The screen has been dark since the bootloader handed over: /dev/console is
# ttyS0 (the last console= wins), so none of these messages reach it, and fbcon
# defers taking the framebuffer until something prints to tty0. Nothing does.
# Paint it ourselves.
if [ -x /bin/kdos-splash ]; then
    /bin/kdos-splash run </dev/null >/dev/null 2>&1 &
fi
sp_step() { [ -x /bin/kdos-splash ] && /bin/kdos-splash step "\$1" 2>/dev/null; return 0; }
sp_ok()   { [ -x /bin/kdos-splash ] && /bin/kdos-splash ok 2>/dev/null; return 0; }
sp_fail() { [ -x /bin/kdos-splash ] && /bin/kdos-splash fail 2>/dev/null; return 0; }
sp_total() { [ -x /bin/kdos-splash ] && /bin/kdos-splash total "\$1" 2>/dev/null; return 0; }

# The two stages every boot path runs; each path adds its own share below.
sp_total 2

# Populate /dev
echo "Populating /dev..."
sp_step "DEVICE MANAGER"
udevd --daemon
echo "Triggering udev events..."
udevadm trigger --type=subsystems --action=add
udevadm trigger --type=devices --action=add
udevadm settle
sp_ok

echo "Loading essential filesystem modules..."
sp_step "FILESYSTEM MODULES"
modprobe -v loop || echo "Modprobe loop failed"
modprobe -v isofs || echo "Modprobe isofs failed"
modprobe -v squashfs || echo "Modprobe squashfs failed"
modprobe -v overlay || echo "Modprobe overlay failed"
sp_ok

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
        cryptdevice=*)
            CRYPTDEV="\${i#cryptdevice=}"
            ;;
        bootstate=UUID=*)
            BOOTSTATE_UUID="\${i#bootstate=UUID=}"
            ;;
    esac
done

#
# An encrypted root, unlocked before anything looks for the filesystem inside
# it. The syntax is Arch's, because it is the one already in people's heads:
#
#     cryptdevice=UUID=<luks-uuid>:<name>   root=UUID=<filesystem-uuid>
#
# The two UUIDs are DIFFERENT things and the distinction is the whole trap: the
# first is the LUKS container's, the second belongs to the filesystem that only
# exists once the container is open. kinstall writes both.
#
# The prompt goes through the SPLASH, not to /dev/console. console= is ttyS0 on
# this kernel command line (the last one wins), so a plain `read -p` prompts a
# serial port nobody is looking at while the screen shows a boot splash that
# appears to have frozen. The keystrokes are read from /dev/tty1, which is where
# the keyboard actually is.
#
# There is no per-keystroke feedback: the splash owns the framebuffer and bash
# owns the terminal, and a passphrase field that echoed dots would mean moving
# the read into the splash. Stated rather than hidden.
#
# Two knobs, both defaulted to the real thing. The keyboard is on tty1 because
# console= is the serial port, and the mapper directory is where the kernel puts
# an opened container. They are variables so `testing/selftest.sh` can exercise
# this function without a LUKS volume and without root — the same trick
# `kdos stutter --fixture` uses for /proc.
: "\${PASS_TTY:=/dev/tty1}"
: "\${CRYPT_MAPPER_DIR:=/dev/mapper}"

unlock_root() {
    local spec="\$1" luks_uuid name dev tries

    case "\$spec" in
        UUID=*:*)  luks_uuid="\${spec#UUID=}"; luks_uuid="\${luks_uuid%%:*}"
                   name="\${spec##*:}" ;;
        /dev/*:*)  dev="\${spec%%:*}"; name="\${spec##*:}" ;;
        *)         echo "cryptdevice: cannot parse '\$spec'"; return 1 ;;
    esac
    [ -n "\$name" ] || name=kdosroot

    if [ -z "\$dev" ]; then
        for i in \$(seq 1 10); do
            dev=\$(blkid -U "\$luks_uuid")
            [ -n "\$dev" ] && break
            sleep 1
        done
    fi
    if [ -z "\$dev" ] || [ ! -e "\$dev" ]; then
        echo "cryptdevice: no device for \$spec"
        return 1
    fi
    if [ ! -x /bin/cryptsetup ]; then
        echo "cryptdevice: this initramfs has no cryptsetup"
        return 1
    fi

    modprobe -q dm-crypt 2>/dev/null

    tries=0
    while [ \$tries -lt 3 ]; do
        tries=\$((tries + 1))
        [ -x /bin/kdos-splash ] && /bin/kdos-splash msg \
            "PASSPHRASE FOR \$name (attempt \$tries of 3)" 2>/dev/null
        # -s: never echo the passphrase. Read from tty1 because the keyboard
        # is there and /dev/console is the serial port.
        PASS=""
        read -r -s PASS < "\$PASS_TTY" || true
        # Fed on STDIN, never as an argument: /proc/<pid>/cmdline is readable
        # by every process on the machine for as long as the process lives.
        printf '%s' "\$PASS" | cryptsetup open --key-file=- "\$dev" "\$name"
        rc=\$?
        PASS=""
        if [ \$rc -eq 0 ] && [ -e "\$CRYPT_MAPPER_DIR/\$name" ]; then
            [ -x /bin/kdos-splash ] && /bin/kdos-splash msg "UNLOCKED" 2>/dev/null
            return 0
        fi
        [ -x /bin/kdos-splash ] && /bin/kdos-splash msg \
            "WRONG PASSPHRASE" 2>/dev/null
    done
    return 1
}

#
# A/B slots: the boot state lives on the ESP because it must be readable and
# WRITABLE before any root filesystem is mounted — including the one that turns
# out not to work. `select` prints the UUID to boot and spends an attempt in the
# same breath, so a kernel that hangs after this point has still been counted.
#
# Failing to read it is not fatal: `root=` on the command line is what a machine
# without A/B uses anyway, and it stays the fallback.
#
if [ -n "\$BOOTSTATE_UUID" ] && [ -x /bin/kdos-bootctl ]; then
    sp_total 1
    sp_step "BOOT SLOT"
    modprobe -q vfat 2>/dev/null
    mkdir -p /esp
    ESP_DEV=""
    for i in \$(seq 1 10); do
        ESP_DEV=\$(blkid -U "\$BOOTSTATE_UUID")
        [ -n "\$ESP_DEV" ] && break
        sleep 1
    done
    if [ -n "\$ESP_DEV" ] && mount -t vfat "\$ESP_DEV" /esp 2>/dev/null; then
        SEL=\$(KDOS_BOOTSTATE=/esp/EFI/kdos/bootstate \
               /bin/kdos-bootctl select 2>/dev/console)
        # Unmounted immediately: the root filesystem mounts it again at
        # /boot/efi, and two mounts of one FAT filesystem is how a state file
        # gets written twice and read once.
        umount /esp 2>/dev/null
        if [ -n "\$SEL" ]; then
            echo "Boot slot selected: \$SEL"
            ROOT_UUID="\$SEL"
            sp_ok
        else
            echo "No usable boot state; keeping root=\$ROOT_UUID"
            sp_ok
        fi
    else
        echo "Cannot read the boot state; keeping root=\$ROOT_UUID"
        sp_ok
    fi
fi

if [ -n "\$CRYPTDEV" ]; then
    # One extra stage on the progress bar, added here rather than up front:
    # the total is additive, and only this branch knows there is an unlock.
    sp_total 1
    sp_step "UNLOCKING"
    if unlock_root "\$CRYPTDEV"; then
        sp_ok
        # The filesystem inside the container has only just appeared, so the
        # udev pass that ran before the unlock never saw it.
        udevadm settle 2>/dev/null || true
    else
        sp_fail
        echo "Failed to unlock \$CRYPTDEV — dropping to a shell"
        exec /bin/sh
    fi
fi

if [ -n "\$ROOT_UUID" ]; then
    # Disk Boot Mode
    echo "Waiting for root device \$ROOT_UUID..."
    sp_total 3
    sp_step "ROOT DEVICE"

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
        sp_ok
        sp_step "MOUNTING ROOT"
        mount "\$ROOT_DEV" /newroot

        if [ -x /newroot/sbin/init ]; then
            sp_ok
            # Move Mountpoints
            mount --move /dev /newroot/dev
            mount --move /proc /newroot/proc
            mount --move /sys /newroot/sys

            # Switch Root
            echo "Switching root..."
            sp_step "SWITCHING ROOT"
            sp_ok
            exec switch_root /newroot /sbin/init
        else
            echo "Error: /sbin/init not found on root device!"
            sp_fail
        fi
    else
        echo "Error: Root device with UUID=\$ROOT_UUID not found!"
        sp_fail
    fi
    
    # Fallback to shell if disk boot fails
    echo "Disk boot failed. Dropping to shell..."
    exec /bin/sh
fi

# Live ISO Boot Mode (Fallback)
mkdir -p /mnt/iso
echo "Searching for KDOS boot media..."

# Announce the stage BEFORE the settle wait: with only the two common steps
# closed the bar would otherwise sit on "done" through the whole scan.
sp_total 4
sp_step "BOOT MEDIA"

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
    sp_ok
    sp_step "SYSTEM IMAGE"
    
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
             sp_ok

             # Setup OverlayFS
             sp_step "OVERLAY ROOT"
             mkdir -p /mnt/overlay
             mount -t tmpfs tmpfs /mnt/overlay
             mkdir -p /mnt/overlay/upper /mnt/overlay/work /newroot

             echo "Mounting OverlayFS..."
             modprobe overlay
             mount -t overlay overlay -o lowerdir=/mnt/system,upperdir=/mnt/overlay/upper,workdir=/mnt/overlay/work /newroot

             # Check if switch root dir is valid
             if [ ! -d "/newroot" ]; then
                echo "Error: /newroot is not a directory"
                sp_fail
                exec /bin/sh
             fi
             sp_ok

            # Create missing mountpoints in newroot
            mkdir -p /newroot/dev /newroot/proc /newroot/sys /newroot/run /newroot/tmp

            # Move pseudo-filesystems under the new root (standard; Alpine mkinitfs does this)
            mount --move /dev /newroot/dev
            mount --move /proc /newroot/proc
            mount --move /sys /newroot/sys

            # Backing mounts (squashfs /mnt/system, overlay tmpfs /mnt/overlay, ISO
            # /mnt/iso) stay in the old rootfs; switch_root leaves them alone (its
            # wipe skips anything on another device) and the overlay keeps an
            # internal kernel ref to the squashfs, so / stays valid.

            # Switch Root
            echo "Switching to new root..."
            sp_step "SWITCHING ROOT"
            if [ -x /newroot/sbin/init ]; then
                # Stop udevd
                udevadm control --exit
                # The splash keeps running through this: switch_root deletes the
                # old rootfs but not the processes living in it, and its FIFO is
                # on devtmpfs, which has just been moved into the new root.
                sp_ok
                exec switch_root /newroot /sbin/init
            else
                echo "Error: /sbin/init not found in new root!"
                ls -l /newroot/sbin/init
                sp_fail
                exec /bin/sh
            fi
        else
            echo "Failed to mount system.sfs"
            sp_fail
            exec /bin/sh
        fi
    else
        echo "Failed to setup loop device for system.sfs"
        sp_fail
        exec /bin/sh
    fi
else
    echo "Failed to find KDOS installation media."
    sp_fail
    exec /bin/sh
fi

echo "Boot failed. dropping to shell"
exec /bin/sh
EOF
chmod +x init

# ---------------------------------------------------------------------------
# CPU microcode
# ---------------------------------------------------------------------------
# The early loader runs before the initramfs is decompressed and before any
# filesystem exists: it scans the raw initrd image for the literal paths
# kernel/x86/microcode/{GenuineIntel,AuthenticAMD}.bin. Three consequences,
# and getting any one wrong means the microcode is silently never applied:
#
#   - this cpio must NOT be compressed, and the blobs inside it must not be
#     either. linux-firmware ships amd-ucode as .zst because the *runtime*
#     firmware loader can decompress; the early loader cannot.
#   - it must come FIRST in the file, ahead of the gzipped part.
#   - CONFIG_MICROCODE_LATE_LOADING is off, so this is the only path there is.
#     /usr/lib/firmware/intel-ucode.bin is never read at runtime.
UCODE=/kdos/build/ucode
rm -rf $UCODE
mkdir -p $UCODE/kernel/x86/microcode

if [ -f /usr/lib/firmware/intel-ucode.bin ]; then
    cp /usr/lib/firmware/intel-ucode.bin \
       $UCODE/kernel/x86/microcode/GenuineIntel.bin
    echo "Microcode: Intel bundle carried"
else
    echo "Microcode: no intel-ucode.bin — Intel CPUs will run BIOS microcode"
fi

# One AMD container per family, concatenated in the order the shell sorts them;
# the loader walks the containers and matches on the equivalence table.
for BLOB in /usr/lib/firmware/amd-ucode/microcode_amd*.bin.zst; do
    [ -e "$BLOB" ] || continue
    zstd -d -c "$BLOB" >> $UCODE/kernel/x86/microcode/AuthenticAMD.bin
done
if [ -s $UCODE/kernel/x86/microcode/AuthenticAMD.bin ]; then
    echo "Microcode: AMD containers carried"
else
    rm -f $UCODE/kernel/x86/microcode/AuthenticAMD.bin
    echo "Microcode: no amd-ucode blobs — AMD CPUs will run BIOS microcode"
fi

UCODE_CPIO=/kdos/build/ucode.cpio
rm -f $UCODE_CPIO
if [ -n "$(ls -A $UCODE/kernel/x86/microcode)" ]; then
    ( cd $UCODE && find . | cpio -o -H newc ) > $UCODE_CPIO 2>/dev/null
fi

# Pack Initramfs
find . | cpio -o -H newc | gzip -9 > ../initramfs.gz.part
if [ -s "$UCODE_CPIO" ]; then
    cat $UCODE_CPIO ../initramfs.gz.part > ../initramfs.cpio.gz
else
    mv ../initramfs.gz.part ../initramfs.cpio.gz
fi
rm -f ../initramfs.gz.part
echo "Initramfs created at $INITRAMFS"

ls /kdos/build