# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

cd src
# qmake reads the compiler flags from the environment (qt5-qtbase's cflags
# patch); the project has no install rule, so the binary is copied.
/usr/lib/qt5/bin/qmake candle2.pro
make
install -Dm755 Candle2 "$PKG/usr/bin/candle2"
install -Dm644 images/candle_256.png "$PKG/usr/share/icons/hicolor/256x256/apps/candle2.png"

# Qt takes the Wayland app_id from the program name.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/candle2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Candle
GenericName=CNC Controller
Comment=Send G-code to a GRBL CNC machine or laser and watch the toolpath
Exec=candle2
Icon=candle2
Terminal=false
StartupWMClass=candle2
Categories=Qt;Utility;Engineering;
Keywords=cnc;grbl;gcode;g-code;mill;router;laser;sender;
DESKTOP
chmod 644 "$PKG/usr/share/applications/candle2.desktop"
