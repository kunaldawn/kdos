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

# What ships is the print and scan path: hpcups, the hp backend, the PPDs and
# the hpaio SANE backend. The Qt toolbox, fax and the hp-* Python tools are not
# built or are removed below: hp-plugin downloads HP's closed plugin, which
# this image does not carry.
# --disable-imageProcessor-build: libImageProcessor is a prebuilt closed
# object in the tarball; with it on, hpcups links it.
# --disable-network-build: the network half of hpmud needs net-snmp, which is
# not a port. Network printers print through cups' own ipp and socket backends
# with the hpcups PPDs.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--with-docdir=/usr/share/doc/hplip \
	--disable-doc-build \
	--disable-gui-build \
	--disable-fax-build \
	--disable-qt3 --disable-qt4 --disable-qt5 \
	--disable-policykit \
	--disable-imageProcessor-build \
	--disable-network-build \
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

# No prebuilt object from the tarball may reach the package.
if find "$PKG" -name 'libImageProcessor*' -o -name 'hbpl1-*.so' -o -name 'lj-*.so' | grep -q .; then
	echo 'hplip: a prebuilt HP object reached the package' >&2
	exit 1
fi
