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
echo "Squashing Root Filesystem..."
# The pseudo filesystems, build artifacts and caches are excluded, and the
# mountpoints they need are recreated as empty directories — an excluded /proc
# is a root that cannot mount one.
#
# `-e` CONSUMES THE REST OF THE COMMAND LINE: every argument after it is an
# exclude pattern, so it goes LAST and every option goes ahead of it. An option
# placed after `-e` is not an error — it is silently taken as a filename to
# exclude, which leaves the compressor at the gzip default, drops every
# pseudo-directory, and appends to an existing system.sfs instead of replacing
# it.
mksquashfs / $ISO_ROOT/system.sfs \
    -noappend -comp xz \
    -p "proc d 555 0 0" \
    -p "sys d 555 0 0" \
    -p "dev d 755 0 0" \
    -p "tmp d 1777 0 0" \
    -p "run d 755 0 0" \
    -p "mnt d 755 0 0" \
    -p "media d 755 0 0" \
    -e proc sys dev tmp run mnt media var/cache var/log build kdos ports

# 2a. The KDOS base pack, when KDOS_PACK_KDOS built one.
#
# It goes on ISO9660 rather than into /var/lib/kdos/packs for the reason
# 01_packs.sh records: that directory is inside the rootfs, and squashing a
# compressed image of the rootfs into system.sfs makes the medium carry the
# tree twice. kdos-packd scans the medium for *.kpack and needs no index, so
# `kdos-box create ports base=pack:kdos` finds it with nothing installed.
#
# IT IS THE ONLY PACK A MEDIUM CARRIES. Applications are built by podman on the
# machine that asks for one, or imported from an exported set; nothing is baked.
# GATED ON THE FLAG, NOT ON THE FILE. `build/` is not cleaned between runs, so
# testing whether the artefact EXISTS carries a 9.1 GB pack from an earlier
# opt-in build onto every ISO after it — measured, on a build that never set
# KDOS_PACK_KDOS at all. The flag is what the user asked for; the file is just
# what is lying around.
if [ "${KDOS_PACK_KDOS:-0}" = "1" ] && [ -f /kdos/build/kdos-base/kdos.kpack ]; then
    mkdir -p $ISO_ROOT/packs
    cp -a /kdos/build/kdos-base/kdos.kpack $ISO_ROOT/packs/
    echo "KDOS base pack: $(du -h $ISO_ROOT/packs/kdos.kpack | cut -f1) on the medium"
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
ports    $(ls /ports/core | wc -l) ports
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
# ports/core/limine.
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
    FONT_SIZE=$(python3 /kdos/script/util/psf2limine.py \
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
# xorriso accepts a boot flag it does not implement without a word — the
# previous `-isohybrid-gpt-basdat` did, and produced an image whose first 512
# bytes were zero for as long as nobody wrote one to a stick. A build that
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
