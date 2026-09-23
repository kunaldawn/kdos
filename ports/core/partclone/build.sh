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

autoreconf -f -i

# ddrescue is for a DYING disk and reads every sector; this is for a healthy
# one and reads only the blocks the filesystem says are in use. Both ship,
# because they answer different questions.
#
# One --enable per filesystem, and each is gated on its library being present:
# without the flag the tool is simply not built, so a recipe that claims ext4
# and omits it produces a partclone that cannot read ext4.
#
# --enable-ntfs builds partclone.ntfs against libntfs-3g and <ntfs-3g/*.h>,
# which the ntfs-3g port installs. configure answers a missing library with a
# warning, not an error, and builds no partclone.ntfs, so ntfs-3g is in
# depends. hfsp and apfs are built in and need no library.
#
# --enable-ncursesw is the -N progress screen; --enable-fuse builds imgfuse,
# which mounts an image as block files without restoring it, against fuse3.
# Both fail configure when their library is missing.
#
# openssl, zlib and zstd are unconditional: configure stops without them.
#
# ITS MAN PAGES NAME THEIR STYLESHEET BY URL — `docbook.sourceforge.net/
# release/xsl/current/manpages/docbook.xsl` — and `make build` runs
# `--network none`, so xsltproc fails and takes the whole build with it.
#
# THE STYLESHEET IS ON THIS MACHINE: `docbook-xsl` is a phase-3 port and
# /etc/xml/catalog even carries the rewrite rule for that exact URL. So the
# answer is to USE it rather than to drop the man pages — docs/Makefile names
# the file in a plain `MAN_STYLESHEET=` variable, which is a flag, not a patch.
#
# The path is globbed rather than written out so a docbook-xsl version bump
# does not silently stop producing documentation, and the build FAILS LOUDLY
# when it is absent: a man page that vanishes because a dependency moved is
# exactly the kind of quiet loss this recipe is avoiding.
#
# XML_CATALOG_FILES is exported for the other half of the resolution — the
# input .xml files carry a DocBook DOCTYPE, and the catalog is what maps that
# to the `docbook-xml` DTDs already installed beside the stylesheets. Nothing
# in this tree exports it, and libxml2 finding /etc/xml/catalog by default is
# not something to rely on inside a chroot.
export XML_CATALOG_FILES=/etc/xml/catalog

MAN_XSL=$(ls -d /usr/share/xml/docbook/xsl-stylesheets-*/manpages/docbook.xsl 2>/dev/null | head -1)
if [ -z "$MAN_XSL" ]; then
	echo "ERROR: docbook-xsl manpages stylesheet not found under /usr/share/xml/docbook" >&2
	exit 1
fi
./configure \
	--prefix=/usr \
	--enable-extfs \
	--enable-fat \
	--enable-exfat \
	--enable-ntfs \
	--enable-hfsp \
	--enable-apfs \
	--enable-btrfs \
	--enable-xfs \
	--enable-f2fs \
	--enable-ncursesw \
	--enable-fuse
make MAN_STYLESHEET="$MAN_XSL"
make MAN_STYLESHEET="$MAN_XSL" DESTDIR=$PKG install
