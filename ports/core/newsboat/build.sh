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

# The documentation is asciidoctor's and asciidoctor is not a port, so only the
# programs are built and installed. `install-newsboat` and `install-podboat`
# are the two targets that carry no documentation dependency.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# The Makefile drives cargo itself and passes no offline flag, so the switch
# has to reach it through the environment or the build reaches for the network
# the chroot does not have.
export CARGO_NET_OFFLINE=true

# -lintl, WHICH THE MAKEFILE WILL NOT ADD HERE. It appends `-liconv -lintl`
# only when `uname -s` is not Linux, taking Linux to mean glibc — where
# gettext is in libc. This is musl, where it is not, so the one case that
# needs the flag is the case the test excludes and the link fails on
# libintl_gettext. It goes in LDFLAGS because that is the only variable of the
# three that survives: NEWSBOAT_LIBS and PODBOAT_LIBS are plain assignments the
# Makefile clobbers. LDFLAGS is last on the link line, after the objects, which
# is where a library reference has to be.
#
# NOT -liconv: musl carries iconv in libc and there is no library of that name.
export LDFLAGS="-lintl"

# THE DEFAULT TARGET IS `doc $(NEWSBOAT) $(PODBOAT) mo-files`, AND THE DOCS ARE
# asciidoctor's — not a port here, so `make` alone stops at doc/newsboat.1 with
# status 127. The two programs are named directly instead; `install-newsboat`
# and `install-podboat` each depend on one program and nothing else, so no
# manual page is built and none is packaged.
make prefix=/usr newsboat podboat
make prefix=/usr DESTDIR=$PKG install-newsboat install-podboat

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/newsboat.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Feeds
GenericName=Feed Reader
Comment=Read RSS and Atom feeds
Exec=newsboat
Icon=mail-message
Terminal=true
Categories=Network;News;
Keywords=rss;atom;feed;news;reader;newsboat;
ENTRY
chmod 644 "$PKG/usr/share/applications/newsboat.desktop"
