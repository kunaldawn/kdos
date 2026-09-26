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
# -Defi_binary=true makes the fwupd-efi port a hard dependency: the capsule
# loader fwupdx64.efi is looked for under /usr/libexec/fwupd/efi, and without
# it every capsule method but capsule-on-disk is reported unsupported.
# -Defi_os_dir=kdos puts the loader beside the kernel in EFI/kdos on the ESP
# rather than wherever os-release and a directory probe would guess.
#
# esp-mount-path.patch: fwupd finds the ESP through udisks, which this host
# does not run, so without the patch the uefi-capsule plugin reports no ESP and
# no capsule is ever staged. The patch describes the FAT filesystem mounted at
# EspLocation (set below) from sysfs and the udev database instead — partition
# number, offset, size, UUID and type, which the BootNext entry for the loader
# is built from. A path that is not a FAT mount point is refused, so the live
# image, with no ESP mounted, reports no ESP rather than a wrong one.
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
patch -p1 -i "$PORT_SRC/esp-mount-path.patch"

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib --localstatedir=/var \
	--buildtype=release \
	--wrap-mode=nodownload \
	-Dtests=false \
	-Ddocs=disabled \
	-Dman=true \
	-Dintrospection=disabled \
	-Dsystemd=disabled \
	-Dlogind=disabled \
	-Defi_binary=true \
	-Defi_os_dir=kdos \
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

# EspLocation is where the installer mounts the ESP (its fstab line is
# /boot/efi). It replaces upstream's file, which carries no setting; the mode
# is upstream's, because the file may hold a remote's credentials.
cat > "$PKG/etc/fwupd/fwupd.conf" <<'EOF'
[fwupd]
# use `man 5 fwupd.conf` for documentation
EspLocation=/boot/efi
EOF
chmod 640 "$PKG/etc/fwupd/fwupd.conf"

# THE DAEMON IS D-Bus ACTIVATED, started as root through
# dbus-daemon-launch-helper on the first fwupdmgr call; there is no init
# script and it exits when idle. Its polkit actions are auth_admin with no
# agent here to answer them, and the rule it installs for wheel tests
# `subject.active`, which nothing here ever is, so updating is `sudo fwupdmgr`.
#
# NOTHING ELSE REFRESHES THE METADATA: upstream's fwupd-refresh.timer is a
# systemd unit and is not installed. Without it `fwupdmgr get-updates` compares
# against whatever was last downloaded by hand, which on a new machine is
# nothing. The row runs as root in the system tier, non-interactive because it
# has no terminal, at a minute no other shipped job uses.
install -d "$PKG/etc/kdos/timers.d"
cat > "$PKG/etc/kdos/timers.d/30-fwupd-refresh.timer" <<'EOF'
# The LVFS firmware metadata, so that `sudo fwupdmgr get-updates` has something
# to compare against. Midday, because a missed slot is not run later: -s 20h
# covers a machine asleep through it, not one switched off, and a desktop off
# every night would never reach an overnight slot.
fwupd-refresh  -H 13 -M 43 -s 20h  --  fwupdmgr refresh
EOF
