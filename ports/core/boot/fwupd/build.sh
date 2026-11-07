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
	-Dlogind=enabled \
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
	-Dlvfs=disabled \
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
