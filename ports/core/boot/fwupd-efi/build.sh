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

# fwupdx64.efi is linked with ld and objcopy against gnu-efi's crt0 and linker
# script, found by meson under /usr/lib; the compiler flags are the project's
# own, so the chroot's CFLAGS and LDFLAGS never reach an EFI object.
#
# The libexecdir is meson's default, /usr/libexec, because fwupd looks for the
# binary under its own libexecdir, which is the same default. Moving one
# without the other leaves the capsule plugin reporting the binary missing.
#
# The NX-compatible flag is set in the PE header by python3-pefile; without
# genpeimg or pefile the build stops at the last step rather than shipping a
# binary that firmware enforcing NX refuses to load.
#
# The efi_sbat_* values are the distribution's line in the binary's .sbat
# section, which Secure Boot revocation reads. KDOS boots with Secure Boot off,
# but the section still names who built the binary.
meson setup build --prefix=/usr --libdir=lib --buildtype=release \
	-Dgenpeimg=disabled \
	-Defi_sbat_distro_id=kdos \
	-Defi_sbat_distro_summary=KDOS \
	-Defi_sbat_distro_pkgname=$name \
	-Defi_sbat_distro_version=$version-$release \
	-Defi_sbat_distro_url=https://github.com/kunaldawn/kdos
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
