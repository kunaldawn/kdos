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
source script/phases/70_image/phase.env

echo "Building Initramfs..."

# Setup workspace
INITRAMFS=/kdos/build/initramfs
rm -rf $INITRAMFS
mkdir -p $INITRAMFS
cd $INITRAMFS

# Create Directory Structure
mkdir -p bin dev proc sys run mnt/iso newroot etc lib

# Install Basic Config
cp /etc/passwd etc/passwd
cp /etc/group etc/group

# copy_closure DEST SRC: SRC to DEST, then every library its NEEDED entries
# name, and theirs, from /usr/lib or /lib into lib/. NEEDED is read with
# readelf, from the dynamic section, without loading the file. A library that
# is installed nowhere stops the build here, naming the program that wanted it.
#
# THE READER IS PROVEN BEFORE IT IS TRUSTED. A readelf that is absent or
# prints another format fails inside a command substitution, which `set -e`
# does not see: every program would be copied with no libraries and the final
# check below would report nothing. binutils' readelf is first; llvm-readelf
# prints the same NEEDED lines. Either must find libblkid in blkid, which is
# known to link it, or the build stops.
READELF=""
for _r in readelf llvm-readelf; do
    command -v $_r >/dev/null 2>&1 || continue
    if $_r -d /usr/sbin/blkid 2>/dev/null | grep -q '(NEEDED).*\[libblkid\.so'; then
        READELF=$_r
        break
    fi
done
unset _r
if [ -z "$READELF" ]; then
    echo "FATAL: no readelf reads NEEDED from /usr/sbin/blkid; the initramfs" >&2
    echo "       library closure cannot be measured. binutils provides one." >&2
    exit 1
fi
needed_libs() {
    $READELF -d "$1" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'
}

copy_closure() {
    local _dest=$1 _src=$2 _lib _dir
    cp "$_src" "$_dest"
    for _lib in $(needed_libs "$_src"); do
        [ -e "lib/$_lib" ] && continue
        for _dir in /usr/lib /lib; do
            if [ -f "$_dir/$_lib" ]; then
                copy_closure "lib/$_lib" "$_dir/$_lib"
                break
            fi
        done
        if [ ! -e "lib/$_lib" ]; then
            echo "FATAL: $_src needs $_lib, which is installed nowhere." >&2
            exit 1
        fi
    done
}

# util_linux_only PATH: stop the build unless PATH answers --version as
# util-linux. Each program held to it below is one whose stand-ins from other
# userlands lack what this init relies on, and the boot fails without saying
# why.
util_linux_only() {
    case "$("$1" --version 2>/dev/null)" in
    *" from util-linux "*) return 0 ;;
    esac
    echo "FATAL: $1 is not util-linux's: its --version does not name util-linux." >&2
    exit 1
}

# GNU coreutils, grep and sed. The first row is what `init` runs; the rest are
# the rescue shell's, for the boot that does not reach a root, and the udev
# rules' (63-md-raid-arrays runs readlink and basename). Each must
# answer --version as GNU's (`dd` alone names its package `coreutils`, without
# the GNU) and is copied with its libraries.
for _n in cat chmod cut ls mkdir mknod seq sleep \
          cp mv rm ln rmdir head tail wc stat df dd sync uname readlink touch \
          basename sort tr grep sed; do
    _p=""
    for _d in /usr/bin /usr/sbin; do
        if [ -x "$_d/$_n" ]; then
            _p=$_d/$_n
            break
        fi
    done
    if [ -z "$_p" ]; then
        echo "FATAL: the initramfs needs $_n, which is installed nowhere." >&2
        exit 1
    fi
    case "$("$_p" --version 2>/dev/null)" in
    *"(GNU "*|*" (coreutils) "*) ;;
    *)
        echo "FATAL: $_p is not GNU's: its --version names no GNU package." >&2
        exit 1
        ;;
    esac
    copy_closure bin/$_n "$_p"
done
unset _n _p _d

# util-linux's switch_root, mount, umount, losetup, dmesg and setsid.
#
# switch_root MUST MOVE THE NEW ROOT ONTO / (mount(newroot, "/", MS_MOVE))
# before it chroot()s. One that only wipes the initramfs and chroot()s leaves
# every process on the machine chrooted for ever: a process that joins a mount
# namespace via setns() -- podman exec, distrobox enter, nsenter -m -- gets the
# empty initramfs rootfs as "/" and every path is ENOENT, and the kernel
# refuses CLONE_NEWUSER to a chrooted caller, so no user namespace can be
# created by anybody, root included, and no box can start at all. The machine
# boots perfectly either way, which is why the check exists.
#
# mount, umount and losetup are what `init` itself runs: the ISO scan, the
# loop device under system.sfs, the overlay and the --move of every mount into
# the new root. setsid gives the rescue shell tty1 as its controlling
# terminal. dmesg is for the rescue shell.
for _p in /usr/sbin/switch_root /usr/bin/mount /usr/bin/umount \
          /usr/sbin/losetup /usr/bin/dmesg /usr/bin/setsid; do
    util_linux_only "$_p"
    copy_closure bin/${_p##*/} "$_p"
done
unset _p

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

# Install util-linux's blkid.
#
# EVERY LOOKUP IN THIS INIT IS `blkid -U <uuid>` — the root filesystem, the
# ESP that holds the A/B state, and the LUKS container an encrypted root
# lives inside. A prober that cannot resolve -U, or cannot name a
# `crypto_LUKS` container, answers nothing to every one of them: an installed
# machine drops to a shell with "Root device not found", A/B selection
# silently never engages, and an encrypted root cannot be unlocked at all,
# with a passphrase prompt that never appears.
util_linux_only /usr/sbin/blkid
copy_closure bin/blkid /usr/sbin/blkid

# Install mdadm, for a root that lives on a software RAID array. At
# /usr/sbin, the path its udev rules run it from, with the init's bin/mdadm a
# link to it.
if [ -x /usr/sbin/mdadm ]; then
    mkdir -p usr/sbin
    copy_closure usr/sbin/mdadm /usr/sbin/mdadm
    ln -sf ../usr/sbin/mdadm bin/mdadm
else
    echo "Note: mdadm not installed — the initramfs cannot assemble an array"
fi

# Install e2fsck, for the check an ext4 root gets before it is mounted. Here
# and nowhere later: the root is mounted read-write below, and a filesystem
# with errors recorded in its superblock would otherwise run until somebody
# checks it by hand from other media. Copied WITH its libraries or not at all,
# the cryptsetup rule — an e2fsck that cannot exec is skipped at boot rather
# than failing it, and a skipped check is exactly the gap this closes.
if [ -x /usr/sbin/e2fsck ]; then
    _ok=1
    for _l in libext2fs.so.2 libcom_err.so.2 libe2p.so.2; do
        [ -f /usr/lib/$_l ] || _ok=0
    done
    if [ "$_ok" = 1 ]; then
        cp /usr/sbin/e2fsck bin/e2fsck
        for _l in libext2fs.so.2 libcom_err.so.2 libe2p.so.2; do
            cp /usr/lib/$_l lib/$_l
        done
    else
        echo "Note: e2fsck's libraries are missing — an ext4 root is mounted unchecked"
    fi
    unset _ok
else
    echo "Note: e2fsprogs not installed — an ext4 root is mounted unchecked"
fi

# Install eudev and dependencies
cp /sbin/udevd bin/udevd
cp /sbin/udevadm bin/udevadm
cp /usr/lib/libudev.so.1 lib/libudev.so.1
cp /usr/lib/libkmod.so.2 lib/libkmod.so.2
cp /usr/lib/liblzma.so.5 lib/liblzma.so.5
cp /usr/lib/libz.so.1 lib/libz.so.1
cp /usr/lib/libzstd.so.1 lib/libzstd.so.1

# Install kdos-bootctl, which decides WHICH root to boot when the machine has
# two. Limine cannot count boots — that is a systemd-boot feature — so the
# counting is ours and it has to happen here rather than in rcS: a kernel that
# boots into a wedged userland must still spend an attempt.
#
# /usr/bin/kdos-bootctl IS A SYMLINK TO /usr/sbin/ksvc and cp copies through it,
# so what lands here is the whole tool — linked -lpng for `kdos theme`'s
# wallpaper retint. libpng16 is therefore carried beside it; libz.so.1 is
# already above and musl's libm is inside libc, so those two are the whole
# closure.
#
# COPIED WHOLE OR NOT AT ALL, the cryptsetup rule one block down: a
# kdos-bootctl that cannot exec makes A/B selection silently never run, and the
# machine reads as one whose slot was never marked good rather than one missing
# a library.
BOOTCTL=""
[ -x /usr/bin/kdos-bootctl ] && BOOTCTL=/usr/bin/kdos-bootctl
[ -z "$BOOTCTL" ] && [ -x /usr/bin/kdos-tools ] && BOOTCTL=/usr/bin/kdos-tools
if [ -z "$BOOTCTL" ]; then
    echo "Note: kdos-bootctl not installed — no A/B slot selection at boot"
elif [ ! -f /usr/lib/libpng16.so.16 ]; then
    echo "Note: no libpng16 for kdos-bootctl — no A/B slot selection at boot"
else
    cp $BOOTCTL bin/kdos-bootctl
    cp /usr/lib/libpng16.so.16 lib/libpng16.so.16
fi

# Install cryptsetup, for an encrypted root.
#
# Copied WITH its libraries and skipped entirely when it is not installed: an
# initramfs that half-carries a cryptsetup is an initramfs that fails at the
# passphrase prompt instead of at build time. The list is explicit and each
# entry is copied only when /usr/lib has it; a library cryptsetup NEEDs that
# the list misses or /usr/lib lacks is caught by the closure check at the end
# of this step, which stops the build and names it. `init` asks only whether
# /bin/cryptsetup exists, when a cryptdevice= needs it.
if [ -x /usr/sbin/cryptsetup ]; then
    cp /usr/sbin/cryptsetup bin/cryptsetup
    for _l in libcryptsetup.so.12 libdevmapper.so.1.02 libjson-c.so.5 \
              libpopt.so.0 libssl.so.4 libcrypto.so.4 libargon2.so.1 \
              libuuid.so.1 libblkid.so.1 libz.so.1; do
        [ -f /usr/lib/$_l ] && cp /usr/lib/$_l lib/$_l
    done
else
    echo "Note: cryptsetup not installed — the initramfs cannot unlock a LUKS root"
fi

# Install lvm and dmsetup, for a root on a logical volume.
#
# AT /usr/sbin, THE PATHS lvm2's UDEV RULES NAME, and not in bin/: the rules
# are copied below with the rest of /usr/lib/udev. 95-dm-notify.rules runs
# `/usr/sbin/dmsetup udevcomplete` for every device-mapper change, and every
# program built on libdevmapper -- lvm, and cryptsetup opening a container --
# waits for that call with no timeout. A dmsetup the rule cannot find is a
# boot that hangs at the unlock or the activation, with nothing on screen.
#
# INSTALLED lvm2 MEANS THE INITRAMFS CARRIES BOTH, or the build stops: lvm2
# arrives as a dependency of cryptsetup and parted, so its rules are on every
# image that has them, and a missing binary is a package that did not install
# properly rather than one that was left out.
if [ -f /var/lib/kpkg/db/lvm2 ] || [ -f /usr/lib/udev/rules.d/95-dm-notify.rules ]; then
    for _p in /usr/sbin/lvm /usr/sbin/dmsetup; do
        if [ ! -x $_p ]; then
            echo "FATAL: lvm2 is installed but $_p is not." >&2
            echo "       Its udev rules run it, and a boot that opens any" >&2
            echo "       device-mapper device waits for it for ever." >&2
            exit 1
        fi
    done
    # The rules take that directory from lvm2's configure (its sbindir), so
    # it is read back rather than assumed: a rule naming dmsetup or lvm
    # anywhere else is the same silent hang, and stops the build here.
    _stray=$(cat /usr/lib/udev/rules.d/*.rules 2>/dev/null | grep -v '^[[:space:]]*#' \
             | grep -o '[^ "=]*/\(dmsetup\|lvm\) ' | grep -v '^/usr/sbin/' || true)
    if ! grep -q '/usr/sbin/dmsetup udevcomplete' /usr/lib/udev/rules.d/95-dm-notify.rules 2>/dev/null \
       || [ -n "$_stray" ]; then
        echo "FATAL: lvm2's udev rules do not run dmsetup and lvm from /usr/sbin," >&2
        echo "       where the initramfs carries them:" $_stray >&2
        exit 1
    fi
    unset _stray
    mkdir -p usr/sbin
    copy_closure usr/sbin/lvm /usr/sbin/lvm
    copy_closure usr/sbin/dmsetup /usr/sbin/dmsetup
    ln -sf ../usr/sbin/lvm bin/lvm
    ln -sf ../usr/sbin/dmsetup bin/dmsetup
    unset _p

    # AND thin_check AND cache_check, for a root on a thin or a cached
    # volume. lvm runs the one that matches before it activates a thin pool
    # or a cache and refuses the volume when it cannot, and 41_system's disk
    # list installs thin-provisioning-tools beside lvm2 (it depends on lvm2,
    # not the other way round), so a missing one is a broken install and stops
    # the build like a missing lvm does. Both names are argv[0] links to one
    # binary, pdata_tools, which is copied once with its libraries.
    #
    # lvm.conf NAMES THEM, and names nothing else: every other setting stays
    # the compiled default. The paths are this initramfs's, written here beside
    # the copy that makes them true, rather than inherited from wherever
    # lvm2's configure pointed.
    for _p in /usr/sbin/thin_check /usr/sbin/cache_check; do
        if [ ! -x $_p ]; then
            echo "FATAL: lvm2 is installed but $_p is not." >&2
            echo "       A thin or cached root would refuse to activate." >&2
            exit 1
        fi
    done
    _pdata=$(readlink -f /usr/sbin/thin_check)
    copy_closure "usr/sbin/${_pdata##*/}" "$_pdata"
    for _p in thin_check cache_check; do
        [ "$_p" = "${_pdata##*/}" ] || ln -sf "${_pdata##*/}" usr/sbin/$_p
    done
    mkdir -p etc/lvm
    cat > etc/lvm/lvm.conf <<'LVMCONF'
# Written by 090_initramfs.sh. Every setting not named here is lvm2's compiled
# default.
global {
	thin_check_executable = "/usr/sbin/thin_check"
	cache_check_executable = "/usr/sbin/cache_check"
}
LVMCONF
    unset _p _pdata
else
    echo "Note: lvm2 not installed — the initramfs cannot activate a volume group"
fi

# Install udev rules and helpers. Which of them stay is decided once every
# program and library is in the tree; see the udev check before the pack.
mkdir -p lib/udev/rules.d etc/udev/rules.d
cp -r /usr/lib/udev/rules.d/* lib/udev/rules.d/ 2>/dev/null || true
cp -r /etc/udev/rules.d/* etc/udev/rules.d/ 2>/dev/null || true
cp -r /usr/lib/udev/* lib/udev/ 2>/dev/null || true
# udev reads only the compiled hwdb.bin, which this tree does not carry; the
# sources would be carried for nothing.
rm -rf lib/udev/hwdb.d

# THE RULES NAME THE ROOT'S PATHS, so the initramfs answers at them. udevd
# runs a bare helper name from /usr/lib/udev, and the packages' rules run
# /usr/bin, /usr/sbin and /sbin; here the programs are in bin/ and the
# helpers in lib/udev, so each of those paths is a link into the tree. lvm2's
# and mdadm's binaries are in usr/sbin itself.
mkdir -p usr/lib usr/sbin
ln -sfn ../../lib/udev usr/lib/udev
ln -sfn ../bin usr/bin
ln -sfn bin sbin
ln -sf ../../bin/blkid usr/sbin/blkid

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
# Every disk transport the installer classifies and installs to must be
# reachable before the root mounts, or the installed root is not found on its
# first boot. NVMe behind Intel VMD (RST), eMMC and SD (mmc), Hyper-V, Xen
# and VMware guests' disks, and the MegaRAID and mpt3sas controllers are all
# modules on this kernel; copy_module brings mmc_core, hv_vmbus and the rest
# of each one's dependencies.
MODULES="$MODULES vmd mmc_block sdhci-pci sdhci-acpi cqhci rtsx_pci_sdmmc hv_storvsc xen-blkfront vmw_pvscsi megaraid_sas mpt3sas"

# vfat, so the initramfs can read the boot state off the ESP.
MODULES="$MODULES vfat nls_cp437 nls_iso8859-1"
# Root filesystems the installer can create. EVERY ROW OF ki_filesystems[] NOT
# BUILT INTO THE KERNEL MUST APPEAR HERE: the installer's table offers the
# choice, this makes it bootable, and a root the initramfs cannot mount
# installs perfectly and never boots again. f2fs is CONFIG_F2FS_FS=m; xfs is =y
# and its copy is a no-op, kept so a kernel with xfs as a module still boots.
MODULES="$MODULES xfs f2fs"
# dm-crypt and the ciphers a LUKS2 default header actually uses. Carried
# unconditionally: they are small, and a kernel that has them built in makes
# copy_module a no-op anyway. The generic AES, SHA-256 and SHA-512 code is
# built into this kernel and has no module to name.
MODULES="$MODULES dm-crypt dm-mod aesni-intel xts crypto_null algif_skcipher"
# Software RAID. A root ON an array needs these before anything can be
# assembled, and a machine whose DATA disks are an array needs them before
# udev settles — without md_mod the members are bare disks with a superblock
# nobody reads, which looks like an empty drive rather than a missing module.
# The personalities are listed individually because md_mod loads none of them.
MODULES="$MODULES md_mod raid0 raid1 raid10 raid456 dm-raid"
# The device-mapper targets an LVM root can be built from beyond the linear one
# dm-mod has built in: a thin volume, a cached or write-cached one, and a
# snapshot. A group holding any of them activates only with its target loaded,
# and the init loads these before its first vgchange -- lvm2's own modprobe
# path is whatever its configure found at build time, not one this initramfs
# is known to have.
MODULES="$MODULES dm-snapshot dm-thin-pool dm-cache dm-cache-smq dm-writecache"

for MOD in $MODULES; do
    copy_module $MOD
done

# The same list, kept on the image beside the initramfs it describes. A kernel
# installed later has none of these modules in this initramfs, and the linux
# package's postinstall reads this file to carry the new kernel's copies of
# exactly this set; a second copy of the list there would be the one that
# goes stale.
printf '%s\n' $MODULES > /boot/initramfs.modules

# Copy modules.order and modules.builtin for depmod
cp /lib/modules/$KERNEL_VER/modules.order $MOD_DIR/
cp /lib/modules/$KERNEL_VER/modules.builtin $MOD_DIR/
if [ -f /lib/modules/$KERNEL_VER/modules.builtin.modinfo ]; then
    cp /lib/modules/$KERNEL_VER/modules.builtin.modinfo $MOD_DIR/
fi

# Regenerate module dependencies for the initramfs. The symbol map is read
# where the kernel installed it; the initramfs does not carry it.
echo "Generating dependency map..."
if [ -f /boot/System.map-$KERNEL_VER ]; then
    depmod -b . -F /boot/System.map-$KERNEL_VER $KERNEL_VER
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
# This mount is the root's too: it is moved there, and nothing remounts it.
# gid=5 (tty) and mode 620 are what a new terminal is created with, so write
# and wall can reach it; ptmxmode=666 lets every user open /dev/pts/ptmx.
# The kernel's defaults are the opener's group, 600 and 000.
mount -t devpts -o gid=5,mode=620,ptmxmode=666 devpts /dev/pts
mkdir -p /dev/shm
mount -t tmpfs -o nosuid,nodev tmpfs /dev/shm

# fbcon defers taking the framebuffer until a glyph is printed on a virtual
# terminal. /dev/console is the last console=: ttyS0 on the live entries, so
# none of these messages reach the screen; tty0 on an installed machine, where
# the echo above has ended the deferral and fbcon draws them over the splash.
if [ -x /bin/kdos-splash ]; then
    /bin/kdos-splash run </dev/null >/dev/null 2>&1 &
fi
sp_step() { [ -x /bin/kdos-splash ] && /bin/kdos-splash step "\$1" 2>/dev/null; return 0; }
sp_ok()   { [ -x /bin/kdos-splash ] && /bin/kdos-splash ok 2>/dev/null; return 0; }
sp_fail() { [ -x /bin/kdos-splash ] && /bin/kdos-splash fail 2>/dev/null; return 0; }
sp_total() { [ -x /bin/kdos-splash ] && /bin/kdos-splash total "\$1" 2>/dev/null; return 0; }

# EVERY FAILURE ENDS HERE, AND SAYS WHY WHERE SOMEBODY IS LOOKING. /dev/console
# is the last console= on the command line, which on the live entries is the
# serial port: a shell there alone leaves a laptop showing the splash's fail
# state and nothing else. The reason goes to the splash as a detail line,
# which is the last thing it draws before it hands the screen back, and then
# to tty1, where fbcon takes the framebuffer over at the first write and the
# keyboard is. A shell is opened on tty1 in its own session with tty1 as its
# controlling terminal, and this process becomes the shell on /dev/console,
# so the serial line keeps its shell and the log it has been reading. When
# /dev/console is itself a virtual terminal there is one screen and one
# shell. PID 1 must never return from here: a shell that exits on
# /dev/console panics the kernel with "Attempted to kill init".
rescue() {
    local con
    echo "\$1"
    if [ -x /bin/kdos-splash ]; then
        /bin/kdos-splash detail "\$1" 2>/dev/null
        /bin/kdos-splash quit 2>/dev/null
    fi
    con=\$(cat /sys/class/tty/console/active 2>/dev/null)
    con=\${con##* }
    case "\$con" in
    tty[0-9]*)
        printf '\n%s\nKDOS cannot boot. This is a rescue shell.\n' "\$1"
        exec /bin/sh
        ;;
    esac
    {
        printf '\n%s\n' "\$1"
        echo "KDOS cannot boot. This is a rescue shell; the boot log is on the serial console."
    } > /dev/tty1 2>/dev/null
    setsid -f -c /bin/sh -i < /dev/tty1 > /dev/tty1 2>&1
    echo "A rescue shell is also open on tty1."
    exec /bin/sh
}

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

#
# ASSEMBLE ANY SOFTWARE RAID BEFORE ANYTHING LOOKS FOR A ROOT. An array's
# members are bare partitions carrying a superblock until mdadm brings them
# together; the filesystem UUID the boot is hunting for lives INSIDE the
# array and does not exist on the disk, so a blkid run before this finds the
# members and not the root.
#
# \`--scan\` AND NOT A CONFIG. Nothing here knows which arrays this machine
# has; each member's own superblock does, which is also what makes a machine
# whose disks were reordered still boot.
#
# SILENT WHEN THERE IS NO ARRAY: mdadm exits non-zero when it assembles
# nothing, which is the ordinary case on the overwhelming majority of
# machines and must not read as a failure.
if [ -x /bin/mdadm ]; then
    # The total is additive and only this branch knows it is going to run —
    # the rule the unlock and the slot blocks already keep.
    sp_total 1
    sp_step "RAID"
    mdadm --assemble --scan >/dev/null 2>&1 || true
    udevadm settle
    sp_ok
fi

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

# Parse Boot Parameters. INIT_ARG starts empty: the kernel hands init every
# unrecognised name=value on its command line as an environment variable.
INIT_ARG=""
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
        kdos_slot=*)
            BOOT_SLOT="\${i#kdos_slot=}"
            ;;
        # Handed on to init: \`single\` runs rcS and then sulogin, with no
        # consoles and no desktop; \`emergency\` runs sulogin before rcS.
        single|emergency)
            INIT_ARG="\$i"
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
# The prompt goes through the SPLASH, not to /dev/console, which is the last
# console=: tty0 under the splash on an installed machine, ttyS0 on the live
# entries, where a plain \`read -p\` prompts a serial port nobody is looking at
# while the screen shows a splash that appears to have frozen. The keystrokes
# are read from /dev/tty1, which is where the keyboard actually is.
#
# There is no per-keystroke feedback: the splash owns the framebuffer and bash
# owns the terminal, and a passphrase field that echoed dots would mean moving
# the read into the splash. Stated rather than hidden.
#
# Two knobs, both defaulted to the real thing. The keyboard is on tty1 whatever
# console= names, and the mapper directory is where the kernel puts
# an opened container. They are variables so \`testing/selftest.sh\` can exercise
# this function without a LUKS volume and without root — the same trick
# \`kdos stutter --fixture\` uses for /proc.
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
        # is there, whatever /dev/console is.
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
# out not to work. \`select\` prints the UUID to boot and spends an attempt in the
# same breath, so a kernel that hangs after this point has still been counted.
#
# Failing to read it is not fatal: \`root=\` on the command line is what a machine
# without A/B uses anyway, and it stays the fallback.
#
# AND THE SLOT'S OWN CONTAINER COMES WITH IT. \`select\` yields a FILESYSTEM
# identifier; on an encrypted machine that filesystem is inside a LUKS
# container, and the command line can name exactly one \`cryptdevice=\`. Two
# slots inside two containers cannot both be named there, so each slot records
# its own and \`crypt\` is asked for the one that belongs to the filesystem
# \`select\` just chose. A slot that names none leaves \`CRYPTDEV\` exactly as the
# command line set it, which is every machine installed without encryption and
# every machine whose two slots share one container.
#
# THE ORDER IS LOAD-BEARING: this block runs BEFORE the unlock below, because
# the unlock is what has to happen to the container this block names. Moving
# the unlock above it would unlock whichever container the command line
# mentions and then look for the other slot's filesystem inside it.
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
        # \`kdos_slot=\` is the slot whose ESP directory this kernel came
        # from, and it is handed to \`select\`: that slot's modules are the
        # only ones this kernel can load, so a hand-picked entry boots its
        # own slot and nothing else. Absent on a menu whose entries share
        # one kernel, where \`select\` decides alone. \`select\` also rewrites
        # the menu on the ESP when its decision moves the next boot.
        SEL=\$(KDOS_BOOTSTATE=/esp/EFI/kdos/bootstate \
               /bin/kdos-bootctl select \${BOOT_SLOT:+"\$BOOT_SLOT"} \
               2>/dev/console)
        # BOTH READS HAPPEN WHILE IT IS MOUNTED. \`crypt\` reads the same file
        # \`select\` just wrote, so asking after the umount below reads nothing
        # and silently drops the container — an encrypted second slot would
        # then be unlocked with the first slot's container and fail to mount,
        # which reads as a corrupt filesystem rather than as a missing lookup.
        # It spends no attempt: \`select\` is the only verb that counts.
        SLOT_CRYPT=""
        [ -n "\$SEL" ] && SLOT_CRYPT=\$(KDOS_BOOTSTATE=/esp/EFI/kdos/bootstate \
               /bin/kdos-bootctl crypt "\$SEL" 2>/dev/null)
        # Unmounted immediately: the root filesystem mounts it again at
        # /boot/efi, and two mounts of one FAT filesystem is how a state file
        # gets written twice and read once.
        umount /esp 2>/dev/null
        if [ -n "\$SEL" ]; then
            echo "Boot slot selected: \$SEL"
            ROOT_UUID="\$SEL"
            if [ -n "\$SLOT_CRYPT" ]; then
                # The mapper name is this initramfs's own and not the state
                # file's: only one container is ever open at a time here, so
                # there is nothing for a per-slot name to disambiguate, and a
                # name read out of a file on the ESP is a name somebody can
                # edit into a path.
                CRYPTDEV="UUID=\$SLOT_CRYPT:kdosroot"
                echo "Slot container: \$SLOT_CRYPT"
            fi
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

#
# VOLUME GROUPS ARE ACTIVATED BEFORE ANYTHING LOOKS FOR THE ROOT, for the
# reason the RAID block gives: a filesystem on a logical volume has no device
# node until its group is active, and \`blkid -U\` finds nothing.
#
# ON EACH SIDE OF THE UNLOCK, because LVM and LUKS stack both ways: a group
# inside a container exists only after the unlock, and a container on a
# logical volume has to be active before it. Each call runs lvm only when
# blkid reports a set of physical volumes it has not already activated, so a
# disk boot with no LVM costs one blkid per call and never starts lvm, and a
# live boot -- no root=, no cryptdevice= -- makes no call at all; 03_lvm.sh
# activates the groups of a live session.
#
# --sysinit is no dmeventd monitoring, no background polling and no locking
# failure: the three things an initramfs cannot provide. The lvm.conf carried
# here names thin_check and cache_check and nothing else, so every other
# setting is the compiled default and every group is activated. The groups
# stay active across switch_root.
#
# THE THIN, CACHE AND SNAPSHOT TARGETS ARE LOADED FIRST, on the first call
# that finds a physical volume: a group holding one of those volumes activates
# only with its target present, and lvm's own modprobe is a path its configure
# chose. A kernel with them built in makes this a no-op.
: "\${LVM_BIN:=/usr/sbin/lvm}"
LVM_PVS=""
activate_lvm() {
    local pvs
    [ -x "\$LVM_BIN" ] || return 0
    pvs=\$(blkid -t TYPE=LVM2_member -o device 2>/dev/null)
    [ -n "\$pvs" ] && [ "\$pvs" != "\$LVM_PVS" ] || return 0
    if [ -z "\$LVM_PVS" ]; then
        modprobe -q -a dm-snapshot dm-thin-pool dm-cache dm-cache-smq \
            dm-writecache 2>/dev/null || true
    fi
    LVM_PVS="\$pvs"
    sp_total 1
    sp_step "VOLUME GROUPS"
    if "\$LVM_BIN" vgchange -aay --sysinit; then
        udevadm settle 2>/dev/null || true
        sp_ok
    else
        # Not a stop: a group that is partly missing need not hold the root,
        # and the root lookup below says so when it does.
        sp_fail
    fi
}
[ -n "\$ROOT_UUID\$CRYPTDEV" ] && activate_lvm

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
        activate_lvm
    else
        sp_fail
        rescue "Failed to unlock \$CRYPTDEV"
    fi
fi

if [ -n "\$ROOT_UUID" ]; then
    # Disk Boot Mode
    echo "Waiting for root device \$ROOT_UUID..."
    sp_total 3
    sp_step "ROOT DEVICE"

    # Wait for device to appear (timeout 10s). A disk that enumerates late
    # may carry the root's physical volume, so each pass activates any group
    # that has appeared since; with no new PV that is one blkid and no lvm.
    for i in \$(seq 1 10); do
        activate_lvm
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
        # An ext4 root is checked before it is mounted, which is the only
        # point at which it is not in use. -p repairs what is safe to repair
        # unattended. 1 and 2 are "repaired", and the root is not mounted so
        # there is nothing to reboot for; 4 and above left errors behind, and
        # the boot goes on and says so rather than stopping at a shell nobody
        # can see behind the splash. btrfs, xfs and f2fs check themselves at
        # mount time.
        ROOT_TYPE=\$(blkid -o value -s TYPE "\$ROOT_DEV" 2>/dev/null)
        case "\$ROOT_TYPE" in
            ext2|ext3|ext4)
                if [ -x /bin/e2fsck ]; then
                    e2fsck -p "\$ROOT_DEV"
                    FSCK_RC=\$?
                    if [ \$FSCK_RC -ge 4 ]; then
                        echo "e2fsck: \$ROOT_DEV still has errors (exit \$FSCK_RC)"
                        [ -x /bin/kdos-splash ] && /bin/kdos-splash msg \
                            "ROOT FILESYSTEM HAS ERRORS - RUN e2fsck" 2>/dev/null
                    fi
                fi
                ;;
        esac
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
            exec switch_root /newroot /sbin/init \$INIT_ARG
        else
            echo "Error: /sbin/init not found on root device!"
            sp_fail
        fi
    else
        echo "Error: Root device with UUID=\$ROOT_UUID not found!"
        sp_fail
    fi
    
    rescue "Disk boot failed: no root at UUID=\$ROOT_UUID with /sbin/init on it"
fi

#
# THE PERSISTENCE STORE IS LOOKED FOR UNTIL USB STORAGE HAS SETTLED. The
# medium is found the moment its own device answers, and a store on another
# USB stick is still being enumerated then: behind a hub it gets an address a
# few hundred milliseconds later, and usb-storage waits a further delay_use
# (one second) before it scans the disk. A single lookup at that point finds
# nothing and the session runs in RAM with the store attached.
#
# USB IS BUSY while a hub port holds a device that is not yet configured, or
# a mass-storage interface has no block device beneath it. Neither state is
# visible for the moment between a hub being configured and its first port
# reporting a connection (power-good plus debounce, well under a second), so
# while a hub other than a root hub is present the lookup also waits until
# the set of USB devices has stood still for one second. With nothing busy
# and no such hub, a boot with no store pays one blkid and no wait. The bound
# is 10 seconds, the bound the medium and the root device get: a device that
# never finishes costs that much and no more, and the boot goes on in RAM.
#
# A NEW DISK GETS ONE MORE LOOKUP. Its block device appears in sysfs before
# the kernel has read its partition table, so a blkid that starts then can
# list the disk without the partition holding the store. blkid opens every
# disk it lists, and the open waits for that read, so the lookup after it
# sees the partitions: the wait ends only after a lookup whose scan counted
# the same USB disks as the scan before the previous one. That second lookup
# runs at once, with no sleep.
: "\${USB_DEVICES:=/sys/bus/usb/devices}"
uptime_cs() {
    local u
    read -r u _ < /proc/uptime
    NOW=\$(( 10#\${u%.*}\${u#*.} ))
}
usb_scan() {
    local p s d found
    USB_BUSY=""
    USB_SEEN=""
    USB_DISKS=""
    USB_HUB=0
    for p in "\$USB_DEVICES"/*:*/*-port*/state; do
        [ -r "\$p" ] || continue
        read -r s < "\$p"
        case "\$s" in
        "not attached"|configured|suspended) ;;
        *) USB_BUSY="\$USB_BUSY \${p%/state}" ;;
        esac
    done
    for d in "\$USB_DEVICES"/*; do
        [ -e "\$d" ] || continue
        case "\${d##*/}" in
        *:*)
            [ -r "\$d/bInterfaceClass" ] || continue
            read -r s < "\$d/bInterfaceClass"
            [ "\$s" == "08" ] || continue
            found=0
            for p in "\$d"/host*/target*/*/block/*; do
                [ -e "\$p" ] || continue
                found=1
                USB_DISKS="\$USB_DISKS \${p##*/}"
            done
            [ "\$found" == "1" ] || USB_BUSY="\$USB_BUSY \$d"
            ;;
        usb*)
            USB_SEEN="\$USB_SEEN \${d##*/}"
            ;;
        *)
            USB_SEEN="\$USB_SEEN \${d##*/}"
            [ -r "\$d/bDeviceClass" ] || continue
            read -r s < "\$d/bDeviceClass"
            [ "\$s" == "09" ] && USB_HUB=1
            ;;
        esac
    done
}
find_persist() {
    local deadline quiet last=x disks="" said=0
    PERSIST=""
    uptime_cs
    deadline=\$(( NOW + 1000 ))
    while :; do
        usb_scan
        PERSIST=\$(blkid -L KDOS_PERSIST 2>/dev/null)
        [ -n "\$PERSIST" ] && return 0
        uptime_cs
        if [ "\$USB_SEEN" != "\$last" ]; then
            last="\$USB_SEEN"
            quiet=\$NOW
            [ "\$USB_HUB" == "1" ] && quiet=\$(( NOW + 100 ))
        fi
        if [ -z "\$USB_BUSY" ] && [ "\$NOW" -ge "\$quiet" ]; then
            [ "\$USB_DISKS" == "\$disks" ] && return 1
            disks="\$USB_DISKS"
            continue
        fi
        disks="\$USB_DISKS"
        if [ "\$NOW" -ge "\$deadline" ]; then
            echo "USB storage still settling after 10s; no persistence store found"
            return 1
        fi
        if [ "\$said" == "0" ]; then
            echo "Waiting for USB devices to settle before looking for a persistence store..."
            said=1
        fi
        sleep 0.1
    done
}

# Live ISO Boot Mode (Fallback)
mkdir -p /mnt/iso
echo "Searching for KDOS boot media..."

# Announce the stage BEFORE the scan: with only the two common steps closed
# the bar would otherwise sit on "done" through the whole scan.
sp_total 4
sp_step "BOOT MEDIA"

# Find the medium by polling, never by waiting a fixed interval first: udev has
# already settled above, so on every machine whose media is enumerated the first
# pass succeeds and costs nothing. The 10-second bound is what covers the slow
# ones — a USB stick, or a device behind a bridge — and it matches the bound the
# disk-boot path gives its root device. Every retry walks EVERY device class
# again: the first node to answer is not always the one holding the medium, and
# a class that has not appeared yet must still get its chance.
SCAN_UNTIL=\$(( \$(cut -d. -f1 /proc/uptime) + 10 ))
FOUND=0
PASS=0
STUCK=0
CHECKED=" "
while :; do
    PASS=\$(( PASS + 1 ))
    for dev in /dev/sr* /dev/sd* /dev/vd* /dev/nvme* /dev/mmcblk*; do
        [ -e "\$dev" ] || continue
        # A DRIVE WITH NO MEDIUM IN IT IS PASSED OVER. Opening it fails
        # (ENOMEDIUM), so the scan opens before it mounts: a mount of it only
        # prints "Can't open blockdev", and its sysfs size is no test, since
        # an empty optical drive reports a guessed size rather than 0. It is
        # not marked checked: a disc still spinning up opens on a later pass.
        { : < "\$dev"; } 2>/dev/null || continue
        # ONE INSPECTION PER DEVICE. A node that mounts as iso9660 and has no
        # system.sfs on it will not grow one, so repeating the mount/umount
        # pair for it every 100 ms is churn on a device the scan has already
        # answered. Only a device that has not mounted yet is retried, which
        # is the case the bound exists for.
        case "\$CHECKED" in *" \$dev "*) continue ;; esac
        # Only the first pass narrates. A hundred retries of the same two
        # lines would bury the one message that explains a failed boot, and
        # this log is the only thing left to read when one happens.
        if [ "\$PASS" == "1" ]; then
            echo "Checking \$dev..."
            mount -t iso9660 "\$dev" /mnt/iso || continue
        else
            mount -t iso9660 "\$dev" /mnt/iso 2>/dev/null || continue
        fi
        CHECKED="\$CHECKED\$dev "
        if [ -f /mnt/iso/system.sfs ]; then
            echo "Found KDOS media on \$dev"
            FOUND=1
            break
        fi
        # THE UMOUNT IS CHECKED because /mnt/iso is the only mount point the
        # scan has: if it stays busy, the next mount stacks a second
        # filesystem on the same path and every later test reads the wrong
        # one. Nothing further can be inspected, so the scan stops here and
        # the boot goes to the shell with a reason on the console.
        if ! umount /mnt/iso; then
            echo "Cannot release /mnt/iso after \$dev — stopping the scan"
            STUCK=1
            break
        fi
    done
    [ "\$FOUND" == "1" ] && break
    [ "\$STUCK" == "1" ] && break
    [ "\$(cut -d. -f1 /proc/uptime)" -lt "\$SCAN_UNTIL" ] || break
    [ "\$PASS" == "1" ] && echo "No KDOS media yet; retrying until the 10s bound..."
    sleep 0.1
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
             mkdir -p /newroot
             modprobe overlay

             # THE UPPER IS WHERE A LIVE SESSION'S WRITES LAND, and whether it
             # survives a power-off is the whole of what persistence means. A
             # filesystem labelled KDOS_PERSIST is used as the upper when one
             # is present; with no store the upper is a tmpfs and the session
             # is gone at reboot. \`kdos persist\` makes the store, and
             # find_persist waits for USB storage before it gives up on one.
             #
             # \`nopersist\` ON THE COMMAND LINE FORCES A CLEAN SESSION, and the
             # boot menu carries an entry that passes it. A store holding a
             # change that stops the desktop coming up would otherwise be
             # reachable only by taking the stick to another machine.
             #
             # THE STORE MUST CARRY XATTRS, HARDLINKS AND A d_type, so vfat,
             # exfat and ntfs cannot hold one: overlayfs refuses such an upper
             # with EINVAL, which is the same answer it gives for every other
             # bad mount. Refusing them BY NAME here is what makes a
             # hand-made store say what is wrong with it.
             PERSIST=""
             case " \$(cat /proc/cmdline) " in
             *" nopersist "*)
                 echo "nopersist: this session will not be saved" ;;
             *)
                 find_persist ;;
             esac

             UPPER=""
             if [ -n "\$PERSIST" ]; then
                 PTYPE=\$(blkid -o value -s TYPE "\$PERSIST" 2>/dev/null)
                 case "\$PTYPE" in
                 vfat|exfat|ntfs|iso9660|squashfs)
                     echo "persistence store \$PERSIST is \$PTYPE, which cannot hold an overlay upper"
                     ;;
                 *)
                     mkdir -p /mnt/persist
                     if mount "\$PERSIST" /mnt/persist; then
                         mkdir -p /mnt/persist/upper /mnt/persist/work
                         UPPER="upperdir=/mnt/persist/upper,workdir=/mnt/persist/work"
                         echo "Persistent session on \$PERSIST (\$PTYPE)"
                     else
                         echo "persistence store \$PERSIST would not mount"
                     fi
                     ;;
                 esac
             fi

             # A STORE THAT DOES NOT WORK MUST NOT COST THE BOOT. Everything
             # above can fail on a medium somebody else wrote, and a stick that
             # drops to a shell because its persistence is broken is worse than
             # one that quietly forgets. Every failure lands here, and the
             # session comes up exactly as it would with no store at all.
             if [ -n "\$UPPER" ]; then
                 echo "Mounting OverlayFS (persistent)..."
                 mount -t overlay overlay -o lowerdir=/mnt/system,\$UPPER /newroot || UPPER=""
                 [ -n "\$UPPER" ] || umount /mnt/persist 2>/dev/null
             fi
             if [ -z "\$UPPER" ]; then
                 echo "Mounting OverlayFS..."
                 mkdir -p /mnt/overlay
                 mount -t tmpfs tmpfs /mnt/overlay
                 mkdir -p /mnt/overlay/upper /mnt/overlay/work
                 mount -t overlay overlay -o lowerdir=/mnt/system,upperdir=/mnt/overlay/upper,workdir=/mnt/overlay/work /newroot
             fi

             # Check if switch root dir is valid
             if [ ! -d "/newroot" ]; then
                sp_fail
                rescue "Error: /newroot is not a directory"
             fi
             sp_ok

            # Create missing mountpoints in newroot
            mkdir -p /newroot/dev /newroot/proc /newroot/sys /newroot/run /newroot/tmp

            # THE SQUASHFS AND THE RAM UPPER ARE DETACHED, and their mount
            # points removed, so nothing is left under /mnt. switch_root
            # deletes the initramfs but never crosses into another filesystem:
            # a mount still on /mnt/system or /mnt/overlay keeps /mnt from
            # being removed, and every live boot prints \`failed to unlink mnt:
            # Directory not empty\`. The overlay took private clones of its
            # layers when it was mounted, so / keeps reading them; the detach
            # only takes away names switch_root would leave unreachable anyway.
            # It runs BEFORE /proc moves: util-linux umount looks its target up
            # in /proc/self/mountinfo and, with no /proc, fails with "No such
            # file or directory" and detaches nothing. It runs only once
            # /sbin/init is known to be there, so a rescue shell for a missing
            # init still finds both mounts.
            if [ -x /newroot/sbin/init ]; then
                umount -l /mnt/system
                [ -z "\$UPPER" ] && umount -l /mnt/overlay
                rmdir /mnt/system /mnt/overlay 2>/dev/null
            fi

            # Move pseudo-filesystems under the new root (standard; Alpine mkinitfs does this)
            mount --move /dev /newroot/dev
            mount --move /proc /newroot/proc
            mount --move /sys /newroot/sys

            # /mnt/iso HAS TO BE MOVED, not detached like the squashfs and the
            # tmpfs are above. Left behind it dies with the initramfs
            # namespace, and everything the booted system reads off the boot
            # medium becomes unreachable BY NAME
            # while the bytes are still on the disk: \`kdos rebuild\` looks in
            # /mnt/iso/sources, and would report a stick empty that is carrying
            # the sources it was built with.
            #
            # Guarded on the mount existing: a boot that found system.sfs some
            # other way must not fail here, and \`mount --move\` on a path that
            # is not a mountpoint is an error rather than a no-op. The test is a
            # shell builtin against the file this branch has already proven is
            # there — nothing on the boot path should need a second binary to
            # answer a question it already knows.
            if [ -e /mnt/iso/system.sfs ]; then
                mkdir -p /newroot/mnt/iso
                mount --move /mnt/iso /newroot/mnt/iso
            fi

            # THE PERSISTENCE STORE MOVES FOR THE SAME REASON AND ONE MORE.
            # Left in the initramfs namespace it is unreachable by name, so
            # \`kdos persist\` could not report how full the store is that the
            # session is writing to. It also has to be a mount the SHUTDOWN can
            # see: /etc/inittab unmounts what is mounted, and a store that is
            # not in the new root's table is never flushed by it.
            if [ -n "\$UPPER" ]; then
                mkdir -p /newroot/mnt/persist
                mount --move /mnt/persist /newroot/mnt/persist
            fi

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
                exec switch_root /newroot /sbin/init \$INIT_ARG
            else
                ls -l /newroot/sbin/init
                sp_fail
                rescue "Error: /sbin/init not found in new root!"
            fi
        else
            sp_fail
            rescue "Failed to mount system.sfs"
        fi
    else
        sp_fail
        rescue "Failed to set up a loop device for system.sfs"
    fi
else
    sp_fail
    rescue "Failed to find KDOS installation media."
fi

rescue "Boot failed."
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
#     either, whatever form linux-firmware ships them in: the early loader
#     cannot decompress. A `.bin` is copied as it is and a `.bin.zst` is
#     expanded on the way in.
#   - it must come FIRST in the file, ahead of the gzipped part.
#   - CONFIG_MICROCODE_LATE_LOADING is off, so this is the only path there is.
#     /usr/lib/firmware/intel-ucode.bin is never read at runtime.
#
# A vendor whose microcode is installed and yields an empty file stops the
# build. An initramfs without microcode boots with no symptom, so nothing
# later would notice.
UCODE=/kdos/build/ucode
rm -rf $UCODE
mkdir -p $UCODE/kernel/x86/microcode

if [ -f /usr/lib/firmware/intel-ucode.bin ]; then
    cp /usr/lib/firmware/intel-ucode.bin \
       $UCODE/kernel/x86/microcode/GenuineIntel.bin
    if [ ! -s $UCODE/kernel/x86/microcode/GenuineIntel.bin ]; then
        echo "FATAL: /usr/lib/firmware/intel-ucode.bin is empty." >&2
        exit 1
    fi
    echo "Microcode: Intel bundle carried"
elif [ -f /var/lib/kpkg/db/intel-ucode ]; then
    echo "FATAL: intel-ucode is installed but /usr/lib/firmware/intel-ucode.bin is not." >&2
    exit 1
else
    echo "Microcode: no intel-ucode.bin — Intel CPUs will run BIOS microcode"
fi

# One AMD container per family, concatenated in the order the shell sorts them;
# the loader walks the containers and matches on the equivalence table.
if [ -d /usr/lib/firmware/amd-ucode ]; then
    for BLOB in /usr/lib/firmware/amd-ucode/microcode_amd*.bin \
                /usr/lib/firmware/amd-ucode/microcode_amd*.bin.zst; do
        [ -f "$BLOB" ] || continue
        case "$BLOB" in
        *.zst) zstd -d -c "$BLOB" >> $UCODE/kernel/x86/microcode/AuthenticAMD.bin ;;
        *)     cat "$BLOB" >> $UCODE/kernel/x86/microcode/AuthenticAMD.bin ;;
        esac
    done
    if [ ! -s $UCODE/kernel/x86/microcode/AuthenticAMD.bin ]; then
        echo "FATAL: /usr/lib/firmware/amd-ucode/ holds no microcode_amd*.bin or .bin.zst" >&2
        echo "       container, so AMD CPUs would run BIOS microcode." >&2
        exit 1
    fi
    echo "Microcode: AMD containers carried"
else
    echo "Microcode: no amd-ucode/ — AMD CPUs will run BIOS microcode"
fi

UCODE_CPIO=/kdos/build/ucode.cpio
rm -f $UCODE_CPIO
if [ -n "$(ls -A $UCODE/kernel/x86/microcode)" ]; then
    ( cd $UCODE && find . | LC_ALL=C sort | cpio -o -H newc --reproducible ) \
        > $UCODE_CPIO 2>/dev/null
fi

# EVERY UDEV RULE CARRIED HERE CAN RUN WHAT IT NAMES. The rules and helpers
# above are the root's whole set, and a rule whose program is absent makes
# udevd log "failed to execute" for every device it matches. Nothing is lost
# by leaving one out: the root's udev replays every rule once it starts.
#
# A helper first: one that links a library this tree does not carry (libmtp,
# libinput's, libgpod's) exists and fails at every event, so it goes. Then a
# rules file any of whose RUN, PROGRAM or IMPORT{program} keys names a
# program the tree lacks -- an absolute path, or a bare name, which udevd
# runs from /usr/lib/udev -- goes whole: removing lines would leave its
# GOTOs and LABELs unbalanced.
#
# THE RULES THAT ASSEMBLE A ROOT MUST STAY, or the build stops. device-mapper
# (10-dm, 11-dm-lvm, 13-dm-disk, 95-dm-notify) is what LUKS and LVM wait on;
# md's assemble an array; 64-btrfs completes a multi-device btrfs; and
# 60-persistent-storage makes /dev/disk/by-*. Each is required only when the
# root has it, and one that names a missing program is a program this step
# failed to carry.
for _f in lib/udev/*; do
    [ -f "$_f" ] && [ ! -L "$_f" ] || continue
    for _l in $(needed_libs "$_f" 2>/dev/null); do
        if [ ! -e "lib/$_l" ]; then
            echo "Note: udev helper ${_f##*/} needs $_l, which the initramfs does not carry — left out"
            rm -f "$_f"
            break
        fi
    done
done
_required=" 10-dm 11-dm-lvm 13-dm-disk 95-dm-notify 63-md-raid-arrays 64-md-raid-assembly 64-btrfs 60-persistent-storage "
for _r in lib/udev/rules.d/*.rules etc/udev/rules.d/*.rules; do
    [ -f "$_r" ] || continue
    for _p in $(grep -v '^[[:space:]]*#' "$_r" \
                | grep -oE '(RUN(\{program\})?|PROGRAM|IMPORT\{program\})[+:]?==?"[^"]*"' \
                | sed 's/^[^"]*"[[:space:]]*//; s/[[:space:]].*//; s/"$//' | sort -u); do
        case "$_p" in
        /*) _t=".$_p" ;;
        *[!A-Za-z0-9_.-]*) continue ;;
        *) _t="lib/udev/$_p" ;;
        esac
        [ -e "$_t" ] && continue
        _b=${_r##*/}
        case "$_required" in
        *" ${_b%.rules} "*)
            echo "FATAL: the udev rule $_r runs $_p, which the initramfs does not carry." >&2
            echo "       A root that needs this rule could not be assembled." >&2
            exit 1
            ;;
        esac
        echo "Note: udev rule $_b runs $_p, which the initramfs does not carry — left out"
        rm -f "$_r"
        break
    done
done
for _r in $_required; do
    if [ -f /usr/lib/udev/rules.d/$_r.rules ] && [ ! -f lib/udev/rules.d/$_r.rules ]; then
        echo "FATAL: the udev rule $_r.rules is on the root and not in the initramfs." >&2
        exit 1
    fi
done
unset _f _l _r _p _t _b _required

# EVERY PROGRAM'S LIBRARIES ARE HERE, or the initramfs is not built. NEEDED is
# read with the reader proven above, without loading anything, for each
# program in bin/ and usr/sbin/ and each library in lib/. A file that is not
# ELF -- a script -- names nothing. A missing one is an init that cannot exec -- the kernel
# panics with "Attempted to kill init" -- or a tool that says "not found" at
# the one moment it is needed.
_missing=""
for _f in bin/* usr/sbin/* lib/*.so*; do
    [ -f "$_f" ] && [ ! -L "$_f" ] || continue
    for _l in $($READELF -d "$_f" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
        [ -e "lib/$_l" ] || _missing="$_missing $_f:$_l"
    done
done
if [ -n "$_missing" ]; then
    echo "FATAL: the initramfs is missing libraries its programs need:" >&2
    for _m in $_missing; do echo "       $_m" >&2; done
    exit 1
fi
unset _missing _f _l _m

# Pack Initramfs
find . | LC_ALL=C sort | cpio -o -H newc --reproducible | gzip -9 > ../initramfs.gz.part
if [ -s "$UCODE_CPIO" ]; then
    cat $UCODE_CPIO ../initramfs.gz.part > ../initramfs.cpio.gz
else
    mv ../initramfs.gz.part ../initramfs.cpio.gz
fi
rm -f ../initramfs.gz.part
echo "Initramfs created at $INITRAMFS"
