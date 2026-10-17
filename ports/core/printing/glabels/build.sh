# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

patch -p1 -i "$PORT_SRC/xml-unzip.patch"

export CFLAGS="$CFLAGS -fcommon -Wno-error=incompatible-pointer-types"

./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--disable-nls \
	--disable-gtk-doc \
	--disable-schemas-compile \
	--without-libebook \
	--without-libbarcode \
	--without-libzint \
	--with-libqrencode \
	--without-libiec16022
make
make DESTDIR=$PKG install

find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

rm -f "$PKG"/usr/share/applications/*.desktop
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/glabels-3.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=gLabels
GenericName=Label Designer
Comment=Design and print labels, business cards and envelopes
Exec=glabels-3 %F
Icon=glabels-3.0
Terminal=false
StartupWMClass=glabels-3
Categories=Office;GTK;
MimeType=application/x-glabels;
Keywords=label;card;envelope;barcode;print;mail merge;glabels;
EOF2
chmod 644 "$PKG/usr/share/applications/glabels-3.desktop"
