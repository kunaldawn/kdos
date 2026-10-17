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

export KBUILD_BUILD_TIMESTAMP="$(LC_ALL=C date -u -d "@${SOURCE_DATE_EPOCH:-0}")"
export KBUILD_BUILD_USER=kdos
export KBUILD_BUILD_HOST=kdos
export KBUILD_BUILD_VERSION=1

make mrproper

cp $PORT_SRC/kdos-logo-mono.pbm drivers/video/logo/kdos-logo-mono.pbm

cp $PORT_SRC/kdos.config .config
KCONFIG_WERROR=1 KCONFIG_WARN_UNKNOWN_SYMBOLS=1 make olddefconfig

if grep -E '^(CONFIG_[A-Za-z0-9_]+=|# CONFIG_[A-Za-z0-9_]+ is not set$)' $PORT_SRC/kdos.config | grep -vxFf .config; then
	echo "ERROR: kdos.config sets the lines above and olddefconfig did not keep them" >&2
	exit 1
fi

for opt in CONFIG_RUST CONFIG_DRM_PANIC_SCREEN_QR_CODE CONFIG_DEBUG_INFO_BTF CONFIG_DEBUG_INFO_BTF_MODULES CONFIG_SQUASHFS_ZSTD CONFIG_SQUASHFS_XZ; do
	grep -qx "$opt=y" .config || { echo "ERROR: $opt was dropped by olddefconfig" >&2; exit 1; }
done

modkey=/kdos/build/keys/linux-module-signing.pem
if [ -d /kdos/build ]; then
	if [ ! -s "$modkey" ]; then
		mkdir -p "${modkey%/*}"
		( umask 077; openssl req -new -nodes -utf8 -sha512 -days 36500 -batch -x509 \
			-config $PORT_SRC/x509.genkey -newkey ec -pkeyopt ec_paramgen_curve:secp384r1 \
			-outform PEM -out "$modkey.new" -keyout "$modkey.new" )
		mv "$modkey.new" "$modkey"
	fi
	./scripts/config --set-str CONFIG_MODULE_SIG_KEY "$modkey"
fi

make HOSTCFLAGS="-D__attribute_const__= -D__always_inline=inline" bzImage modules
make INSTALL_MOD_PATH=$PKG INSTALL_MOD_STRIP=1 DEPMOD=/bin/true modules_install

rm -f "$PKG/lib/modules/$version/build"
rm -f "$PKG/lib/modules/$version/source"

mkdir -p $PKG/boot
cp arch/x86/boot/bzImage $PKG/boot/vmlinuz-kdos
cp System.map $PKG/boot/System.map-$version

make INSTALL_HDR_PATH=$PKG/usr headers_install
