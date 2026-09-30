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

# CHIRP posts every radio model it opens, with a per-install UUID, to
# chirpmyradio.com, and asks the same server for a newer release; the patch
# turns the reporting module off, which stops both. The repeater queries
# stay, as menu actions the user starts.
patch -p1 -i "$PORT_SRC/no-reporting.patch"

mkdir -p vendor
tar -xf $PORT_SRC/$name-vendor-$version.tar.xz --strip-components=1 -C vendor

# THE CLOSURE IS IN THE BUNDLE, except where a module is already a port:
# pyserial, requests, lark and wxPython come from their python3-* ports. BUILD
# ISOLATION IS OFF, so each sdist builds with setuptools from its port.
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root=$PKG --prefix=/usr \
	yattag suds-community .

install -Dm644 chirp/share/chirpw.1 "$PKG/usr/share/man/man1/chirp.1"

# THE MENU: CHIRP sets GDK_BACKEND=x11 before importing wx, so it runs under
# Xwayland, and GTK's X11 class is the program name capitalised, Chirp. On its
# first run it offers to write ~/.local/share/applications/chirp.desktop,
# which would shadow this entry with one naming ~/.local/bin/chirp;
# --no-install-desktop-app stops the offer. The locale catalogues are not
# compiled: English only.
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
