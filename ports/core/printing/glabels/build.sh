# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# A .glabels file is gzipped XML, and libxml2 decompresses only when the
# parser is asked to; without the patch every saved label fails to open.
patch -p1 -i "$PORT_SRC/xml-unzip.patch"

# -fcommon: two headers define a global rather than declare it, and every
# object that includes one then carries its own copy. Two pointer casts that
# GCC now rejects are warnings again.
export CFLAGS="$CFLAGS -fcommon -Wno-error=incompatible-pointer-types"

# The Zint backend calls ZBarcode_Render, which zint removed in 2.7, so it is
# off. GNU Barcode, libiec16022 and Evolution's address book are not ports;
# QR codes come from qrencode and the rest from the built-in encoders.
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

# English help only.
find "$PKG/usr/share/help" -mindepth 1 -maxdepth 1 ! -name C -exec rm -rf {} +

# No GtkApplication and no set_prgname, so the app_id is the program name,
# glabels-3. The entry replaces upstream's, whose name carries a "3".
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
