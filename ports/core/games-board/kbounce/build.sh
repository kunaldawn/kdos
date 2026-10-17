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

cat > "$PKG/usr/share/applications/org.kde.kbounce.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=KBounce
GenericName=Ball Bouncing Game
Comment=Build walls to fill the field without being hit by a ball
TryExec=kbounce
Exec=kbounce -qwindowtitle %c
Icon=kbounce
X-DocPath=kbounce/index.html
Categories=Qt;KDE;Game;ArcadeGame;
Keywords=bounce;balls;walls;jezzball;arcade;kbounce;
Terminal=false
StartupWMClass=org.kde.kbounce
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.kde.kbounce.desktop"
