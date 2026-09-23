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

# Everything happens under PKG_ROOT, the root kpkgadd is installing into: an A/B
# update installs into the inactive slot, and working on / instead would strip
# the running system's modules and index the wrong tree.
root="${PKG_ROOT:-/}"
root="${root%/}"

# Remove module trees of every other kernel. When the root is the running
# system, the running kernel's tree stays: its modules must keep loading until
# the reboot.
running=
[ -z "$root" ] && running=$(uname -r)
for d in "$root"/lib/modules/*/; do
	[ -d "$d" ] || continue
	ver=$(basename "$d")
	[ "$ver" = "$version" ] && continue
	[ "$ver" = "$running" ] && continue
	echo "Removing stale kernel modules: $ver"
	rm -rf "$d"
done

echo "Generating modules.dep for $version..."
if command -v depmod >/dev/null; then
	depmod -b "${root:-/}" -a "$version"
else
	echo "Warning: depmod not found, skipping."
fi
