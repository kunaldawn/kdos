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

# libvirt runs swtpm_setup as tss for every system-mode guest with an emulated
# TPM, and swtpm_localca refuses a CA statedir it cannot read and write: the
# guest never starts. kpkg rolls every package root:root, so the directory is
# given to tss here, 0750 as upstream's own install step leaves it.
#
# Everything is written under PKG_ROOT, the root kpkgadd is installing into.
# tss's uid is read from the target's passwd as a number: chown resolves a name
# against the running root, not this one.
root="${PKG_ROOT:-/}"
dir="$root/var/lib/swtpm-localca"
[ -d "$dir" ] || exit 0
while IFS=: read -r n _ uid _; do
	[ "$n" = tss ] || continue
	chown -R "$uid:0" "$dir"
	chmod 0750 "$dir"
done < "$root/etc/passwd"
