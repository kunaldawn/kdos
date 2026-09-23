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


# BINDIR IS /usr/sbin AND THE DEFAULT IS /sbin. The initramfs and the init
# scripts resolve it on $PATH, which puts /usr/sbin on it and /sbin last;
# installing to the default leaves two plausible homes for one binary.
#
# COROSYNC AND THE CLUSTER HALF ARE OFF. The Makefile builds clustered RAID
# against corosync and dlm whenever pkg-config finds them; COROSYNC and DLM on
# the command line replace that probe with its "not found" answer.
#
# THE FLAGS GO ON THE COMMAND LINE. The Makefile assigns CFLAGS with `=`, so an
# exported CFLAGS never reaches the compiler. CXFLAGS is the slot it leaves for
# extra flags, and it follows the -Werror in CWFLAGS, so the -Wno-error in it
# wins. CWFLAGS itself is left alone: it carries -fwrapv and
# -fno-delete-null-pointer-checks, which the code relies on.
#
# libudev is linked unconditionally unless CXFLAGS names -DNO_LIBUDEV.
MDADM_MAKE=(BINDIR=/usr/sbin COROSYNC=-DNO_COROSYNC DLM=-DNO_DLM CXFLAGS="$CFLAGS -Wno-error")
make "${MDADM_MAKE[@]}"
make DESTDIR=$PKG "${MDADM_MAKE[@]}" install

# THE INITRAMFS ASSEMBLES BEFORE IT MOUNTS, so a config naming arrays by UUID
# is not what runs there — `mdadm --assemble --scan` walks every disk. What
# this ships is the empty file its absence would make noisy: mdadm warns on
# every invocation when /etc/mdadm.conf is missing.
install -d "$PKG/etc"
cat > "$PKG/etc/mdadm.conf" <<'KDOS_SH'
# Arrays are found by scanning rather than by being listed here: a machine
# whose disks moved is a machine whose listed device paths are wrong, and the
# superblock on each member already carries the array's identity.
DEVICE partitions
KDOS_SH
