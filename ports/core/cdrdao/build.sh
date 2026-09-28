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

# Disc-at-once CD writing: K3b runs cdrdao for audio CDs, CD-TEXT and
# copies, and without it those modes are greyed out. The PCCTS parser
# generator is the bundled copy under pccts/, found by configure when no
# system one is installed.
#
# gcdmaster, the GTK front end, stays off: K3b is the desktop's burner. Ogg
# and MP3 tracks in a TOC file decode through libvorbisfile and libmad and
# play through libao; toc2mp3 encodes with LAME.
./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man \
	--without-gcdmaster \
	--with-ogg-support \
	--with-mp3-support \
	--with-lame \
	--with-posix-threads
make
make DESTDIR=$PKG install
