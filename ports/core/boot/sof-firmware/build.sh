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

install -dm755 "$PKG/lib/firmware/intel"

for d in sof sof-tplg sof-ipc4 sof-ipc4-lib sof-ipc4-tplg sof-ace-tplg; do
	[ -e "$d" ] && cp -a "$d" "$PKG/lib/firmware/intel/"
done

install -dm755 "$PKG/usr/share/licenses/$name"
for l in LICENCE.Intel LICENCE.NXP Notice.NXP README.Intel; do
	[ -f "$l" ] && install -m644 "$l" "$PKG/usr/share/licenses/$name/"
done
