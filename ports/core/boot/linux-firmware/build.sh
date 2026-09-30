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

# THE COMPLETE UPSTREAM TREE, NOT A SUBSET.
#
# These are vendor binary blobs. They are the one class of thing on this host
# that is not built from source here, because no source is published for them —
# see the exceptions section in README.md, which names this port first.
#
# Nothing is pruned. A curated subset is a bet on which hardware the machine
# will turn out to have, and losing that bet is silent: request_firmware()
# fails and the device simply does not work, with no diagnostic anywhere. The
# cost of being right every time is measured below and is paid once.
#
# Upstream's own copy-firmware.sh does the install, rather than a copy of the
# tree, because the tree on disk is NOT the installed layout: WHENCE carries
# `Link:` directives that name the aliases drivers actually request, and only
# that script creates them. A plain copy produces a tree with no symlinks at
# all, where every driver asking for an aliased name gets nothing.
#
# --zstd because the kernel is built CONFIG_FW_LOADER_COMPRESS_ZSTD=y and loads
# blobs compressed. Measured on this tree: 1.9 GB extracted -> 921 MB
# installed, 2307 symlinks, none dangling.
#
# Serial, and without dedup. GNU parallel and rdfind are both ports, so
# upstream's `-j` and `make dedup` are reachable; neither is taken because
# neither has been measured here and both change a 921 MB install: `-j` its
# build, dedup its on-disk layout (duplicates become hardlinks, and the
# dangling-symlink check below has to keep passing over the result). Serial
# takes about a minute and the duplicate blobs are inside the 921 MB.

./copy-firmware.sh --zstd "$PKG/lib/firmware"

# A dangling firmware symlink is invisible at runtime — request_firmware()
# fails and the device just does not work — so the build stops rather than
# shipping one.
dangling=0
while IFS= read -r l; do
	[ -e "$l" ] || { echo "dangling firmware symlink: $l" >&2; dangling=$((dangling + 1)); }
done < <(find "$PKG/lib/firmware" -type l)
if [ "$dangling" -ne 0 ]; then
	echo "ERROR: $dangling dangling firmware symlink(s)" >&2
	exit 1
fi

# WHENCE is upstream's provenance and licence map: it says who owns each blob
# and under what terms. It is the document a redistributor needs, so it ships
# with the blobs rather than being left in the tarball.
install -d "$PKG/usr/share/licenses/$name"
install -m644 WHENCE "$PKG/usr/share/licenses/$name/"
install -m644 LICENSE* "$PKG/usr/share/licenses/$name/" 2>/dev/null || true
