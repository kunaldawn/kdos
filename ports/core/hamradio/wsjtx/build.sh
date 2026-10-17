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

export FFLAGS="$FFLAGS -ftrampoline-impl=heap"
cmake -B build -G Ninja \
	-DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -Wno-error" \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_POLICY_DEFAULT_CMP0167=NEW \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DWSJT_GENERATE_DOCS=ON \
	-DWSJT_SKIP_MANPAGES=OFF \
	-DWSJT_BUILD_UTILS=OFF \
	-DUPDATE_TRANSLATIONS=OFF \
	-Wno-dev
ninja -C build
DESTDIR=$PKG ninja -C build install
for m in fst4sim ft8code jt4code jt65code jt9code msk144code qra64code qra64sim; do
	rm -f "$PKG/usr/share/man/man1/$m.1.gz"
done

for t in rigctl rigctld rigctlcom; do
	rm -f "$PKG/usr/bin/$t-wsjtx"
	ln -s "$t" "$PKG/usr/bin/$t-wsjtx"
done

rm -f "$PKG/usr/share/applications/wsjtx.desktop" \
	"$PKG/usr/share/applications/message_aggregator.desktop"
install -Dm644 icons/Unix/wsjtx_icon.png \
	"$PKG/usr/share/icons/hicolor/128x128/apps/wsjtx.png"
cat > "$PKG/usr/share/applications/wsjtx.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=WSJT-X
GenericName=Weak-Signal Digital Modes
Comment=FT8, FT4, JT65, Q65, WSPR and MSK144 for weak-signal amateur radio
Exec=wsjtx
Icon=wsjtx
Terminal=false
StartupWMClass=wsjtx
Categories=Network;HamRadio;
Keywords=ham;radio;ft8;ft4;jt65;wspr;q65;msk144;weak;signal;eme;
DESKTOP
cat > "$PKG/usr/share/applications/message_aggregator.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=WSJT-X Message Aggregator
GenericName=Weak-Signal Decode Monitor
Comment=Gather decodes from several WSJT-X instances over UDP
Exec=message_aggregator
Icon=wsjtx
Terminal=false
StartupWMClass=message_aggregator
Categories=Network;HamRadio;
Keywords=ham;radio;wsjtx;udp;decodes;aggregator;
DESKTOP
chmod 644 "$PKG/usr/share/applications/wsjtx.desktop" \
	"$PKG/usr/share/applications/message_aggregator.desktop"

test -x "$PKG/usr/bin/wsjtx"
test -x "$PKG/usr/bin/jt9"
if readelf -lW "$PKG/usr/bin/jt9" | grep GNU_STACK | grep -q RWE; then
	echo "wsjtx: jt9 asks for an executable stack" >&2
	exit 1
fi
