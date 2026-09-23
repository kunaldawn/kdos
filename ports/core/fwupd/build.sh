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

# The `xmlb` dependency carries `fallback: ['libxmlb', 'libxmlb_dep']` and the
# tarball ships that wrap, so a build root without the libxmlb port makes meson
# download and build a private copy instead of failing. --wrap-mode=nodownload
# turns a missing dependency back into an error: the chroot has no network, and
# a second libxmlb inside fwupd is not the one the rest of the image links.
#
# -Dvendor_ids_dir must be given: when it is empty meson probes six hardcoded
# paths for usb.ids and calls error() if none holds one, so an auto-detect that
# misses ends the configure rather than dropping a feature. hwdata puts it here.
#
# -Dlogind resolves to `systemd` or `libelogind`, and this host has neither, so
# enabled would fail at setup; disabled costs the daemon its inhibitor lock,
# which only delays a reboot during a flash.
#
# -Defi_binary=false: building the capsule loader needs fwupd-efi, which this
# tree does not carry. The uefi-capsule plugin still builds and reports the
# capsule update method unsupported, which is the true answer.
#
# -Dplugin_uefi_capsule_splash rasterises the boot-time splash through python
# gobject-introspection (the `gi` module), pango and cairo at SETUP time and
# error()s when any is missing; no port provides the `gi` module.
#
# -Dintrospection=disabled. Turning it on takes gobject-introspection and
# glib-introspection in depends: the typelib is generated against glib's own
# GIRs, which glib-introspection installs.
#
# -Dopenssl=disabled keeps one crypto backend: gnutls is probed first and the
# embedded jcat verifier takes whichever it finds, so enabling both would link
# a second library nothing calls.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib --localstatedir=/var \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dtests=false \
	-Ddocs=disabled \
	-Dman=true \
	-Dintrospection=disabled \
	-Dsystemd=disabled \
	-Dlogind=disabled \
	-Defi_binary=false \
	-Dplugin_uefi_capsule_splash=false \
	-Dplugin_modem_manager=enabled \
	-Dpassim=disabled \
	-Dp2p_policy=none \
	-Dumockdev_tests=disabled \
	-Dvalgrind=disabled \
	-Dgnutls=enabled \
	-Dopenssl=disabled \
	-Dpolkit=enabled \
	-Dblkid=enabled \
	-Dlibdrm=enabled \
	-Dlibmnl=enabled \
	-Dreadline=enabled \
	-Dbluez=enabled \
	-Dhsi=enabled \
	-Dbash_completion=true \
	-Dfish_completion=false \
	-Dsupported_build=disabled \
	-Dvendor_ids_dir=/usr/share/hwdata
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build
