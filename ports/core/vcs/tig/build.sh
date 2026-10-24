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

./configure --prefix=/usr --sysconfdir=/etc --with-ncursesw
for def in HAVE_PCRE2 HAVE_READLINE HAVE_NCURSESW; do
	grep -q "^#define $def 1" config.h || { echo "tig: $def not configured" >&2; exit 1; }
done
make
make DESTDIR=$PKG install
install -Dm644 doc/tig.1 -t "$PKG/usr/share/man/man1"
install -Dm644 doc/tigrc.5 -t "$PKG/usr/share/man/man5"
install -Dm644 doc/tigmanual.7 -t "$PKG/usr/share/man/man7"

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
NoDisplay=true
Categories=Development;RevisionControl;
Keywords=git;log;history;blame;tig;
EOF
chmod 644 "$PKG/usr/share/applications/tig.desktop"
