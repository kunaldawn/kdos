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
#
# --with-ncursesw makes wide ncurses a configure error when it is missing;
# without it a narrow ncurses is taken instead and UTF-8 is drawn wrong.
# pcre2 and readline have no such switch: each is used if found, and a tig
# without them still builds, with POSIX regex search and no line editing at
# the prompt. config.h is checked for all three.
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
Categories=Development;RevisionControl;
Keywords=git;log;history;blame;tig;
EOF
chmod 644 "$PKG/usr/share/applications/tig.desktop"
