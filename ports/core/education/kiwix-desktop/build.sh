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

# A qmake project. Qt 6's qmake reads no compiler flags from the environment,
# so the tree's flags, and with them the reproducibility maps, are passed in.
# The project file appends -Werror after the command line's assignments, so
# -Wno-error goes behind -after or a new compiler warning stops the build.
# Qt TextToSpeech is picked up by qtHaveModule(); without qt6-qtspeech the
# read-aloud bar is compiled out with nothing to say so.
mkdir -p build && cd build
qmake6 .. \
	PREFIX=/usr \
	CONFIG+=release \
	QMAKE_CFLAGS+="$CFLAGS" \
	QMAKE_CXXFLAGS+="$CXXFLAGS" \
	QMAKE_LFLAGS+="$LDFLAGS" \
	-after QMAKE_CXXFLAGS+=-Wno-error
make
make INSTALL_ROOT="$PKG" install
[ -f "$PKG/usr/share/icons/hicolor/48x48/apps/kiwix-desktop.png" ] ||
	{ echo "kiwix-desktop: the hicolor PNG icon was not installed" >&2; exit 1; }

# QGuiApplication::setDesktopFileName makes the Wayland app_id
# org.kiwix.desktop; the entry names it so the panel ties the window to its
# launcher. The catalogue pane downloads over the network and is empty
# offline; ZIM files on the disk open through File > Open or the MIME type.
cat > "$PKG/usr/share/applications/org.kiwix.desktop.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Kiwix
GenericName=Offline Library
Comment=Read ZIM archives such as Wikipedia with no network
Exec=kiwix-desktop %F
Icon=kiwix-desktop
Terminal=false
StartupWMClass=org.kiwix.desktop
MimeType=application/org.kiwix.desktop.x-zim;
Categories=Qt;Education;Reference;
Keywords=zim;wikipedia;offline;encyclopedia;library;kiwix;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kiwix.desktop.desktop"
