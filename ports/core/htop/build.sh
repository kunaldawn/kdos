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

# The process-backtrace screen and the crash backtrace want the nongnu
# libunwind with libunwind-ptrace; the libunwind port is LLVM's, which has
# neither, so both stay off. Demangling only serves those backtraces.
./autogen.sh
./configure --prefix=/usr --mandir=/usr/share/man \
	--enable-sensors \
	--enable-delayacct \
	--enable-capabilities \
	--enable-backtrace=no \
	--without-libunwind \
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
