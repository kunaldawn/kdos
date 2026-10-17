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
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenGL=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

cat > "$PKG/usr/share/applications/org.kde.ksudoku.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KSudoku
GenericName=Sudoku Game
Comment=Sudoku puzzles, from 4x4 to 3-D Roxdoku
TryExec=ksudoku
Exec=ksudoku -qwindowicon ksudoku -qwindowtitle %c
Icon=ksudoku
X-DocPath=ksudoku/index.html
Categories=Qt;KDE;Game;LogicGame;
Keywords=sudoku;puzzle;numbers;roxdoku;ksudoku;
Terminal=false
StartupWMClass=org.kde.ksudoku
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.ksudoku.desktop"
