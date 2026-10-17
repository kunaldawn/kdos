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

for p in fix-memmove disable_upgrade more-imageprocessor-removes no-empty-glob \
	types-musl python3 add-cast add-const-to-cast add-missing-headers-and-macros \
	add-missing-prototypes add-missing-type-to-variable-decl \
	dont-return-value-from-void-func fix-func-decl-missing-arg use-correct-macro \
	hplip-no-urlopener; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done

GNUMAKEFLAGS="CFLAGS=-std=c++11" save_CFLAGS="$CFLAGS" ./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--with-docdir=/usr/share/doc/hplip \
	--disable-doc-build \
	--disable-gui-build \
	--disable-fax-build \
	--disable-qt3 --disable-qt4 --disable-qt5 \
	--disable-policykit \
	--disable-imageProcessor-build \
	--enable-network-build \
	--enable-scan-build \
	--enable-dbus-build \
	--enable-hpcups-install \
	--disable-class-driver \
	--enable-cups-drv-install \
	--enable-cups-ppd-install
make
make -j1 DESTDIR=$PKG install

rm -rf "$PKG/usr/bin" "$PKG/etc/udev" "$PKG/etc/sane.d" "$PKG/usr/share/hal" \
	"$PKG/usr/lib/systemd" "$PKG/usr/share/applications" "$PKG/etc/xdg" \
	"$PKG/usr/lib/cups/filter/pstotiff" "$PKG/usr/share/doc"
rm -f "$PKG"/usr/share/man/man1/hp-*.1
install -Dm644 /dev/stdin "$PKG/etc/sane.d/dll.d/hpaio" <<'CONF'
hpaio
CONF

if find "$PKG" -name 'libImageProcessor*' -o -name 'hbpl1-*.so' -o -name 'lj-*.so' | grep -q .; then
	echo 'hplip: a prebuilt HP object reached the package' >&2
	exit 1
fi
test -f "$PKG/usr/share/ipp-usb/quirks/HPLIP.conf"
