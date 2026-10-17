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

cat > "$PKG/usr/share/applications/org.kde.kblocks.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KBlocks
GenericName=Falling Blocks Game
Comment=Fit the falling blocks into full rows
TryExec=kblocks
Exec=kblocks -qwindowtitle %c
Icon=kblocks
X-DocPath=kblocks/index.html
Categories=Qt;KDE;Game;ArcadeGame;
Keywords=tetris;blocks;falling;arcade;kblocks;
Terminal=false
StartupWMClass=org.kde.kblocks
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kblocks.desktop"
