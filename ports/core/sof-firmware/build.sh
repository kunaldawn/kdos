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

# NOT in linux-firmware. Upstream ships SOF separately, and
# CONFIG_SND_SOC_SOF=m binds a driver that then requests these blobs by name.
# Without them a machine with a working audio device and a bound driver makes
# no sound at all.
#
# Upstream's install.sh wants a versioned target and sudo. Laying the tree out
# here instead keeps the package manifest honest about every path it owns.

install -dm755 "$PKG/lib/firmware/intel"

# Two trees, and both are required. sof/ is the DSP firmware; sof-tplg/ is the
# topology set that binds a firmware image to a machine's specific codec and
# speaker layout. Firmware with no topology loads and binds nothing, which is
# still silence — `kdos doctor` therefore checks for both separately.
# The order matters: sof-ace-tplg is a SYMLINK to sof-ipc4-tplg upstream, so
# the target is copied first and the link resolves rather than dangling. cp -a
# preserves it as a link rather than duplicating ~40 MB of topologies.
for d in sof sof-tplg sof-ipc4 sof-ipc4-lib sof-ipc4-tplg sof-ace-tplg; do
	[ -e "$d" ] && cp -a "$d" "$PKG/lib/firmware/intel/"
done

# `tools/` and `install.sh` are upstream's own installer and are deliberately
# not shipped: kpkg owns installation here, and a second installer inside the
# package is a second answer to where these files go.

install -dm755 "$PKG/usr/share/licenses/$name"
for l in LICENCE.Intel LICENCE.NXP Notice.NXP README.Intel; do
	[ -f "$l" ] && install -m644 "$l" "$PKG/usr/share/licenses/$name/"
done
