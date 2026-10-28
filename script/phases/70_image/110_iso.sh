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

echo "Building KDOS ISO..."

ISO_ROOT=/kdos/build/iso_root
rm -rf $ISO_ROOT
mkdir -p $ISO_ROOT

# 1. Copy Kernel and Initramfs
#
# THEY SIT ON THE MEDIUM, NOT IN THE EFI SYSTEM PARTITION, because Limine
# reads ISO9660 and a UEFI firmware never has to. That is what keeps the ESP
# down to the two megabytes Limine's own El Torito image is: a bootloader that
# could only read FAT would need the kernel and the initramfs copied into the
# ESP as well as onto the medium, and the image would carry both twice.
mkdir -p $ISO_ROOT/boot $ISO_ROOT/EFI/BOOT

if [ -f /boot/vmlinuz-kdos ]; then
    cp /boot/vmlinuz-kdos $ISO_ROOT/boot/vmlinuz
else
    echo "Error: /boot/vmlinuz-kdos not found!"
    exit 1
fi

if [ -f /kdos/build/initramfs.cpio.gz ]; then
    cp /kdos/build/initramfs.cpio.gz $ISO_ROOT/boot/initramfs.cpio.gz
    # Also copy to /boot so it is included in system.sfs (and thus installed)
    cp /kdos/build/initramfs.cpio.gz /boot/initramfs.cpio.gz
    # The image's initramfs is the one for the image's kernel. One a `linux`
    # postinstall built in this tree for an earlier kernel would be preferred
    # over it by kinstall and `kdos-bootctl deploy`, and boot a stale base.
    rm -f /boot/initramfs-kdos.cpio.gz
else
    echo "Error: /kdos/build/initramfs.cpio.gz not found!"
    exit 1
fi

# 2. Create System SquashFS
#
# zstd BY DEFAULT, because the medium is read far more often than it is
# written: a zstd image decompresses about 2.6x cheaper than an xz one, on
# every boot and every first launch. Level 15, because 19 and 22 cost more CPU
# than xz does for about 0.6% more space. KDOS_ISO_COMP=zstd:3 is a developer
# image — about 10% larger, for the rig — and never a release; xz remains
# selectable. ANY OTHER VALUE IS FATAL, so a typo cannot quietly ship a medium
# in a codec nobody chose.
ISO_COMP="${KDOS_ISO_COMP:-zstd:15}"
case "$ISO_COMP" in
    xz) SFS_COMP=(-comp xz) ;;
    zstd:[1-9]|zstd:1[0-9]|zstd:2[0-2])
        SFS_COMP=(-comp zstd -Xcompression-level "${ISO_COMP#zstd:}") ;;
    *) echo "FATAL: KDOS_ISO_COMP=$ISO_COMP is not xz or zstd:<1-22>" >&2; exit 1 ;;
esac
echo "Squashing Root Filesystem ($ISO_COMP)..."
# The pseudo filesystems, build artifacts and caches are excluded, and the
# mountpoints they need are recreated as empty directories — an excluded /proc
# is a root that cannot mount one. /var/cache and /var/log come back holding
# only the directories installed packages claim under them, empty, with the
# tree's mode and owner: a daemon creates its own directory there with a single
# mkdir(2), which fails with no parent, and some (smbd's log and cache
# directories, libvirt's) are never created by the daemon at all. A claimed
# directory left out is also `missing` to `kpkg check --all`, which fails a
# pristine image. The installed system is copied from this root. A package
# FILE under either is not carried: the build's logs and caches are what the
# exclusion is for.
#
# `-e` CONSUMES THE REST OF THE COMMAND LINE: every argument after it is an
# exclude pattern, so it goes LAST and every option goes ahead of it. An option
# placed after `-e` is not an error — it is silently taken as a filename to
# exclude, which leaves the compressor at the gzip default, drops every
# pseudo-directory, and appends to an existing system.sfs instead of replacing
# it.
#
# 1 MiB BLOCKS, the largest squashfs has. A block is the unit compressed and
# the unit read: the image is 3-8 % smaller than at the 128 KiB default, and a
# cold random read of a small file pulls up to a whole MiB off the medium and
# decompresses it, which a USB stick serves more slowly.
#
# /ports is the repository's bind mount and is excluded here; the recipe tree
# 105_ports.sh staged is appended below in its place. var/tmp/kdos-home is the
# build's HOME (script/chroot/exec.sh), recreated by every chroot entry after
# 020_cleanup.sh emptied var/tmp.
#
# A pseudo-directory whose parent is neither in the image nor defined first is
# fatal to mksquashfs, so every claimed directory brings its parents, and the
# sorted list puts each parent ahead of its children.
VAR_PSEUDO=/kdos/build/sfs-var.pseudo
{
    echo 'var/cache d 755 0 0'
    echo 'var/log d 755 0 0'
    grep -h -e '^\./var/cache/..*/$' -e '^\./var/log/..*/$' /var/lib/kpkg/db/* |
        while IFS= read -r d; do
            d=${d#./}
            d=${d%/}
            while [ "$d" != var/cache ] && [ "$d" != var/log ]; do
                printf '%s\n' "$d"
                d=${d%/*}
            done
        done |
        LC_ALL=C sort -u |
        while IFS= read -r d; do
            m=$(stat -c '%a %u %g' "/$d" 2>/dev/null) || m="755 0 0"
            printf '"%s" d %s\n' "$d" "$m"
        done
} > $VAR_PSEUDO
mksquashfs / $ISO_ROOT/system.sfs \
    -noappend "${SFS_COMP[@]}" -b 1M \
    -p "proc d 555 0 0" \
    -p "sys d 555 0 0" \
    -p "dev d 755 0 0" \
    -p "tmp d 1777 0 0" \
    -p "run d 755 0 0" \
    -p "mnt d 755 0 0" \
    -p "media d 755 0 0" \
    -pf $VAR_PSEUDO \
    -e proc sys dev tmp run mnt media var/cache var/log var/tmp/kdos-home build kdos ports

# 2a. The recipe tree, appended as /ports.
#
# An append adds the staged directory's entries to the root of the image and
# takes the codec and block size the image already has. A name the first pass
# did not exclude would be appended beside it as `ports_1`, so the result is
# checked by path.
PORTS_STAGE=/kdos/build/image-ports
if [ ! -f $PORTS_STAGE/ports/sources.idx ]; then
    echo "FATAL: $PORTS_STAGE holds no recipe tree; 105_ports.sh did not run" >&2
    exit 1
fi
echo "Appending the recipe tree as /ports..."
mksquashfs $PORTS_STAGE $ISO_ROOT/system.sfs -no-progress -quiet
if ! unsquashfs -l $ISO_ROOT/system.sfs ports/sources.idx | grep -qx 'squashfs-root/ports/sources.idx'; then
    echo "FATAL: system.sfs has no /ports/sources.idx after the append" >&2
    exit 1
fi

# 2b. The KDOS base pack, when KDOS_PACK_KDOS built one.
#
# It goes on ISO9660 rather than into /var/lib/kdos/packs for the reason
# 100_packs.sh records: that directory is inside the rootfs, and squashing a
# compressed image of the rootfs into system.sfs makes the medium carry the
# tree twice. kdos-packd scans the medium for *.kpack and needs no index, so
# `kdos-box create ports base=pack:kdos` finds it with nothing installed.
#
# IT IS THE ONLY PACK A MEDIUM CARRIES. Applications are built by podman on the
# machine that asks for one, or imported from an exported set; nothing is baked.
# GATED ON THE FLAG, NOT ON THE FILE. `build/` is not cleaned between runs, so
# testing whether the artefact EXISTS would carry a 9.1 GB pack from an
# earlier opt-in build onto every ISO after it, including builds that never
# set KDOS_PACK_KDOS. The flag is what the user asked for; the file is just
# what is lying around.
if [ "${KDOS_PACK_KDOS:-0}" = "1" ] && [ -f /kdos/build/kdos-base/kdos.kpack ]; then
    mkdir -p $ISO_ROOT/packs
    cp -a /kdos/build/kdos-base/kdos.kpack $ISO_ROOT/packs/
    echo "KDOS base pack: $(du -h $ISO_ROOT/packs/kdos.kpack | cut -f1) on the medium"
fi

# 2c. The sources, when asked for.
#
# A booted stick that can rebuild its own ISO. The tree goes on the ISO9660
# filesystem BESIDE system.sfs rather than inside it, so it costs the installed
# system nothing and is readable from /mnt/iso the moment the live image is up.
#
# Every image carries the recipes at /ports (2a); this adds the fetched
# sources, script/ and fs/. Opt-in because of its size: ports/ is tens of
# gigabytes of upstream tarballs that are already compressed, and squashing them again
# buys nothing.
# `make build KDOS_ISO_SOURCES=1` is a developer stick, not the default one.
#
# The copy reads the tree where the chroot mounts it: ports/ at /ports, and
# src/, script/, fs/, the Makefile and the Dockerfile bound back over /kdos,
# whose own bind of the build container's /workspace is non-recursive and
# shows each of them as an empty mount point. CLAUDE.md is not mounted into
# the container, and each top-level file is copied only when present. Of
# ports/ everything but its
# dot-directories travels, every fetched source beside its recipe: those are
# the fetch cache, whose bytes each port directory already holds by hard link,
# and compiled host helpers. A binhost in build/binhost goes beside the tree as
# sources/binhost only when KDOS_MAKE_BINHOST is set for this run, for the
# reason 2b gates on its flag: build/binhost survives between runs, and a test
# on its index file alone would put an earlier opt-in build's binhost on every
# sources medium after it.
if [ "${KDOS_ISO_SOURCES:-0}" = "1" ]; then
    echo "Copying the sources onto the ISO (this is the big one)..."
    mkdir -p $ISO_ROOT/sources/ports
    for d in src script fs; do
        cp -a /kdos/$d $ISO_ROOT/sources/
    done
    for f in /ports/*; do
        cp -a "$f" $ISO_ROOT/sources/ports/
    done
    for f in Makefile Dockerfile CLAUDE.md; do
        [ -f /kdos/$f ] && cp -a /kdos/$f $ISO_ROOT/sources/
    done
    if [ "${KDOS_MAKE_BINHOST:-0}" = "1" ] && [ -f /kdos/build/binhost/PACKAGES ]; then
        cp -a /kdos/build/binhost $ISO_ROOT/sources/binhost
    fi
    # A stamp, so `kdos rebuild` can say what it is about to rebuild FROM.
    cat > $ISO_ROOT/sources/SOURCES <<EOS
# The KDOS tree that built this image.
ports    $(find $ISO_ROOT/sources/ports/core -mindepth 2 -maxdepth 3 -name kpkgbuild | wc -l) ports
size     $(du -sh $ISO_ROOT/sources 2>/dev/null | cut -f1)
built    $(date -u +%Y-%m-%dT%H:%M:%SZ)
EOS
    echo "Sources: $(du -sh $ISO_ROOT/sources | cut -f1)"
fi

# 3. The boot menu
#
# ONE BOOTLOADER FOR BOTH FIRMWARES. Limine boots from BIOS and from UEFI out
# of one tree and one configuration file, so what a machine shows at power-on
# does not depend on how it started. A second bootloader for the other firmware
# would mean two menus to keep in step, and the one that is wrong is the one
# nobody is booting today.
echo "Configuring the boot menu..."
LIMINE_DIR=/usr/share/limine
if [ ! -d "$LIMINE_DIR" ]; then
    echo "Error: limine is not installed at $LIMINE_DIR" >&2
    exit 1
fi

mkdir -p $ISO_ROOT/boot/limine
cp $LIMINE_DIR/limine-bios.sys    $ISO_ROOT/boot/limine/
cp $LIMINE_DIR/limine-bios-cd.bin $ISO_ROOT/boot/limine/
cp $LIMINE_DIR/limine-uefi-cd.bin $ISO_ROOT/boot/limine/
cp $LIMINE_DIR/BOOTX64.EFI        $ISO_ROOT/EFI/BOOT/BOOTX64.EFI
# AND THE 32-BIT FIRMWARE'S FIRST STAGE BESIDE IT. A UEFI machine loads
# `EFI/BOOT/BOOT<arch>.EFI` off removable media and reads only the one its own
# firmware can execute, so the two sit together and never compete: a 64-bit
# firmware takes BOOTX64 and an early Atom tablet, whose CPU is 64 bit and whose
# firmware is not, takes BOOTIA32 and boots the same kernel. Both are built by
# the limine port.
cp $LIMINE_DIR/BOOTIA32.EFI       $ISO_ROOT/EFI/BOOT/BOOTIA32.EFI

# THE MENU IS DRAWN IN THE CONSOLE'S OWN FACE, so the first screen of KDOS is
# the same character grid in the same palette as every screen after it.
#
# `ter-i16n` AND NOT THE CONSOLE'S OWN `ter-kdos32n`, for two reasons that both
# fail silently. Limine indexes glyphs by CP437 and only Terminus's `-i` faces
# are encoded that way — the console's leaves 46 slots blank, every double-line
# box glyph among them. And Limine reads 8-dot-wide fonts ONLY: `term_font_size`
# refuses any other width and falls back to the built-in font, which draws a
# perfectly good menu in the wrong typeface. psf2limine.py refuses both
# mistakes rather than converting them.
BOOTFONT=/usr/share/consolefonts/ter-i16n.psf.gz
if [ -f "$BOOTFONT" ]; then
    zcat "$BOOTFONT" > /tmp/kdos-bootfont.psf
    FONT_SIZE=$(python3 /kdos/script/phases/70_image/psf2limine.py \
                /tmp/kdos-bootfont.psf $ISO_ROOT/boot/limine/font.bin)
    rm -f /tmp/kdos-bootfont.psf
    FONT_LINES="term_font: boot():/boot/limine/font.bin
term_font_size: $FONT_SIZE"
else
    echo "Warning: $BOOTFONT not found — the menu keeps Limine's own font"
    FONT_LINES=""
fi

# THE ARTWORK GOES BEHIND EVERYTHING AND THE MENU SITS OPAQUE ON TOP OF IT.
# `centered` draws a wallpaper at its own size in the middle of the screen,
# which is exactly where Limine draws the menu, and `term_background`'s leading
# transparency byte defaults to `80` whenever a wallpaper is set. Either one
# alone prints the artwork THROUGH the entry text. Neither is a taste: they are
# the difference between a menu somebody can read and one they cannot.
#
# The file here is PRE-DIMMED, because Limine has no wallpaper opacity. The
# backdrop generator writes it; how dark it is belongs there and not here.
#
# Only the PATH is set below. `wallpaper_style`, every colour and the font
# scale come out of `kdos-bootctl theme --print`, so the medium, the installer
# and a later `kdos theme` cannot disagree about how the menu looks.
BACKDROP_ART=/usr/share/kdos/boot/kdos-backdrop.png
if [ ! -f "$BACKDROP_ART" ]; then
    BACKDROP_ART=/usr/share/kdos/boot/kdos-banner.png
fi
if [ -f "$BACKDROP_ART" ]; then
    cp "$BACKDROP_ART" $ISO_ROOT/boot/limine/wallpaper.png
    WALLPAPER="wallpaper: boot():/boot/limine/wallpaper.png"
else
    echo "Warning: no KDOS boot artwork — the menu is a plain backdrop"
    WALLPAPER=""
fi

# THE COLOURS COME OUT OF libkcolor AND ARE NEVER WRITTEN HERE. `kdos-bootctl
# theme --print` expands the scheme's own nine numbers into Limine's keys, and
# the installer writes the same block through the same function — so the menu,
# the splash, the console and the desktop are one palette, and a stick and the
# machine installed from it cannot show different colours. A literal picked to
# look right here would drift the moment the scheme is retuned.
#
# TEN SECONDS IS A COUNTDOWN SOMEBODY CAN ACT ON. Every second of it is boot
# time spent before the kernel exists, with nothing else running, so it is
# bought rather than free — but the menu is the only way to reach the verbose
# entry, the clean session and memtest86+, and a machine that will not boot
# needs one of those. A countdown short enough to miss makes them unreachable
# on exactly the machine that needs them, which costs far more than the wait.
# Any keypress cancels it and leaves the menu up indefinitely.
#
# `timeout: 0` is NOT an immediate boot with a menu; it boots the default entry
# without drawing one at all, and those entries become unreachable.
BOOT_THEME=$(kdos-bootctl theme --print "$KDOS_ACCENT")

cat > $ISO_ROOT/boot/limine/limine.conf <<EOF
timeout: 10
default_entry: 1

$BOOT_THEME
$WALLPAPER
$FONT_LINES

/KDOS Live
    comment: Start KDOS from this medium
    protocol: linux
    path: boot():/boot/vmlinuz
    module_path: boot():/boot/initramfs.cpio.gz
    cmdline: root=/dev/ram0 rw console=tty0 console=ttyS0 quiet loglevel=3

/KDOS Live (clean session)
    comment: Ignore the persistence store and start fresh
    protocol: linux
    path: boot():/boot/vmlinuz
    module_path: boot():/boot/initramfs.cpio.gz
    cmdline: root=/dev/ram0 rw console=tty0 console=ttyS0 quiet loglevel=3 nopersist

/KDOS Live (verbose)
    comment: Every kernel message on the console
    protocol: linux
    path: boot():/boot/vmlinuz
    module_path: boot():/boot/initramfs.cpio.gz
    cmdline: root=/dev/ram0 rw console=tty0 console=ttyS0 loglevel=7
EOF

# 3b. memtest86+, and it is a MENU ENTRY rather than a program because bad RAM
# is the one fault no tool running under an OS can honestly diagnose — the OS
# is itself in the memory being tested. It boots INSTEAD of the kernel, owns
# the machine, and tests everything.
#
# The port installs the payload to /usr/share/kdos/memtest86plus/; a missing
# one is a warning and not a failure, so an ISO still rolls on a tree where
# that port has not been built.
#
# THE PAYLOAD IS AN EFI BINARY, so the entry is chainloaded and `if_fw_type`
# hides it on a BIOS boot. Offered there it would be an entry that cannot
# start, on the one screen a machine with bad memory is able to reach.
MEMTEST=/usr/share/kdos/memtest86plus/memtest.efi
if [ -f "$MEMTEST" ]; then
    echo "Adding memtest86+..."
    cp "$MEMTEST" $ISO_ROOT/boot/memtest.efi
    cat >> $ISO_ROOT/boot/limine/limine.conf <<EOF

/Memory Test (memtest86+)
    comment: Test this machine's RAM — UEFI only
    protocol: efi
    if_fw_type: UEFI
    path: boot():/boot/memtest.efi
EOF
else
    echo "memtest86+: no payload at $MEMTEST — skipping the menu entry"
fi

# 4. Generate the ISO
#
# THE IMAGE BOOTS FOUR WAYS AND EACH ONE IS A SEPARATE FLAG. Optical BIOS and
# optical UEFI are the two El Torito records; USB BIOS and USB UEFI are the
# partition table, because `dd` copies bytes and a firmware reading a stick
# never looks in a boot catalogue.
#
#   -b boot/limine/limine-bios-cd.bin   the BIOS El Torito record
#   --efi-boot boot/limine/limine-uefi-cd.bin   the UEFI one
#   -efi-boot-part --efi-boot-image     makes THAT image an EFI System
#                                       Partition in the table, so a written
#                                       stick has one to find
#   --protective-msdos-label            an MBR for the firmware that wants one
#   limine bios-install                 the BIOS boot code in that MBR
#
# THE ESP IS THE EL TORITO IMAGE ITSELF, not a partition appended after the
# ISO. Nothing is added past the ISO9660 volume, so the volume descriptor still
# describes the whole file and `kdos clone` copies a complete medium from it.
#
# ISO9660 STILL STARTS AT SECTOR 0 and must keep doing so: the initramfs finds
# the medium by mounting each whole-disk node `-t iso9660`, which on a written
# stick is /dev/sda itself. A partition offset for the ISO would move the
# filesystem off sector 0 and that scan would find nothing.
echo "Generating ISO..."
ISO_BUILD=/kdos/build/iso-build
mkdir -p $ISO_BUILD
rm -f $ISO_BUILD/efiboot.img

xorriso -as mkisofs \
    -iso-level 3 \
    -full-iso9660-filenames \
    -volid "KDOS_LIVE" \
    -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot boot/limine/limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image \
    --protective-msdos-label \
    -o $ISO_BUILD/kdos.iso \
    $ISO_ROOT

# THE BIOS BOOT CODE GOES IN AFTERWARDS, because it is written into the MBR of
# a finished image: xorriso lays out the volume, then this patches the first
# sector to chain into limine-bios.sys. Without it the UEFI paths still work
# and a BIOS machine reads a disk with no boot code and moves on to the next
# device, which looks exactly like a machine that was never offered the stick.
limine bios-install $ISO_BUILD/kdos.iso

# EVERY BOOT PATH IS VERIFIED, NOT ASSUMED, because all four fail silently.
# xorriso accepts a boot flag it has nothing to act on without a word, and the
# image it then writes can have a first 512 bytes of zero, which nothing shows
# until somebody writes it to a stick. A build that
# cannot boot has to fail here rather than on somebody's machine.
# THE TYPE HAS TWO NAMES AND THE IMAGE ENDS UP WITH THE SECOND ONE.
# `limine bios-install` converts the GPT xorriso wrote into an MBR — it says so
# — because more firmware boots that. fdisk then calls the partition
# `EFI (FAT-12/16/32)` and not `EFI System`, which is the GPT spelling. Matching
# only the GPT name fails a perfectly bootable image, and matching only the MBR
# one would fail if that conversion ever stopped happening.
iso_fail=0
if ! fdisk -l $ISO_BUILD/kdos.iso 2>/dev/null \
     | grep -Eq "EFI System|EFI \(FAT"; then
    echo "FATAL: no EFI System partition — UEFI would not boot from a stick" >&2
    iso_fail=1
fi
if [ "$(dd if=$ISO_BUILD/kdos.iso bs=1 skip=510 count=2 2>/dev/null | od -An -tx1 | tr -d ' \n')" != "55aa" ]; then
    echo "FATAL: no MBR boot signature — BIOS would not boot from a stick" >&2
    iso_fail=1
fi
# The MBR's first 440 bytes are the boot code area. xorriso leaves them ZERO —
# a protective label is a partition table and nothing else — and
# `limine bios-install` is what fills them. All-zero here is a BIOS machine
# reading the stick, finding no code, and moving on to the next boot device,
# which on most firmware is indistinguishable from the stick not being plugged
# in. The 55aa signature above is written by xorriso either way and so cannot
# answer this.
# `od -v` IS LOAD-BEARING: without it od collapses runs of identical lines to a
# single `*`, so the dump of an empty boot area is not all zeros but zeros and
# an asterisk — and every test for "all zero" answers no. That reads as boot
# code being present on an image that has none.
if [ -z "$(dd if=$ISO_BUILD/kdos.iso bs=440 count=1 2>/dev/null \
           | od -An -tx1 -v | tr -d ' \n0')" ]; then
    echo "FATAL: the MBR boot code area is empty — limine bios-install did" >&2
    echo "       not run, so BIOS would not boot from a stick" >&2
    iso_fail=1
fi
[ "$iso_fail" = 0 ] || exit 1

echo "ISO Construction Complete: $ISO_BUILD/kdos.iso"
