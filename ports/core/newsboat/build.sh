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

# THE GOALS ARE NAMED, NOT `make` AND `make install`. The default target's
# `doc` builds the two manual pages and the HTML manual and FAQ, and
# `install-docs` installs that HTML, the changelog and contrib/ beside the
# pages. The pages are their own targets — asciidoctor with upstream's
# doc/man.rb converter — and are installed by hand, so no HTML is built.
make prefix=/usr newsboat podboat mo-files doc/newsboat.1 doc/podboat.1
make prefix=/usr DESTDIR=$PKG install-newsboat install-podboat install-mo
install -Dm644 doc/newsboat.1 doc/podboat.1 -t "$PKG/usr/share/man/man1"

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
