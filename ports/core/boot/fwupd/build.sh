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
rm -rf "$PKG/etc/grub.d"

cat > "$PKG/etc/fwupd/fwupd.conf" <<'EOF'
[fwupd]
# use `man 5 fwupd.conf` for documentation
EspLocation=/boot/efi
EOF
chmod 640 "$PKG/etc/fwupd/fwupd.conf"

install -d "$PKG/etc/kdos/timers.d"
cat > "$PKG/etc/kdos/timers.d/30-fwupd-refresh.timer" <<'EOF'
# The LVFS firmware metadata, so that `sudo fwupdmgr get-updates` has something
# to compare against. Midday, because a missed slot is not run later: -s 20h
# covers a machine asleep through it, not one switched off, and a desktop off
# every night would never reach an overnight slot.
fwupd-refresh  -H 13 -M 43 -s 20h  --  fwupdmgr refresh
EOF
