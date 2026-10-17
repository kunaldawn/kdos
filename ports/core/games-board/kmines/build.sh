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
	-D CMAKE_REQUIRE_FIND_PACKAGE_KF6DocTools=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.kmines.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KMines
GenericName=Minesweeper-like Game
Comment=Clear a minefield without setting any mine off
TryExec=kmines
Exec=kmines -qwindowtitle %c
Icon=kmines
X-DocPath=kmines/index.html
Categories=Qt;KDE;Game;StrategyGame;
Keywords=minesweeper;mines;puzzle;kmines;
Terminal=false
StartupWMClass=org.kde.kmines
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kmines.desktop"
