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

# `source` is empty and the tarball is committed beside this recipe, the same
# shape kdos-splash and the art packages use: what ships is a VENDORED subset,
# not upstream's 1.9 GB tree. ports/core/linux-firmware/vendor.py is the
# host-only tool that produced it and records exactly what was dropped.
#
# Blobs are already .zst. The kernel is built CONFIG_FW_LOADER_COMPRESS_ZSTD=y
# and loads them compressed, so they are installed verbatim — decompressing
# here would triple the installed size for nothing.
# kpkg verifies `source` archives before extracting, but this port has no
# source — the tarball is a committed artefact. So the check happens here
# instead, against the same `sha256 =` key, and the recipe is not trusted to
# be decorative.
_want=$(sed -n 's/^sha256[[:blank:]]*=[[:blank:]]*\([0-9a-f]*\).*/\1/p' \
	"$PORT_SRC/kpkgbuild")
_got=$(sha256sum "$PORT_SRC/linux-firmware-$version.tar.zst" | cut -d' ' -f1)
if [ "$_want" != "$_got" ]; then
	echo "ERROR: linux-firmware tarball sha256 mismatch" >&2
	echo "  expected $_want" >&2
	echo "  got      $_got" >&2
	exit 1
fi

install -d "$PKG/lib/firmware"
tar -xf "$PORT_SRC/linux-firmware-$version.tar.zst" -C "$PKG/lib/firmware"

# A dangling firmware symlink is invisible at runtime: request_firmware()
# fails and the device just does not work. vendor.py measured zero symlinks in
# the tree, so this must find nothing — if it ever does, the vendoring changed
# and the build should stop rather than ship dead paths.
if find "$PKG/lib/firmware" -type l ! -exec test -e {} \; -print | grep -q .; then
	echo "ERROR: dangling firmware symlinks in the vendored tree" >&2
	exit 1
fi

install -Dm644 "$PKG/lib/firmware/WHENCE" \
	"$PKG/usr/share/licenses/linux-firmware/WHENCE"
