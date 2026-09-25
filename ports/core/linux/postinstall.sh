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

# The kernels an installed machine boots are the ones on its ESP, not the one
# in /boot: Limine cannot read every root filesystem the installer offers, so
# each root slot's kernel and initramfs are copied to EFI/kdos/<slot>/ there,
# and an ESP no deploy has touched holds one flat pair in EFI/kdos/ that either
# slot boots. The ESP is the RUNNING system's /boot/efi whichever root this
# package is going into.
esp=/boot/efi/EFI/kdos

# The version a bzImage carries: the boot protocol's kernel_version field at
# 0x20E points, relative to 0x200, at a string that starts with it. Empty for
# anything that is not a bzImage.
kernel_version() {
	[ "$(dd if="$1" bs=1 skip=514 count=4 2>/dev/null)" = HdrS ] || return 0
	off=$(od -An -tu2 -j 526 -N 2 "$1" 2>/dev/null | tr -d ' ')
	[ -n "$off" ] && [ "$off" -gt 0 ] || return 0
	dd if="$1" bs=1 skip=$((off + 512)) count=128 2>/dev/null |
		tr '\0' '\n' | head -n 1 | cut -d' ' -f1
}

esp_versions=
for k in "$esp/vmlinuz" "$esp"/a/vmlinuz "$esp"/b/vmlinuz; do
	[ -f "$k" ] && esp_versions="$esp_versions $(kernel_version "$k")"
done

# Remove module trees of every other kernel. Two kinds stay. The running
# kernel's, when the root is the running system: its modules must keep loading
# until the reboot. And every kernel's on the ESP, in every root: until this
# root's own kernel reaches the ESP, the next boot of it runs one of those, and
# a root stripped of that kernel's modules boots with no GPU, network or sound
# driver and nothing saying why.
running=
[ -z "$root" ] && running=$(uname -r)
for d in "$root"/lib/modules/*/; do
	[ -d "$d" ] || continue
	ver=$(basename "$d")
	[ "$ver" = "$version" ] && continue
	[ "$ver" = "$running" ] && continue
	case " $esp_versions " in *" $ver "*) continue ;; esac
	echo "Removing stale kernel modules: $ver"
	rm -rf "$d"
done

echo "Generating modules.dep for $version..."
if command -v depmod >/dev/null; then
	depmod -b "${root:-/}" -a "$version"
else
	echo "Warning: depmod not found, skipping."
fi

# The new kernel's initramfs, kept in the root beside the kernel as
# /boot/initramfs-kdos.cpio.gz: `kdos-bootctl deploy` and kinstall take it
# over the image's own, which is built for the image's kernel. The image's
# initramfs stays the base — it carries the microcode, which the early loader
# finds only at the very start of the file, and the init — and a second
# archive is appended after it holding this kernel's copies of the modules the
# base carries for the image's kernel. The kernel unpacks concatenated archives
# in order into one tree, and the init's modprobe looks under `uname -r`, so it
# finds the new set.
#
# The set is /boot/initramfs.modules where the image wrote one. A root with no
# such file takes the set the base itself carries — every module under
# lib/modules/ in it — because without vfat, xfs, f2fs, dm-crypt and md in the
# new kernel's copies, its init cannot read the boot state, count an attempt
# or mount any root those modules serve.
archive_modules() (
	f="$1" off=0
	# Uncompressed newc archives at the front — the microcode — are walked
	# header by header; the compressed part starts after the zero padding
	# that follows the last one's trailer.
	while [ "$(dd if="$f" bs=1 skip=$off count=6 2>/dev/null | tr -d '\0')" = 070701 ]; do
		fsz=$((16#$(dd if="$f" bs=1 skip=$((off + 54)) count=8 2>/dev/null)))
		nsz=$((16#$(dd if="$f" bs=1 skip=$((off + 94)) count=8 2>/dev/null)))
		name=$(dd if="$f" bs=1 skip=$((off + 110)) count=$((nsz - 1)) 2>/dev/null)
		off=$(( (off + 110 + nsz + 3) / 4 * 4 ))
		off=$(( (off + fsz + 3) / 4 * 4 ))
		[ "$name" = 'TRAILER!!!' ] || continue
		while [ "$(od -An -tx1 -j $off -N 4 "$f" 2>/dev/null | tr -d ' \n')" = 00000000 ]; do
			off=$((off + 4))
		done
	done
	tail -c +$((off + 1)) "$f" | gzip -dc 2>/dev/null | cpio -t 2>/dev/null |
		while read -r p; do
			case "$p" in
				lib/modules/*/*.ko*|./lib/modules/*/*.ko*) ;;
				*) continue ;;
			esac
			p=${p##*/}
			echo "${p%%.ko*}"
		done | sort -u
)

build_initramfs() {
	out="$1" base="$root/boot/initramfs.cpio.gz" list="$root/boot/initramfs.modules"
	if [ ! -f "$base" ]; then
		echo "No $base: cannot build an initramfs for $version"
		return 1
	fi
	if [ -f "$list" ]; then
		mods=$(cat "$list")
	else
		mods=$(archive_modules "$base")
	fi
	if [ -z "$mods" ]; then
		echo "No module set in $list or $base: cannot build an" \
			"initramfs for $version"
		return 1
	fi
	work=$(mktemp -d) || return 1
	mkdir -p "$work/lib/modules/$version"
	for m in $mods; do
		modprobe -d "${root:-/}" -S "$version" --show-depends "$m" 2>/dev/null
	done |
	while read -r kind path _; do
		[ "$kind" = insmod ] || continue
		rel=${path#*/lib/modules/$version/}
		dest=$work/lib/modules/$version/${rel%.zst}
		[ -e "$dest" ] && continue
		mkdir -p "${dest%/*}"
		case "$path" in
			*.zst) zstd -q -d -c "$path" > "$dest" ;;
			*) cp "$path" "$dest" ;;
		esac
	done
	for f in modules.order modules.builtin modules.builtin.modinfo; do
		[ -f "$root/lib/modules/$version/$f" ] &&
			cp "$root/lib/modules/$version/$f" "$work/lib/modules/$version/"
	done
	depmod -b "$work" "$version" &&
	(cd "$work" && find . | cpio -o -H newc 2>/dev/null) | gzip -9 > "$work.gz" &&
	cat "$base" "$work.gz" > "$out"
	rc=$?
	rm -rf "$work" "$work.gz"
	return $rc
}

# A failed build removes the previous one too: it belongs to the kernel this
# package just replaced. `kdos-bootctl deploy` then finds only the image's
# initramfs, and refuses it beside a kernel whose modules it does not carry.
initrd="$root/boot/initramfs-kdos.cpio.gz"
if build_initramfs "$initrd.new"; then
	mv -f "$initrd.new" "$initrd"
else
	rm -f "$initrd.new" "$initrd"
	echo "Warning: no initramfs for $version — a deploy of this root" \
		"is refused until one is built"
fi

# Put this kernel on the ESP, into the directory of the slot this root is.
# Installing into the running system, that happens here; installing into
# another root — `kdos update` filling the inactive slot — it is `kdos update`
# that deploys, once every package of the run has gone in, because the slot's
# kernel must not reach the ESP beside a half-updated root.
if [ -n "$root" ]; then
	echo "Installed into $root: its kernel reaches the ESP with" \
		"\`kdos-bootctl deploy $root\`, which \`kdos update\` runs"
elif [ ! -d "$esp" ] || ! mountpoint -q /boot/efi 2>/dev/null; then
	:
elif ! command -v kdos-bootctl >/dev/null; then
	echo "Warning: no kdos-bootctl — $version is not on the ESP"
else
	echo "Putting $version on the ESP..."
	kdos-bootctl deploy / ||
		echo "Warning: $version is not on the ESP; the next boot runs" \
			"the kernel it ran before"
fi
