# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The update reminder opens a dialog that asks github.com for a release on
# first start; the patch makes an interval of 0 mean never, and ships 0.
patch -p1 -i "$PORT_SRC/no-update-check.patch"
# bCNC pads its Tk class name with spaces, which no desktop entry can name;
# unpadded, the window's class is the program's and matches bcnc.desktop.
patch -p1 -i "$PORT_SRC/wm-class.patch"

mkdir -p vendor
tar -xf "$PORT_SRC/$name-vendor-$version.tar.xz" --strip-components=1 -C vendor
pip3 install --no-deps --no-index --find-links=vendor --no-build-isolation \
	--root="$PKG" --prefix=/usr svgelements shxparser
pip3 install --no-deps --no-index --no-build-isolation --root="$PKG" --prefix=/usr .

# English only: the other interface catalogues go.
_site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("purelib"))')
find "$PKG$_site/bCNC/locales" -mindepth 1 -maxdepth 1 -type d -exec rm -rf {} +

ln -s bCNC "$PKG/usr/bin/bcnc"
# The only icon is 204 pixels square, which is no hicolor size.
install -d "$PKG/usr/share/icons/hicolor/128x128/apps"
python3 -c 'import sys; from PIL import Image; Image.open(sys.argv[1]).convert("RGBA").resize((128, 128), Image.LANCZOS).save(sys.argv[2])' \
	bCNC/bCNC.png "$PKG/usr/share/icons/hicolor/128x128/apps/bcnc.png"

# Tk is X11: the window runs under Xwayland.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/bcnc.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=bCNC
GenericName=CNC Controller
Comment=Send, edit and probe G-code on a GRBL CNC machine
Exec=bCNC
Icon=bcnc
Terminal=false
StartupWMClass=bCNC
Categories=Science;Electronics;Engineering;
Keywords=cnc;grbl;gcode;g-code;mill;pcb;probe;autolevel;sender;
DESKTOP
chmod 644 "$PKG/usr/share/applications/bcnc.desktop"
