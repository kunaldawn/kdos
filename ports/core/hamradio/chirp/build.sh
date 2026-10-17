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

patch -p1 -i "$PORT_SRC/no-reporting.patch"
patch -p1 -i "$PORT_SRC/version.patch"
grep -qx "CHIRP_VERSION = \"next-$version\"" chirp/__init__.py
grep -q "version='$version'," setup.py

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

pip3 install --ignore-installed --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	yattag suds-community .

install -Dm644 chirp/share/chirpw.1 "$PKG/usr/share/man/man1/chirp.1"

install -Dm644 chirp/share/chirp.png "$PKG/usr/share/icons/hicolor/256x256/apps/chirp.png"
for s in 48 64 128; do
	install -d "$PKG/usr/share/icons/hicolor/${s}x${s}/apps"
	rsvg-convert -w $s -h $s chirp/share/chirp.svg \
		-o "$PKG/usr/share/icons/hicolor/${s}x${s}/apps/chirp.png"
done
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/chirp.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=CHIRP
GenericName=Radio Programming Tool
Comment=Read, edit and write the memory channels of hundreds of amateur radios
Exec=chirp --no-install-desktop-app %F
Icon=chirp
Terminal=false
StartupWMClass=Chirp
Categories=Utility;HamRadio;
Keywords=ham;radio;programming;handheld;memory;channels;baofeng;icom;yaesu;kenwood;
DESKTOP
chmod 644 "$PKG/usr/share/applications/chirp.desktop"

test -x "$PKG/usr/bin/chirp"
test -x "$PKG/usr/bin/chirpc"
