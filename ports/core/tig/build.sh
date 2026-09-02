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

# lazygit is the tool that ACTS on a repository; this is the one that reads it,
# and the two are worth having separately — a blame or a log is where most time
# in a repository goes.
./configure --prefix=/usr --sysconfdir=/etc
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/tig.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Git History
GenericName=Version Control
Comment=Browse a repository's history
Exec=tig
Icon=vcs-normal
Terminal=true
Categories=Development;RevisionControl;
Keywords=git;log;history;blame;tig;
EOF
chmod 644 "$PKG/usr/share/applications/tig.desktop"
