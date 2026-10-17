# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D KDE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.kreversi.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KReversi
GenericName=Reversi Board Game
Comment=Turn your opponent's pieces over by surrounding them
TryExec=kreversi
Exec=kreversi -qwindowtitle %c
Icon=kreversi
X-DocPath=kreversi/index.html
Categories=Qt;KDE;Game;BoardGame;
Keywords=reversi;othello;board;kreversi;
Terminal=false
StartupWMClass=org.kde.kreversi
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kreversi.desktop"
