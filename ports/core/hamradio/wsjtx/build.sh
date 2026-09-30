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

# WSJT_BUILD_UTILS=OFF: the simulators and code demonstrators install a dozen
# generic names (cablog, echosim, ft8sim) into /usr/bin. The user guide is the
# asciidoctor HTML; the manual pages are a2x through DocBook. Both are built.
cmake -B build -G Ninja \
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

# Upstream copies hamlib's rigctl, rigctld and rigctlcom in under -wsjtx
# names; links keep the one binary the hamlib port owns.
for t in rigctl rigctld rigctlcom; do
	rm -f "$PKG/usr/bin/$t-wsjtx"
	ln -s "$t" "$PKG/usr/bin/$t-wsjtx"
done

# THE MENU: upstream's entries carry no window class and name the icon under
# pixmaps. No organisation domain is set, so Qt's Wayland app_id is the
# executable's name.
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
