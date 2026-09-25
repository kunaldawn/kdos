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

# musl has no statvfs64; its statvfs is already 64-bit. The flag is exported
# rather than passed to make, which would replace the phase's CXXFLAGS and
# with them the prefix map that keeps the build path out of the binary.
#
# The GPU box is built, and on Intel graphics it reads the i915 PMU through
# perf_event_open. That is a system-wide event, which the kernel's default
# perf_event_paranoid of 2 refuses without CAP_PERFMON, so for the desktop user
# the Intel panel is empty; run as root, it fills. Nothing grants the
# capability to the binary.
export CXXFLAGS="$CXXFLAGS -Dstatvfs64=statvfs"
make QUIET=true PREFIX=/usr
make DESTDIR=$PKG PREFIX=/usr QUIET=true install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/btop.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=System Monitor
GenericName=Resource Monitor
Comment=Processes, CPU, memory, disks and network
Exec=btop
Icon=speedometer
Terminal=true
Categories=System;Monitor;
Keywords=process;cpu;memory;task;monitor;btop;
X-KDOS-Float=true
X-KDOS-Size=100x30
EOF
chmod 644 "$PKG/usr/share/applications/btop.desktop"
