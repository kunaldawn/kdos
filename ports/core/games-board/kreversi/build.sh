# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# The board is QML inside a widget window, so qtdeclarative is a run-time
# dependency as well as a build one.

# KF_SKIP_PO_PROCESSING leaves the interface catalogues out: bundled data is
# English only. kdoctools_install() builds every translated handbook as well,
# so Help opens a local English copy and the others are removed after the
# install.
cmake -S . -B build -G Ninja \
	-D CMAKE_INSTALL_PREFIX=/usr \
	-D CMAKE_INSTALL_LIBDIR=lib \
	-D CMAKE_BUILD_TYPE=Release \
	-D KDE_INSTALL_USE_QT_SYS_PATHS=ON \
	-D BUILD_TESTING=OFF \
	-D KF_SKIP_PO_PROCESSING=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData sets the desktop
# file name, so the Wayland app_id is org.kde.kreversi. The hicolor PNGs it names
# are upstream's, installed above.
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
