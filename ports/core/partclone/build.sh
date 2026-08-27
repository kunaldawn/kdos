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
# --disable-ntfs, AND THE REASON IS NOT THAT NTFS DOES NOT MATTER. partclone's
# NTFS support wants ntfsprogs' INTERNAL headers (libntfs), which the ntfs-3g
# port does not install — it ships libntfs-3g, a different library. configure
# answers the absence with a warning and builds no partclone.ntfs, so asking
# for it produces a recipe whose claim and whose output disagree. `ntfsclone`
# from ntfsprogs is the tool for that filesystem and it is already installed.
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
	--disable-ntfs \
	--enable-btrfs \
	--enable-xfs \
	--enable-f2fs
make MAN_STYLESHEET="$MAN_XSL"
make MAN_STYLESHEET="$MAN_XSL" DESTDIR=$PKG install
