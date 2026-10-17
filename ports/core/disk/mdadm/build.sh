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

MDADM_MAKE=(BINDIR=/usr/sbin COROSYNC=-DNO_COROSYNC DLM=-DNO_DLM CXFLAGS="$CFLAGS -Wno-error")
make "${MDADM_MAKE[@]}"
make DESTDIR=$PKG "${MDADM_MAKE[@]}" install

install -d "$PKG/etc"
cat > "$PKG/etc/mdadm.conf" <<'EOF'
# Arrays are found by scanning rather than by being listed here: a machine
# whose disks moved is a machine whose listed device paths are wrong, and the
# superblock on each member already carries the array's identity.
DEVICE partitions
EOF
