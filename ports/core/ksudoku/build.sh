# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# Roxdoku, the 3-D variant, is compiled in only when both OpenGL and GLU are
# found, and is otherwise dropped without an error. OpenGL is required here;
# GLU has no switch of its own and comes from the glu port in depends.

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
	-D CMAKE_REQUIRE_FIND_PACKAGE_OpenGL=ON \
	-Wno-dev
cmake --build build
DESTDIR=$PKG cmake --install build

find "$PKG/usr/share/doc/HTML" -mindepth 1 -maxdepth 1 ! -name en -exec rm -rf {} +

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: KAboutData sets the desktop
# file name, so the Wayland app_id is org.kde.ksudoku. The hicolor PNGs it names
# are upstream's, installed above.
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
