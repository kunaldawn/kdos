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
# COROSYNC AND THE CLUSTER HALF ARE OFF. `mdadm` builds clustered RAID
# support against corosync and dlm when it finds them and silently without
# when it does not — the explicit flag is what keeps the recipe's claim and
# the binary the same thing.
export CFLAGS="$CFLAGS -Wno-error"
make CONFIG_LIBUDEV=1 BINDIR=/usr/sbin
make DESTDIR=$PKG BINDIR=/usr/sbin install

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
