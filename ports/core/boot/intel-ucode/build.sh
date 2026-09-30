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

# A microcode "bundle" is nothing but the per-family files concatenated — the
# kernel walks the blob, matches on signature and processor flags, and keeps the
# highest revision it finds. So the whole build is one cat; the glob is sorted by
# the shell, and every phase env file already pins LC_ALL=C, so the byte order of
# the result is the same on any builder.
#
# intel-ucode-with-caveats/ is deliberately NOT included. Upstream ships those
# separately because they need coordinated BIOS/firmware support and must not be
# applied by the early loader on their own; today that is 06-4f-01 (Broadwell-EP)
# and it is upstream's call to make, not ours.
#
# Nothing prunes by family. A curated microcode set boots on the machines it
# covers and leaves every other one silently unpatched, which is the failure
# mode this port exists to remove.
install -d "$PKG/usr/lib/firmware"
cat intel-ucode/* > "$PKG/usr/lib/firmware/intel-ucode.bin"
chmod 644 "$PKG/usr/lib/firmware/intel-ucode.bin"

# The kernel's scan_microcode() walks the bundle record by record and ends with
#
#     return size ? NULL : patch;
#
# so a bundle that does not consume to exactly its last byte loads NOTHING —
# not "everything up to the bad record", nothing at all, silently. One stray
# file in intel-ucode/ (a README, a .sig) is enough. So the same walk runs here,
# where it can stop the build instead: header_version must be 1 and total_size
# (offset 32, or 2048 when data_size is 0) must land exactly on the end.
_bundle="$PKG/usr/lib/firmware/intel-ucode.bin"
_end=$(stat -c %s "$_bundle")
_off=0
_n=0
while [ "$_off" -lt "$_end" ]; do
	_hdr=$(od -An -tu4 -j "$_off" -N 40 -v "$_bundle")
	set -- $_hdr
	# $1 header_version  $8 data_size  $9 total_size
	if [ "$1" != "1" ]; then
		echo "ERROR: intel-ucode: record at $_off is not microcode" >&2
		exit 1
	fi
	_size=$9
	[ "$8" = "0" ] && _size=2048
	if [ "$_size" -le 0 ] || [ $((_off + _size)) -gt "$_end" ]; then
		echo "ERROR: intel-ucode: record at $_off runs past the bundle" >&2
		exit 1
	fi
	_off=$((_off + _size))
	_n=$((_n + 1))
done
echo "intel-ucode: $_n microcode records, $_end bytes, walk ends clean"

install -Dm644 license "$PKG/usr/share/licenses/$name/LICENSE"
