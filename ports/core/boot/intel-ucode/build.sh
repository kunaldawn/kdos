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

install -d "$PKG/usr/lib/firmware"
cat intel-ucode/* > "$PKG/usr/lib/firmware/intel-ucode.bin"
chmod 644 "$PKG/usr/lib/firmware/intel-ucode.bin"

_bundle="$PKG/usr/lib/firmware/intel-ucode.bin"
_end=$(stat -c %s "$_bundle")
_off=0
_n=0
while [ "$_off" -lt "$_end" ]; do
	_hdr=$(od -An -tu4 -j "$_off" -N 40 -v "$_bundle")
	set -- $_hdr
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
