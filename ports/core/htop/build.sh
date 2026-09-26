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

# The process-backtrace screen walks another process's stack through
# libunwind-ptrace, and htop's own crash report unwinds through libunwind; both
# are libunwind-nongnu. Naming the backend and --with-libunwind=yes turns a
# missing header into a configure error rather than a build without the
# screen. Demangling stays off: it needs libiberty or libdemangle as a library,
# and neither is installed here, so C++ frames show their mangled names.
./autogen.sh
./configure --prefix=/usr --mandir=/usr/share/man \
	--enable-sensors \
	--enable-delayacct \
	--enable-capabilities \
	--enable-backtrace=unwind-ptrace \
	--with-libunwind=yes \
	--disable-demangling
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/htop.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Process Viewer
GenericName=Process Viewer
Comment=Watch and signal running processes
Exec=htop
Icon=speedometer
Terminal=true
Categories=System;Monitor;
Keywords=process;task;kill;monitor;htop;
EOF
chmod 644 "$PKG/usr/share/applications/htop.desktop"
