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

cat > "$PKG/usr/share/applications/org.kde.katomic.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KAtomic
GenericName=Sokoban-like Logic Game
Comment=Slide atoms around the board to build a molecule
TryExec=katomic
Exec=katomic -qwindowtitle %c
Icon=katomic
X-DocPath=katomic/index.html
Categories=Qt;KDE;Game;StrategyGame;
Keywords=atoms;molecule;puzzle;sokoban;katomic;
Terminal=false
StartupWMClass=org.kde.katomic
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.katomic.desktop"
